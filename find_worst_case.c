


#define CPU_MHZ 2667


#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>



  /********************/
 /* Global variables */
/********************/
static double program_start;
static int heap_size;
static int test_num;
static int num_disturbance_threads;
static double busy_lengths[NUM_TESTS] = {
  0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 
  5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5,
  10.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 
  50.0, 55.0, 60.0, 65.0, 70.0, 75.0, 80.0, 85.0, 90.0, 95.0, 100.0
};



  /********************/
 /*   Statistics     */
/********************/
#define MAX_NUM_SAMPLES (1024*1024*10)
#define NUM_TESTS 38

static double stats[MAX_NUM_SAMPLES];
static int stat_num=0;

static int double_compare(const void *a, const void *b){
  const double *da = a;
  const double *db = b;
  return *da  > *db; 
}

static void get_statistics(double *min,double *q1,double *median,double *q3,double *max){
  int n=stat_num;
  qsort(stats,n,sizeof(double),double_compare);
  *min    = stats[0];
  *q1     = stats[(int)((n+1)/4)]; //approx enough
  *median = stats[(int)((n+1)/2)]; //approx enough
  *q3     = stats[(int)(3*(n+1)/4)]; //approx enough
  *max    = stats[n-1];
}


static void add_sample(double sample){
  stats[stat_num++]=sample;
}

static void reset_statistics(void){
  memset(stats,0,MAX_NUM_SAMPLES*sizeof(double));
  stat_num=0;
}



  /**********************/
 /* Threads and timing */
/**********************/
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

// Copied from the JACK source. (http://www.jackaudio.org)
typedef int _Atomic_word;
static inline _Atomic_word __attribute__ ((__unused__))
__exchange_and_add(volatile _Atomic_word* __mem, int __val)
{
  register _Atomic_word __result;
  __asm__ __volatile__ ("lock; xaddl %0,%1"
			: "=r" (__result), "=m" (*__mem) 
			: "0" (__val), "m" (*__mem));
  return __result;
}

static inline int atomic_add_get_oldval(int *mem,int inc){
  return __exchange_and_add(mem,inc); // 
}


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


// clear memory manually to make sure the memory is properly
// allocated and in cache.
static void* my_calloc(size_t size1,size_t size2){
  size_t size=size1*size2;
  void* ret=malloc(size);

  if(ret==NULL){
    abort();
  }

  memset(ret,0,size);
  return ret;
}

int between(int start, int end){
  int len = end-start;
  int size = rand()%len;
  return start+size;
}



  /***********************/
 /* Disturbance threads */
/***********************/
int num_disturbance_threads_started = 0;
char *from_mem;
char *to_mem;
int mem_size;
bool run_disturbance=true;
pthread_t threads[200]={0};

static void *disturbance_thread(void *arg){
  int thread_num = atomic_add_get_oldval(&num_disturbance_threads_started,1);
  int cpu = (thread_num%3)+1; // cpu=1,2 or 3. (The test is run on cpu 0.)

  if(test_num==2){
    bound_thread_to_cpu(cpu);
    set_realtime(SCHED_FIFO,60);
    printf("Started disturbance thread on CPU %d\n",cpu);
  }

  if(test_num==1){
    int max_size = 8192*16;
    while(run_disturbance){
      int start = between(1,mem_size-max_size-10);
      int end   = between(start,start+max_size);
      memcpy(to_mem+start,from_mem+start,end-start);
    }
  }else{
    int mem_size=20*1014*1024;
    void *from_mem  = my_calloc(1,mem_size);
    void *to_mem    = my_calloc(1,mem_size);
    while(run_disturbance){
      memcpy(to_mem,from_mem,mem_size);
      usleep(1); // Must sleep a little bit to avoid crashing computer.
    }
  }

  printf("Disturbance thread %d finished\n",cpu);
  return NULL;
}

