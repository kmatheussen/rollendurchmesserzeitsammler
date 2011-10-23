/*
    Copyright (C) 2008-2009 Kjetil Matheussen
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.
    
    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software 
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

    Note that the license can be changed to a more liberal one on request if
    you have a good reason.
*/


// config.h is only included from here.
#include "config.h"
#if defined(BENCHMARK) || defined(USE_TLSF)
#  define FIND_LARGEST_MEM 1
#else
#  define FIND_LARGEST_MEM 0
#endif


#define _GNU_SOURCE
#define __USE_GNU
#include <sched.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>


// Need ALIGNMENT etc.
#include "gcconfig.h"

#include "rollendurchmesserzeitsammler.h"


#define GC_MAX(a,b) (((a)>(b))?(a):(b))
#define GC_MIN(a,b) (((a)<(b))?(a):(b))


// clear memory manually to make sure the memory is properly allocated
// and in cache.
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

#include "memalloc.h"

#include "atomic.c"
#include "semaphore.c"
#include "mutex.c"
#include "sync.c"
#include "twostacks.c"
#include "pool.c"
#include "hash.c"


struct das_benchmark{
  struct das_benchmark *next;
  char *name;
  double start;
  double total;
  double vals[300];
  int    num;
  double min;
  double max;
}typedef benchmark_t;

struct das_meminfo{
  struct das_meminfo *next;
  hash_elem_t hash_elem;
  char *start;
  char *end;
  bool marked;
  struct das_meminfo *interiors;
  struct das_meminfo *interior_owner;
  bool is_atomic;
}typedef meminfo_t;

struct _tar_heap{
  struct _tar_heap *prev;
  struct _tar_heap *next;
  memtype_t        mheap;
  semaphore_t     *audio_function_can_run;
  void            *audio_function_arg;
  void            *audio_function;
  meminfo_t       *all_meminfos;
  twostacks_t     *twostacks;

  char            *largest_used_mem; // in mheap
  int              largest_used_mem_allocated;
  int              largest_allocated_mem;
  char            *mheap_start;

  int              allocated_mem;
  int              allocated_atomic_mem;
  bool             start_new_gc;
  bool             take_full_snapshot;
  bool             first_full_snapshot_is_taken;
  bool             more_than_half_used;

  stack_t         *writer;
  sync_t          *free_sync;

  hash_t          *hash;

  size_t           max_mem;
  benchmark_t     *benchmarks;
  benchmark_t     *bench_full_snap;
  benchmark_t     *bench_partial_snap;
  benchmark_t     *bench_roots_snap;
  benchmark_t     *bench_prepare_gc;
  benchmark_t     *bench_mark;
  benchmark_t     *bench_sweep;
  benchmark_t     *bench_free;
  benchmark_t     *bench_alloc;
  benchmark_t     *bench_audio;
  benchmark_t     *bench_fragmentation;
  benchmark_t     *bench_atomic_fragmentation;
  benchmark_t     *bench_snapshot_length;
#if ANALYSE_FRAGMENTATION
  int              fragmentation_info_size;
  void            *fragmentation_info;
#endif
};



struct{
  meminfo_t *all_meminfos;
  char      *heap_start;
  char      *heap_end;
  char      *atomic_heap_start;
  char      *atomic_heap_end;

  size_t     snapshot_mem_offset;
  //poolmem_t *poolmem;
  pool_t    *meminfo_pool;
  stack_t   *reader;

  tar_heap_t *heap;
  int num_new_allocations;

  hash_t *hash;
  meminfo_t *false_addresses;
  //address_pool
}typedef mark_variables_t;


/******************************************************/
/******************************************************/
/**************    Global variables             *******/
/******************************************************/
/******************************************************/

/*
heap_size         = 0

roots_start       = NULL
roots_end         = NULL

current_heap      = NULL
snapshot_mem      = NULL
meminfo_pool      = NULL

collector_ready   = SEMAPHORE_INIT(0)

gc_can_run        = false\label{gv:gcr}
*/

