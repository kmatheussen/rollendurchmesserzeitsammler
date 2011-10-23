
#define CPU_MHZ 2667


#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>


/* Global variables */

static int cpu_num = 0;
static double program_start;
static int heap_size;
static int test_num;



/* Statistics */

#define MAX_NUM_SAMPLES (1024*1024*10)
#define NUM_TESTS 38


static double busy_lengths[NUM_TESTS] = {
  0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 
  5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5,
  10.0, 15.0, 20.0, 25.0, 30.0, 35.0, 40.0, 45.0, 
  50.0, 55.0, 60.0, 65.0, 70.0, 75.0, 80.0, 85.0, 90.0, 95.0, 100.0
};


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




/* Threads and timing */

static void bound_thread_to_cpu(int cpu){
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu,&set);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);
}

// This function is copied from the JACK source. http://www.jackaudio.org
typedef int _Atomic_word;
static inline _Atomic_word 
__attribute__ ((__unused__))
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

// Also copied from http://www.jackaudio.org
typedef unsigned long long cycles_t;
static inline cycles_t get_cycles (void){
  unsigned long long ret;
  __asm__ __volatile__("rdtsc" : "=A" (ret));
  return ret;
}

static double get_ms(void){
  return ((double)get_cycles()) / (1000.0*(double)CPU_MHZ);
}

// clear memory manually to make sure the memory is properly allocated
// and in cache.
static void* my_calloc(size_t size1,size_t size2){
  size_t size=size1*size2;
  void* ret=malloc(size);

  if(ret==NULL){
    abort();
  }

  memset(ret,0,size);
  return ret;
}



// Kanskje jeg skulle proeve aa preallokere 8 100MB blokker?


/* The test */

static void *snapshot_thread(void *arg){
  int cpu = atomic_add_get_oldval(&cpu_num,1);

  bound_thread_to_cpu(cpu);

  void *from_mem  = my_calloc(1,heap_size);
  void *to_mem    = my_calloc(1,heap_size);

  if(from_mem==NULL || to_mem==NULL){
    fprintf(stderr,"Out of memory for size %d on cpu %d\n",heap_size,cpu);
    free(from_mem);
    free(to_mem);
    return NULL;
  }


  if(cpu==0)
    set_realtime(SCHED_FIFO,90);
  else
    set_realtime(SCHED_RR,1); // run realtime while synchronizing

  while(get_ms() < program_start); // Synchronize all threads

  if(cpu!=0){
    if(test_num==1)
      set_realtime(SCHED_OTHER,0);
    else
      set_realtime(SCHED_FIFO,90); // Same priority.
  }

  // At least one of the threads must run non-realtime.
  if(cpu==1)
    set_realtime(SCHED_OTHER,0);

  double verystart=get_ms();

  double best_time = 0.0;
  double worst_time = 0.0;

  for(;;){
    double start=get_ms();
    memcpy(from_mem,to_mem,heap_size);
    double time=get_ms()-start;

    if(cpu==0)
      add_sample(time);

    if(best_time==0.0 || time<best_time)
      best_time=time;
    if(time>worst_time)
      worst_time=time;
    if(start-verystart > 1000)
      break;
  }

  sleep(1);

  fprintf(stderr,
          "size: %d, cpu: %d. Best: %f (%f GB/s), Worst: %f (%f GB/s)\n",
          heap_size,cpu,
          (float)best_time,(float) (heap_size / best_time) * (1000.0/(1024.0*1024.0*1024.0)),
          (float)worst_time,(float) (heap_size / worst_time) * (1000.0/(1024.0*1024.0*1024.0)));

  free(from_mem);
  free(to_mem);

  return NULL;
}


void perform_test(){
  int cpu=0;
  int num_cpus;
  cpu_num = 0;

  if(test_num==0)
    num_cpus = 1;
  else
    num_cpus = 4;

  pthread_t threads[num_cpus];

  set_realtime(SCHED_FIFO,90);

  program_start = get_ms() + 2000.0; // Wait two seconds for the threads to synchronize.
  for(cpu=0;cpu<num_cpus;cpu++){
    memset(&threads[cpu],0,sizeof(pthread_t));
    pthread_create(&threads[cpu],NULL,snapshot_thread,NULL);
  }

  set_realtime(SCHED_OTHER,0);

  for(cpu=0;cpu<num_cpus;cpu++){
    pthread_join(threads[cpu],NULL);
  }

  sleep(1);
}

float GB_per_s(double ms){
  return (heap_size / ms) * (1000.0/(1024.0*1024.0*1024.0));
}

int main(int argc, char **argv){
  int i;
  double min,q1,median,q3,max;

  FILE *file = NULL;

  test_num = atoi(argv[1]);

  if(test_num==0)
    file = fopen("/tmp/snapshot_cpu1.stats","w");
  if(test_num==1)
    file = fopen("/tmp/snapshot_cpu4.stats","w");
  if(test_num==2)
    file = fopen("/tmp/snapshot_cpu4rt.stats","w");

  
  fprintf(file,"# 1. size \t 2. min \t 4. q1 \t 6. median \t 8. q3 \t 10. max \n");
  fprintf(file,"# (3,5,7,9 and 11 are GB/s)\n");

  for(i=0;i<NUM_TESTS;i++){
    heap_size=(int)(busy_lengths[i] * 1024.0*1024.0);
    reset_statistics();
    cpu_num = 0;
    perform_test();
    get_statistics(&min,&q1,&median,&q3,&max);
    fprintf(file,"%f\t %f %f\t %f %f\t %f %f\t %f %f\t %f %f\n",
            ((float)(heap_size))/(1024.0*1024.0),
            min,GB_per_s(min),
            q1,GB_per_s(q1),
            median,GB_per_s(median),
            q3,GB_per_s(q3),
            max,GB_per_s(max));           
  }

  fclose(file);
          
  return 0;
}

