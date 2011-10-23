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

#include "config.h"

static size_t heap_size = 50 * 1024 * 1024;

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

#if defined(__i386__)
// Also copied from http://www.jackaudio.org
typedef unsigned long long cycles_t;
static inline cycles_t get_cycles (void){
  unsigned long long ret;
  __asm__ __volatile__("rdtsc" : "=A" (ret));
  return ret;
}    
#elif defined(__x86_64)

/* ia64 */

typedef unsigned long cycles_t;
static inline cycles_t
get_cycles (void)
{
	cycles_t ret;
	__asm__ __volatile__ ("mov %0=ar.itc" : "=r"(ret));
	return ret;
}
#else
#error "not supported"
#endif

static double get_ms(void){
  return ((double)get_cycles()) / (1000.0*(double)CPU_MHZ);
}

static void bound_thread_to_cpu(int cpu){
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu,&set);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);
}


static int set_pthread_priority(pthread_t pthread,int policy,int priority,char *message,char *name){
  struct sched_param par={0};
  par.sched_priority=priority;

  //if((sched_setscheduler(pid,policy,&par)!=0)){
  if ((pthread_setschedparam(pthread, policy, &par)) != 0) {
    fprintf(stderr,message,policy==SCHED_FIFO?"SCHED_FIFO":policy==SCHED_RR?"SCHED_RR":policy==SCHED_OTHER?"SCHED_OTHER":"SCHED_UNKNOWN",priority,getpid(),name,strerror(errno));
    return 0;
  }
  return 1;
}

static void set_realtime(int type, int priority){
  //bound_thread_to_cpu(0);
  set_pthread_priority(pthread_self(),type,priority,"Unable to set %s/%d for %d (\"%s\"). (%s)", "a gc thread");
}

static void *busy_thread(void *arg){
  int cpu = (int)arg;
  bound_thread_to_cpu(cpu);
  fprintf(stderr,"hepp %d\n",cpu);
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

// Called (once) from gc_thread
static long double calculate_snapshot_time(void){
  double  snapshot_time = 0.0;
  void   *from          = my_calloc(1,heap_size);
  void   *to            = my_calloc(1,heap_size);


  //set_realtime(SCHED_RR,10);
  bound_thread_to_cpu(0);
  usleep(1);


  int n;
  int i;

  //set_realtime(SCHED_FIFO,50);


  for(n=0;n<2;n++){
    printf("Test %d/2\n",n+1);
    double verystart=get_ms();
    set_realtime(SCHED_OTHER,0);
    for(i=0;;i++){
      double start=get_ms();
      //fprintf(stderr,"start: %f\n",(float)start);
      memcpy(to,from,heap_size);
      double time=get_ms()-start;
      //fprintf(stderr,"time: %f\n",(float)time);
      if(snapshot_time==0.0 || time<snapshot_time)
        snapshot_time=time;
      double sofar = start-verystart;
      if(sofar > 40000)
        break;    
      //bound_thread_to_cpu((i/64)%3);
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
  printf("This program calculates snapshot time:\n\n");
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