static tar_heap_t  *all_heaps        = NULL;

static int          max_mem_size     = 0;
static int          atomic_heap_size = 0;
static int          heap_size        = 0;
static int          half_heap_size   = 0;

static char        *roots_start      = NULL;
static char        *roots_end        = NULL;

static memtype_t    atomic_heap      = NULL;
static char        *largest_used_atomic_mem = NULL;
static int          largest_used_atomic_mem_allocated = 0;
static int          allocated_atomic_mem = 0; // only use if FIND_LARGEST_MEM
static int          largest_atomic_mem_allocated = 0;
static char        *atomic_heap_start = NULL;

static char        *snapshot_mem     = NULL;

static semaphore_t *collector_ready  = NULL;
static semaphore_t *mark_and_sweep_ready = NULL;

static semaphore_t *heap_to_remove    = NULL;
static semaphore_t *heap_removed      = NULL;

static bool         gc_can_run       = false;

static int          audio_priority;

static float        time_between_full_snapshot = 1.0f;
static float        time_since_last_full_snapshot = 0.0f;

// stuff
#include "messagewindow.c"
#include "timing.c"
#include "fragmentation.c"
#include "minimal.c"
#include "threads.c"
#include "roots.c"
#include "mark.c"
#include "sweep.c"
#include "gc_thread.c"



/******************************************************/
/******************************************************/
/**************     Program init                *******/
/******************************************************/
/******************************************************/


/*
gc_init(new_heap_size):
  heap_size    = new_heap_size

  roots_start  = sys_alloc(MAX_ROOTS_SIZE)
  roots_end    = roots_start

  snapshot_mem = sys_alloc(heap_size)
  meminfo_pool = init_pool(sizeof(struct meminfo))

  create_thread(REALTIME_PRIORITY,gc_thread)
*/


void tar_init(int new_atomic_heap_size,
	      int new_heap_size,
	      int das_max_mem_size,
              int audio_priority_setting,
	      float das_time_between_full_snapshot
	      ){

  if(ALIGNMENT<4){
    fprintf(stderr,"Error. Alignment needs to be at least 4 bytes. (%d)\n",ALIGNMENT);
    abort();
  }

  max_mem_size               = das_max_mem_size;
  time_between_full_snapshot = das_time_between_full_snapshot;

  atomic_heap_size     = new_atomic_heap_size;
  heap_size            = new_heap_size;
  half_heap_size       = heap_size / atoi(SNAPSHOT_DIVIDE);
  
  roots_start          = my_calloc(1,MAX_ROOTS_SIZE);
  roots_end            = roots_start;

  snapshot_mem         = my_calloc(1,heap_size);

  atomic_heap          = CREATE_MEMHEAP(atomic_heap_size);
#if USE_TLSF
  atomic_heap_start    = ((char*)atomic_heap) + tlsf_header_size();
#else
  atomic_heap_start    = atomic_heap->mem;
#endif

  collector_ready      = semaphore_create(0);
  mark_and_sweep_ready = semaphore_create(0);

  heap_to_remove       = semaphore_create(0);
  heap_removed         = semaphore_create(0);

  audio_priority       = audio_priority_setting;

  init_snapshot();
  init_timing();
  init_messagewindow();
  init_gc_thread();

  // Start threads.
  init_busylooper  ();
  free_init        ();
  snapshot_creator (gc_thread);
#if PARALLEL_SNAPSHOT
  snapshot_creator (par_snapshot_thread);
#endif
  mark_creator     (mark_and_sweep_thread);

  // Wait for threads to start.
  while(false
	|| semaphore_is_waiting(collector_ready)==false
	|| semaphore_is_waiting(mark_and_sweep_ready)==false
	|| semaphore_is_waiting(run_free_thread)==false)
    usleep(1000000/50);

}




/******************************************************/
/******************************************************/
/**************     Create heap                 *******/
/******************************************************/
/******************************************************/

