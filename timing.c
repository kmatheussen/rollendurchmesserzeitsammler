
/* Lots of stuff copied from jack. -Kjetil. */
/* GPL */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#define USE_HPET 0


typedef unsigned long long cycles_t;

#if 0
#if defined(__x86_64__)
typedef long unsigned int jack_time_t;
#else
typedef long long unsigned int jack_time_t;
#endif
#endif

typedef double jack_time_t;


static unsigned int __jack_cpu_mhz = 0;



#ifdef __GNUC__
#  define likely(x)	__builtin_expect((x),1)
#  define unlikely(x)	__builtin_expect((x),0)
#else
#  define likely(x)	(x)
#  define unlikely(x)	(x)
#endif



#if defined(__gnu_linux__) && (defined(__i386__) || defined(__x86_64__))
#define HPET_SUPPORT
#define HPET_MMAP_SIZE			1024
#define HPET_CAPS			0x000
#define HPET_PERIOD			0x004
#define HPET_COUNTER			0x0f0
#define HPET_CAPS_COUNTER_64BIT		(1 << 13)
#if defined(__x86_64__)
typedef uint64_t hpet_counter_t;
#else
typedef uint32_t hpet_counter_t;
#endif
static int hpet_fd;
static unsigned char *hpet_ptr;
static uint32_t hpet_period; /* period length in femto secs */
static uint64_t hpet_offset = 0;
static uint64_t hpet_wrap;
static hpet_counter_t hpet_previous = 0;
#endif /* defined(__gnu_linux__) && (__i386__ || __x86_64__) */

#define jack_error printf

int
jack_hpet_init ()
{
	uint32_t hpet_caps;

	hpet_fd = open("/dev/hpet", O_RDONLY);
	if (hpet_fd < 0) {
	  jack_error ("This system has no accessible HPET device (%s)\n", strerror (errno));
	  return -1;
	}

	hpet_ptr = (unsigned char *) mmap(NULL, HPET_MMAP_SIZE,
					  PROT_READ, MAP_SHARED, hpet_fd, 0);
	if (hpet_ptr == MAP_FAILED) {
	  fprintf(stderr,"This system has no mappable HPET device (%s)\n", strerror (errno));
	  close (hpet_fd);
	  return -1;
	}
	fprintf(stderr,"hpet_ptr: %p\n",hpet_ptr);

	/* this assumes period to be constant. if needed,
	   it can be moved to the clock access function 
	*/
	hpet_period = *((uint32_t *) (hpet_ptr + HPET_PERIOD));
	hpet_caps = *((uint32_t *) (hpet_ptr + HPET_CAPS));
	hpet_wrap = ((hpet_caps & HPET_CAPS_COUNTER_64BIT) &&
		(sizeof(hpet_counter_t) == sizeof(uint64_t))) ?
		0 : ((uint64_t) 1 << 32);

	return 0;
}

static jack_time_t jack_get_microseconds_from_hpet (void) 
{
	hpet_counter_t hpet_counter;
	long double hpet_time;

	hpet_counter = *((hpet_counter_t *) (hpet_ptr + HPET_COUNTER));
	if (unlikely(hpet_counter < hpet_previous))
		hpet_offset += hpet_wrap;
	hpet_previous = hpet_counter;
	hpet_time = (long double) (hpet_offset + hpet_counter) *
		(long double) hpet_period * (long double) 1e-9;
	return ((jack_time_t) (hpet_time + 0.5));
}




// Oops! handterer ikke turn-around. (vel, 194 år...)
static inline cycles_t get_cycles (void)
{
        unsigned long long ret;
        __asm__ __volatile__("rdtsc" : "=A" (ret));
        return ret;
}


jack_time_t jack_get_microseconds_from_cycles (void) {
  return ((double)get_cycles()) / __jack_cpu_mhz;
}

double cycle_to_time(cycles_t cycles){
  return cycles/((double)__jack_cpu_mhz*1000000.0);
}

float cycle_to_microtime(cycles_t cycles){
  return (float)(cycles/((double)__jack_cpu_mhz*1000.0));
}


#if USE_HPET
double get_ms(void){
  jack_get_microseconds_from_hpet() / 1000.0;
}
#else
double get_ms(void){
  return ((double)get_cycles()) / (1000.0*(double)__jack_cpu_mhz);
}
#endif

#if 0
void jack_error(char *ai){
  fprintf(stderr,ai);
}
#endif

/*
 * This is another kludge.  It looks CPU-dependent, but actually it
 * reflects the lack of standards for the Linux kernel formatting of
 * /proc/cpuinfo.
 */

unsigned int jack_get_mhz (void)
{
  FILE *f = fopen("/proc/cpuinfo", "r");
  if (f == 0)
    {
      perror("can't open /proc/cpuinfo\n");
      exit(1);
    }
  
  for ( ; ; )
    {
      unsigned long long mhz;
      int ret;
      char buf[1000];
      
      if (fgets(buf, sizeof(buf), f) == NULL) {
	jack_error ("FATAL: cannot locate cpu MHz in "
		    "/proc/cpuinfo\n");
	exit(1);
      }
      
#if defined(__powerpc__)
      ret = sscanf(buf, "clock\t: %" SCNu64 "MHz", &mhz);
#elif defined( __i386__ ) || defined (__hppa__)  || defined (__ia64__) || defined(__x86_64__)
      ret = sscanf(buf, "cpu MHz         : %" SCNu64, &mhz);
#elif defined( __sparc__ )
      ret = sscanf(buf, "Cpu0Bogo        : %" SCNu64, &mhz);
#elif defined( __mc68000__ )
      ret = sscanf(buf, "Clocking:       %" SCNu64, &mhz);
#elif defined( __s390__  )
      ret = sscanf(buf, "bogomips per cpu: %" SCNu64, &mhz);
#else /* MIPS, ARM, alpha */
      ret = sscanf(buf, "BogoMIPS        : %" SCNu64, &mhz);
#endif
      
      if (ret == 1)
	{
	  fclose(f);
	  return (unsigned int)mhz;
	}
    }
}