void start_disturbance_threads(int num){
  int i;

  mem_size  = 80*1024*1024;
  from_mem  = my_calloc(1,mem_size);
  to_mem    = my_calloc(1,mem_size);

  for(i=0;i<num;i++){
    pthread_create(&threads[i],NULL,disturbance_thread,NULL);
  }
  while(num_disturbance_threads_started<num)
    sleep(1);
}

void stop_disturbance_threads(int num){
  int i;
  run_disturbance=false;
  for(i=0;i<num;i++){
    pthread_cancel(threads[i]);
    pthread_join(threads[i],NULL);
  }
}



  /******************************/
 /* Test snapshot performance. */
/******************************/
float MB_per_ms(double ms){
  return (((double)heap_size) / ms) / (1024.0*1024.0);
}

static void *snapshot_thread(void *arg){
  int cpu = 0;
  void *from_mem  = my_calloc(1,heap_size);
  void *to_mem    = my_calloc(1,heap_size);

  bound_thread_to_cpu(cpu);

  if(from_mem==NULL || to_mem==NULL){
    fprintf(stderr,"Out of memory for size %d on cpu %d\n",heap_size,cpu);
    free(from_mem);
    free(to_mem);
    return NULL;
  }

  set_realtime(SCHED_FIFO,60); // jack thread priority
  usleep(20);

  double verystart=get_ms();
  double best_time = 0.0;
  double worst_time = 0.0;

  for(;;){
    double start=get_ms();
    memcpy(to_mem,from_mem,heap_size);
    double time=get_ms()-start;

    add_sample(time);

    if(best_time==0.0 || time<best_time)
      best_time=time;
    if(time>worst_time)
      worst_time=time;
    if(start-verystart > 1000)
      break;

    usleep(10); // This is: 1. Realistic
                //          2. Necessary to avoid stalling the computer.
  }

  fprintf(stderr,
          "size: %d, cpu: %d. Best: %f (%f MB/ms), Worst: %f (%f MB/ms)\n",
          heap_size,cpu,
          (float)best_time,(float) MB_per_ms(best_time),
          (float)worst_time,(float) MB_per_ms(worst_time));

  free(from_mem);
  free(to_mem);

  return NULL;
}

void perform_test(){
  pthread_t thread={0};

  pthread_create(&thread,NULL,snapshot_thread,NULL);
  pthread_join(thread,NULL);

  sleep(1);
}



  /******************************/
 /* Run tests.                 */
/******************************/
int main(int argc, char **argv){
  int i;
  double min,q1,median,q3,max;

  FILE *file = NULL;

  test_num = atoi(argv[1]);

  if(test_num==0)
    file = fopen("/tmp/snapshot_cpu1.stats","w");
  else if(test_num==1){
    file = fopen("/tmp/snapshot_cpu4.stats","w");
    num_disturbance_threads = 40;
  }else if(test_num==2){
    file = fopen("/tmp/snapshot_cpu4rt.stats","w");
    num_disturbance_threads = 3;
  }else{
    file = fopen("/tmp/snapshot_extra.stats","w");
    num_disturbance_threads = 3;
    test_num=2;
  }

  fprintf(file,
          "# 1. size \t 2. min \t 4. q1 \t 6. "
          "median \t 8. q3 \t 10. max \n"
          "# (3,5,7,9 and 11 are MB/ms)\n");

  start_disturbance_threads(num_disturbance_threads);

  for(i=0;i<NUM_TESTS;i++){
    heap_size=(int)(busy_lengths[i] * 1024.0*1024.0);
    reset_statistics();
    perform_test();
    get_statistics(&min,&q1,&median,&q3,&max);
    fprintf(file,"%f\t %f %f\t %f %f\t %f %f\t %f %f\t %f %f\n",
            ((float)(heap_size))/(1024.0*1024.0),
            min,MB_per_ms(min),
            q1,MB_per_ms(q1),
            median,MB_per_ms(median),
            q3,MB_per_ms(q3),
            max,MB_per_ms(max));           
  }

  stop_disturbance_threads(num_disturbance_threads);

  fclose(file);
  return 0;
}