/*
gc_create_heap(dsp_function):
  heap                      = sys_alloc(sizeof(struct heap))
  heap.start                = create_mheap(heap_size)
  heap.end                  = heap.start + heap_size
  heap.dsp_function_can_run = SEMAPHORE_INIT(1)
  heap.dsp_function         = dsp_function
  heap.all_meminfos         = {}
  heap.ringbuffer           = create_ringbuffer(
                                MAX_RB_ELEMENTS 
                                * sizeof(struct meminfo_minimal)
                              )
  return heap
*/

void *tar_create_heap(void){
  tar_heap_t *heap  = my_calloc(1,sizeof(tar_heap_t));
  //fprintf(stderr,"   CREATING heap %p\n",heap);

  heap->mheap       = CREATE_MEMHEAP(heap_size);
#if USE_TLSF
  heap->mheap_start = ((char*)heap->mheap) + tlsf_header_size();
#else
  heap->mheap_start = heap->mheap->mem;
#endif
  heap->largest_used_mem = GET_RAW_MEM(heap);
  fprintf(stderr,"header size: %d\n",tlsf_header_size());

  heap->audio_function_can_run = semaphore_create(1);
  
  heap->twostacks = twostacks_create(MAX_RB_ELEMENTS * sizeof(meminfo_minimal_t));
  heap->writer    = heap->twostacks->writer;

  heap->free_sync = sync_create();

  heap->hash      = hash_create(393241,NOT_FOUND_POINTER);

  if(all_heaps!=NULL)
    all_heaps->prev = heap;
  heap->next        = all_heaps;
  all_heaps         = heap;

#if BENCHMARK
  heap->bench_full_snap = bench_create(heap,"full snapshot");
  heap->bench_partial_snap = bench_create(heap,"partial snapshot");
  heap->bench_roots_snap = bench_create(heap,"roots snapshot");
  heap->bench_mark = bench_create(heap,"mark");
  heap->bench_sweep = bench_create(heap,"sweep");
  heap->bench_free = bench_create(heap,"free");
  heap->bench_alloc = bench_create(heap,"alloc");
  heap->bench_audio = bench_create(heap,"audio");
  heap->bench_fragmentation = bench_create(heap,"non-atomic fragm.(%)");
  heap->bench_atomic_fragmentation = bench_create(heap,"atomic fragm.(%)");
  heap->bench_snapshot_length = bench_create(heap,"snapshot len.(kb)");
#endif

  START_FRAGMENTATION_ANALYSIS(heap);

  return heap;
}

void tar_touch_heaps(tar_heap_t *heap){
  //bound_thread_to_cpu(SNAPSHOT_CPU1);
  //usleep(50);
  memcpy(snapshot_mem,GET_RAW_MEM(heap),heap_size);
  //memset(snapshot_mem,0,heap_size);

  //  bound_thread_to_cpu(AUDIO_CPU);
  // usleep(50);
  // memset(GET_RAW_MEM(heap->mheap),0,heap_size);
}