static void init_timing ()
{
  static bool is_started=false;
  if(is_started==true)
    return;
  is_started=true;
#if CPU_MHZ
  __jack_cpu_mhz=CPU_MHZ;
#else
  __jack_cpu_mhz = jack_get_mhz ();
#endif
  jack_hpet_init();
  fprintf(stderr,"MHZ: %d\n",(int)__jack_cpu_mhz);
}


void tar_bench_print(tar_heap_t *heap){
  fprintf(stderr,"Max heap usage: %f%%, Max atomic heap usage: %f%% (%d/%d/%d, %d/%d/%d)\n",
          100.0f * (float)(heap->largest_used_mem-GET_RAW_MEM(heap)) / (float)heap_size,
          100.0f * (float)(largest_used_atomic_mem-GET_RAW_ATOMIC_MEM()) / (float)atomic_heap_size,
          (heap->largest_used_mem  - GET_RAW_MEM(heap))    , heap->largest_used_mem_allocated  , heap_size,
          (largest_used_atomic_mem - GET_RAW_ATOMIC_MEM()) , largest_used_atomic_mem_allocated , atomic_heap_size
          );
          
  fprintf(stderr,"Max used non-atomic mem: %d: Max used atomic mem: %d\n",
          heap->max_mem, largest_atomic_mem_allocated);

  benchmark_t *bench=heap->benchmarks;
  while(bench!=NULL){
    fprintf(stderr,"%20s:\t total: %f, \t min: %f, \t average: %f, \t max: %f, num: %d.\n",
	    bench->name,
	    (float)bench->total,
	    (float)bench->min,
	    (float)(bench->total/(double)bench->num),	    
	    (float)bench->max,
	    bench->num
	    );
    bench=bench->next;
  }
  bench=heap->bench_full_snap;
  int i;
  fprintf(stderr,"Snapshot times: ");
  for(i=0;i<bench->num;i++)
    fprintf(stderr,"%f, ",(float)bench->vals[i]);
  fprintf(stderr,"\n");
  bench=heap->bench_full_snap;

  bench=heap->bench_snapshot_length;
  fprintf(stderr,"Snapshot lengths: ");
  for(i=0;i<bench->num;i++)
    fprintf(stderr,"%f, ",(float)bench->vals[i]);
  fprintf(stderr,"\n");
}

static void bench_add_data(benchmark_t *bench,double val){
  bench->total+=val;
  if(bench->num<300)
    bench->vals[bench->num]=val;
  bench->num++;
  if(bench->min==0.0 || val < bench->min)
    bench->min=val;
  if(val > bench->max)
    bench->max=val;
}

static void bench_snap_start(benchmark_t *bench){
  bench->start=get_ms();
}
static void bench_snap_end(benchmark_t *bench){
  double time=get_ms() - bench->start;
  bench_add_data(bench,time);
}


benchmark_t *bench_create(tar_heap_t *heap,char *name){
  benchmark_t *bench=calloc(1,sizeof(benchmark_t));
  bench->name=name;
  bench->next=heap->benchmarks;
  heap->benchmarks=bench;
  return bench;
}

#if BENCHMARK
#   define BENCH_SNAP_START(a) bench_snap_start(a)
#   define BENCH_SNAP_END(a) bench_snap_end(a);
#   define BENCH_ADD_DATA(a,b) bench_add_data(a,b);
#else
#   define BENCH_SNAP_START(a)
#   define BENCH_SNAP_END(a)
#   define BENCH_ADD_DATA(a,b)
#endif


#ifdef TIMINGTEST

#define __USE_GNU
#include <sched.h>

cpu_set_t set;

#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

int audio_priority=60;


#define GC_MAX(a,b) (((a)>(b))?(a):(b))
#define GC_MIN(a,b) (((a)<(b))?(a):(b))

#include "threads.c"


int main(){
  init_timing();
  printf("Remember to run 'cpuspeed -C' first!.\nmzh: %llu\n",__jack_cpu_mhz);

  set_realtime(SCHED_FIFO,99);

  CPU_ZERO(&set);
  CPU_SET(0,&set);
  sched_setaffinity(0, sizeof(cpu_set_t), &set);

  double start=get_ms();
  printf("start-time: %d,%f\n",(int)__jack_cpu_mhz,(float)start);
  usleep(1004500);
  //sleep(1);

#if 0
  CPU_ZERO(&set);
  CPU_SET(1,&set);
  int res=sched_setaffinity(0, sizeof(cpu_set_t), &set);
#else
  int res=0;
#endif
  double end=get_ms();
  printf("end-time: %f, diff: %f\n",(float)end,(float)(end-start));
  printf("(res=%d)\n\n",res);
  return 0;
}
#endif
