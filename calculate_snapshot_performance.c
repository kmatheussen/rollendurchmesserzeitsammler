#define NUM_CPUS 4
#define CPU_MHZ 2667

#define _GNU_SOURCE
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>
#include <stdbool.h>



static size_t heap_size = 50 * 1024 * 1024;


  /**********************/
 /* Threads and timing */
/**********************/
#if defined(__i386__)
// Also copied from the JACK source
typedef unsigned long long cycles_t;
static inline cycles_t get_cycles (void){
  unsigned long long ret;
  __asm__ __volatile__("rdtsc" : "=A" (ret));
  return ret;
}    
#endif

static double get_ms(void){
#if defined(__x86_64)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ((double)ts.tv_nsec) / 1000000.0;
#else
  return ((double)get_cycles()) / (1000.0*(double)CPU_MHZ);
#endif
}

static void bound_thread_to_cpu(int cpu){
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu,&set);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);
}

static int set_pthread_priority(pthread_t pthread,int policy,int priority){
  struct sched_param par={0};
  par.sched_priority=priority;

  if ((pthread_setschedparam(pthread, policy, &par)) != 0) {
    abort();
    return 0;
  }
  return 1;
}

static void set_realtime(int type, int priority){
  set_pthread_priority(pthread_self(),type,priority);
}



  /***********************/
 /* buzy threads.       */
/***********************/
// These threads prevents others from
// using the memory bus
static void *busy_thread(void *arg){
  int cpu = (int)arg;
  bound_thread_to_cpu(cpu);
  set_realtime(SCHED_FIFO,1);
  while(true){
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
    __asm__ __volatile__("nop");
  }
  return NULL;
}

void start_busy_threads(void){
  static bool started=false;
  int cpu;
  if(started==false){
    for(cpu=1;cpu<NUM_CPUS;cpu++){
      pthread_t *thread=calloc(sizeof(pthread_t),1);
      pthread_create(thread,NULL,busy_thread,(void*)cpu);
    }  
    started=true;
  }
}

// clear memory manually to make sure the memory is properly
// allocated and in cache.
static void* my_calloc(size_t size1,size_t size2){
  size_t size=size1*size2;
  void* ret=malloc(size);

  if(ret==NULL){
    fprintf(stderr,"my_calloc: malloc returned NULL.");
    abort();
  }

  memset(ret,0,size);
  return ret;
}







  /***********************/
 /* Find p(M)           */
/***********************/
// Called (once) from gc_thread
static long double calculate_snapshot_time(void){
  double  snapshot_time = 0.0;
  void   *from          = my_calloc(1,heap_size);
  void   *to            = my_calloc(1,heap_size);

  bound_thread_to_cpu(0);
  usleep(1);

  int n;
  int i;

  for(n=0;n<2;n++){
    printf("Test %d/2\n",n+1);
    double verystart=get_ms();
    set_realtime(SCHED_OTHER,0);
    for(i=0;;i++){
      double start=get_ms();
      memcpy(to,from,heap_size);
      double time=get_ms()-start;
      if(snapshot_time==0.0 || time<snapshot_time)
        snapshot_time=time;
      double sofar = start-verystart;
      if(sofar > 40000)
        break;    
      if(sofar > 10000 && sofar < 20000)
        set_realtime(SCHED_RR,2);
      else if(sofar > 20000 && sofar < 30000)
        set_realtime(SCHED_FIFO,10);
      else if(sofar > 30000)
        set_realtime(SCHED_FIFO,70);
      usleep((int)time);
    }
    start_busy_threads();
  }

  printf("snapshot time: %fms (%f MB/ms). Size: %d\n",
         (float)snapshot_time,
         (float) (heap_size / snapshot_time) / (1024.0*1024.0),
         heap_size
         );

  set_realtime(SCHED_OTHER,0);
  return ((long double)heap_size) / (((long double)snapshot_time) * 1024.0 * 1024.0);
}

int main(int argc, char **argv){
  printf("\n\n******************************\n");
  printf("This program calculates snapshot performance:\n\n");
  printf("  1.   Make sure you are root. \n");
  printf("  2.   No other programs should run while running this program.\n");
  printf("  3.   Snapshot performance is stored in %s. (MB/ms)\n",argv[1]);
  printf("******************************\n\n");
  long double result = calculate_snapshot_time();
  FILE *file = fopen(argv[1],"w");
  fprintf(file,"%Lf\n",result);
  fclose(file);
  return 0;
}