static void free_all_meminfos(tar_heap_t *heap,bool audio_is_running){
  meminfo_t *all_interiors=NULL;
  meminfo_t *last_interior=NULL;
  meminfo_t *last;
  meminfo_t *meminfo;
  meminfo_t *prev=NULL;
  meminfo_t *last_atomic=NULL;
  meminfo_t *atomics=NULL;
  
  // Filter out atomic meminfos from heap->all_meminfos and find last_atomic.
  meminfo=heap->all_meminfos;
  while(meminfo!=NULL){
    meminfo_t *next=meminfo->next;
    
    meminfo_t *interior=meminfo->interiors;
    while(interior!=NULL){
      if(last_interior==NULL)
	last_interior=interior;
      interior->next   = all_interiors;
      all_interiors    = interior;
      interior         = interior->interiors;
    }

    if(meminfo->is_atomic==true){
      if(last_atomic==NULL)
	last_atomic=meminfo;
      
      if(prev!=NULL)
	prev->next=next;
      else
	heap->all_meminfos=next;
      meminfo->next=atomics;
      atomics=meminfo;
    }

    prev    = meminfo;
    meminfo = next;
  }

    
  // Free atomic memory and meminfos pointing to atomic objects.
  if(atomics!=NULL)
    free_atomic_meminfo_list(atomics,last_atomic,audio_is_running);
  

  // Find last non-atomic meminfo + interiors.
  last=heap->all_meminfos;
  while(last->next!=NULL){
    meminfo_t *interior=last->interiors;
    
    while(interior!=NULL){
      if(last_interior==NULL)
	last_interior=interior;
      interior->next   = all_interiors;
      all_interiors    = interior;
      interior         = interior->interiors;
    }
    
    last=last->next;
  }


  if(true
     && heap->all_meminfos != NULL
     && all_interiors      != NULL
     )
    {
      // Free both non-atomic meminfos and interior meminfos in one operation.
      meminfo_t *first=heap->all_meminfos;
      last->next = all_interiors;
      last = last_interior;
      free_meminfo_list(first,last,audio_is_running);
    }
  else
    {
      // Free non-atomic meminfos.
      if(heap->all_meminfos != NULL)
	free_meminfo_list(heap->all_meminfos,last,audio_is_running);
      
      // Free interior meminfos.
      if(all_interiors != NULL){
	free_meminfo_list(all_interiors,last_interior,audio_is_running);
      }
    }
}


// delete from double-linked list
static void remove_heap_from_all_heaps(tar_heap_t *heap){
  if(heap->prev==NULL){
    all_heaps        = heap->next;
    if(all_heaps!=NULL){
      all_heaps->prev  = NULL;
      if(all_heaps->next!=NULL)
	all_heaps->next->prev = all_heaps;
    }
  }else{
    heap->prev->next = heap->next;
    if(heap->next!=NULL)
      heap->next->prev = heap->prev;
  }
}


void tar_delete_heap(tar_heap_t *heap,bool audio_is_running){
  END_FRAGMENTATION_ANALYSIS(heap);

  // Remove heap from the all_heaps list.
  if(audio_is_running){
    semaphore_signal_val(heap_to_remove,heap);
    semaphore_wait(heap_removed);
  }else{
    remove_heap_from_all_heaps(heap);
  }


  // Free heap->all_meminfos
  if(heap->all_meminfos!=NULL)
    free_all_meminfos(heap,audio_is_running);


  // Free non-atomic heap, and so on.
  {
    FREE_MEMHEAP(heap->mheap);
    
    semaphore_delete(heap->audio_function_can_run);
    
    twostacks_delete(heap->twostacks);
    
    sync_delete(heap->free_sync);
    
    hash_delete(heap->hash);
    
  }


  free(heap);
}



/******************************************************/
/******************************************************/
/**************     Allocate memory             *******/
/******************************************************/
/******************************************************/

/*
gc_alloc(heap,size):
  heap.alloced_mem += size
  minimal       = arb_write(heap.arb_writer,sizeof(struct meminfo_minimal))
  minimal.start = alloc(heap.start,size)
  minimal.size  = size
  return minimal.start

*/

static void *das_alloc(tar_heap_t *heap, void *start, size_t size, memtype_t mheap, bool is_atomic){
  void              *ret;
  meminfo_minimal_t *minimal = stack_write(heap->writer,sizeof(meminfo_minimal_t));
    
  if(minimal==NULL){
    fprintf(stderr,"Error: heap->writer is empty. It might help to increase MAX_RB_ELEMENTS.\n");
    return NULL;
  }

  ret             =  MALLOC_MEM(mheap,size);
  if(ret          == NULL){
    stack_unwrite(heap->writer,sizeof(meminfo_minimal_t));
    fprintf(stderr,"Rollendurchmesserzeitsammler: Out of %s memory. Returning NULL (%uk/%uk)\n",is_atomic?"atomic":"non-atomic",(unsigned)tar_get_used_mem(heap)/1024,(unsigned)tar_get_used_atomic_mem(heap)/1024);
    return NULL;
  }

  minimal_write(minimal,start,ret,size,is_atomic);

  //fprintf(stderr,"alloc %p, is_atomic: %d, raw: %p\n",ret,is_atomic,GET_RAW_MEM(mheap));
#ifdef DEBUG_DOUBLEFREE
  asdfasdf!!!
  memset(minimal->mem,0,size);
  ((char*)minimal->mem)[16]='u';
#endif

  return ret;
}

void *tar_alloc(tar_heap_t *heap,size_t size){
  if(size==0)
    return NULL;

  BENCH_SNAP_START(heap->bench_alloc);

  size=ALIGN_UP_POINTER(size);
  assert(size>0);

  heap->allocated_mem += size;
  assert(heap->allocated_mem >= 0);

#if BENCHMARK
  if(heap->allocated_mem > heap->max_mem)
    heap->max_mem=heap->allocated_mem;
#endif

  char *ret = das_alloc(heap,GET_RAW_MEM(heap),size,heap->mheap,false);

#if CLEAR_MEMORY_DURING_ALLOCATION
  memset(ret,0,size);
#endif

  BENCH_SNAP_END(heap->bench_alloc);

#if FIND_LARGEST_MEM
  if(ret+size > heap->largest_used_mem){
    heap->largest_used_mem = ret+size;
    heap->largest_used_mem_allocated = heap->allocated_mem;
  }
#endif

#if BENCHMARK
  {
    double top  = GET_CONT_HEAP_SIZE(heap,heap->mheap);
    double used = heap->allocated_mem;
    if(used>1024 && top>1024) // Got 1200% fragmentation with TLSF. Probably when very little memory is used.
      BENCH_ADD_DATA(heap->bench_fragmentation,100.0 * (top-used) / used); // incorrect calculation. Often more than 100% fragmentation, which is impossible.
  }
#endif


  //ADD_FRAGMENTATION_DATA(heap);

  return ret;
}

void *tar_alloc_atomic(tar_heap_t *heap,size_t size){
  if(size==0)
    return NULL;

  BENCH_SNAP_START(heap->bench_alloc);

  size=ALIGN_UP_POINTER(size);
  assert(size>0);

  heap->allocated_atomic_mem += size;
  assert(heap->allocated_atomic_mem >= 0);

  char *ret = das_alloc(heap,GET_RAW_ATOMIC_MEM(),size,atomic_heap,true);

  BENCH_SNAP_END(heap->bench_alloc);


#if FIND_LARGEST_MEM
  allocated_atomic_mem += size;
  if(ret+size > largest_used_atomic_mem){
    largest_used_atomic_mem = ret+size;
    largest_used_atomic_mem_allocated = allocated_atomic_mem;
  }
  if(allocated_atomic_mem > largest_atomic_mem_allocated)
    largest_atomic_mem_allocated = allocated_atomic_mem;
#endif

  //ADD_FRAGMENTATION_DATA(heap);

  return ret;
}


size_t tar_get_used_mem(tar_heap_t *heap){
  return heap->allocated_mem;
}

size_t tar_get_used_atomic_mem(tar_heap_t *heap){
  return heap->allocated_atomic_mem;
}


// Call in case you know more than usual time has been spent.
void tar_dont_gc_now(tar_heap_t *heap){
  heap->start_new_gc=false;
}


/******************************************************/
/******************************************************/
/*******   Decide which heap to run collection on *****/
/******************************************************/
/******************************************************/

static tar_heap_t *get_fullest_heap(void){
  tar_heap_t *heap=all_heaps;
  tar_heap_t *ret=heap;
  float highest = 0.0f;
  while(heap!=NULL){
    float ratio = heap->allocated_mem / heap_size;
    float atomic_ratio = heap->allocated_atomic_mem / atomic_heap_size;
    
    float val = GC_MAX(ratio,atomic_ratio);
    
    if(val > highest){
      ret     = heap;
      highest = val;
    }
    heap = heap->next;
  }
  return ret;
}


// Must be called at each block before using any heaps.
void tar_init_block(float duration){
#if 1
  static bool first_time=true;
  if(first_time==true){
    bound_audio_thread();
    first_time=false;
  }
#endif


  time_since_last_full_snapshot += duration;

  // Remove a deleted heap from list
  {
    if(semaphore_is_signaled(heap_to_remove)){
      tar_heap_t *heap=semaphore_wait(heap_to_remove);
      remove_heap_from_all_heaps(heap);
      semaphore_signal(heap_removed);
    }
  }

  // Release meminfos
  if(semaphore_is_signaled(unfree_meminfos)){
    semaphore_wait(unfree_meminfos);
    pool_append_list(global_mv->meminfo_pool,
		     unfreed_meminfos,
		     unfreed_meminfos_last);
    semaphore_signal(ready_to_unfree_meminfos);
  }

  if(true
     && gc_can_run==true
     && semaphore_is_waiting(collector_ready)
     && semaphore_is_waiting(mark_and_sweep_ready)
     ){
    tar_heap_t *heap=get_fullest_heap();
    if(heap!=NULL)
      heap->start_new_gc=true;
    gc_can_run=false;
  }else{
    gc_can_run=true;
  }

}


/******************************************************/
/******************************************************/
/***********    Before and after audio func  **********/
/******************************************************/
/******************************************************/


/*
run_dsp_block_function(heap):

  //   In case the collector is not finished taking snapshot
  //   since last time, we have to wait for it.
  //   (which in case, there is a good chance we could also
  //    miss the deadline and hear a glitch.)
  wait(heap.dsp_function_can_run)

  //   Run the dsp function
  heap.dsp_function(heap)

  //   In case the collector is ready and the collector did not
  //   run last time, a new collection is started:
  if is_waiting(collector_ready) && gc_can_run==true:
     gc_can_run   = false
     current_heap = heap
     signal(collector_ready)

  else:
     gc_can_run = true
     signal(heap.dsp_function_can_run)
*/

void tar_before_using_heap(tar_heap_t *heap){
  semaphore_wait(heap->audio_function_can_run);
  sync_thread2_lock(heap->free_sync);
  ADD_FRAGMENTATION_DATA(heap); // After freeing
  BENCH_SNAP_START(heap->bench_audio);
}


// If tar_after_using_heap returns true,
// tar_start_gc must be called.
bool tar_after_using_heap(tar_heap_t *heap){
  ADD_FRAGMENTATION_DATA(heap); // Before freeing
  BENCH_SNAP_END(heap->bench_audio);

  sync_thread2_unlock(heap->free_sync);

  if(true
     && heap->more_than_half_used==false
     && GET_CONT_HEAP_SIZE(heap,heap->mheap) > half_heap_size
     )
    {
      messagewindow_show_message("Rollendurchmesserzeitsammler: Warning. More than 1/" SNAPSHOT_DIVIDE " of the heap is used. "
				 "Heap size should be increased. From now on, the program might run out of memory "
				 "without further notice, and the performance will not be predictable.");
      heap->more_than_half_used=true;
    }

  if(false
     || heap->start_new_gc == false
     || (true
         && stack_size(heap->writer) == 0 // Checks if anything has been allocated.
         && time_since_last_full_snapshot < time_between_full_snapshot) // Make sure a new collection is taken at least "once per second".
         // Shouldn't there be a check here if mark_and_sweep_ready is waiting? (No, that check is performed in tar_init_block!)
     )
    {
      heap->start_new_gc = false; // In case it was true. I.e. make sure a gc is not going to run next time.
      semaphore_signal(heap->audio_function_can_run);
     return false;
    }


  return true;
}


// roots must be added before calling tar_start_gc.
void tar_start_gc(tar_heap_t *heap){

  if(ALWAYS_FULL_SNAPSHOT || time_since_last_full_snapshot >= time_between_full_snapshot){
    time_since_last_full_snapshot=0.0f;
    heap->take_full_snapshot=true;
  }else{
    heap->take_full_snapshot=false;
  }

  heap->start_new_gc=false;

  semaphore_signal_val(collector_ready,heap);
}

