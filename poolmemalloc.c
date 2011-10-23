
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include <pthread.h>

#include "poolmemalloc.h"

#define CHAR(a) ((char*)a)

#if 0
   static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#  define LOCK() pthread_mutex_lock(&lock)
#  define UNLOCK() pthread_mutex_unlock(&lock)
#else
#  define LOCK() 
#  define UNLOCK() 
#endif


ST poolmem_t *poolmem_create(size_t size,void *mem,int max_mem_size){
  poolmem_t *poolmem=malloc(sizeof(poolmem_t));

  poolmem->mem=mem==NULL?my_calloc(1,size):memset(mem,0,size);
  if(poolmem->mem==NULL){
    free(poolmem);
    fprintf(stderr,"create_poolmem, no memory\n");
    abort();
    return NULL;
  }

  poolmem->mem_free=poolmem->mem;
  poolmem->mem_free_end=poolmem->mem_free + size;
  poolmem->free=my_calloc(sizeof(poolmem_element_t*),max_mem_size/POINTERSIZE);
  if(poolmem->free==NULL){
    fprintf(stderr,"create_poolmem (2), no memory\n");
    abort();
    if(mem==NULL)
      free(poolmem->mem);
    free(poolmem);
    return NULL;
  }

#if ANALYSE_FRAGMENTATION
  poolmem->num_allocs = calloc(sizeof(int),max_mem_size/POINTERSIZE);
#endif

  poolmem->max_mem_size=max_mem_size;
  return poolmem;
}

ST void poolmem_delete(poolmem_t *poolmem){
  free(poolmem->mem);
  free(poolmem->free);
  free(poolmem);
}


#if ANALYSE_FRAGMENTATION
static int poolmem_find_max_size_allocated(poolmem_t *poolmem){
  int max = 0;
  int size;
  for(size=0;size<poolmem->max_mem_size;size+=POINTERSIZE)
    if(poolmem->num_allocs[size/POINTERSIZE]>0)
      max = size;
  return max;
}

static void poolmem_write_statistics(poolmem_t *poolmem,FILE *file){
  int size;
  int max = poolmem_find_max_size_allocated(poolmem);
  int num=0;

  for(size=0;size<=max;size+=POINTERSIZE){
    int num_elements = poolmem->num_allocs[size/POINTERSIZE];
    if(num_elements>0)
      fprintf(file,"%d\t%d\t\t%d\n",num++,size,num_elements);
  }  
}
#endif


// Approx. 12 instructions worst-case
//
// NOTE! size must be aligned up to POINTERSIZE before calling.
//
ST void *poolmem_alloc(poolmem_t *poolmem,size_t size){
  poolmem_element_t *element=NULL;

  assert(size==ALIGN_UP_POINTER(size));


#if ADD_TLSF_HEADER
typedef struct free_ptr_struct {
    void *prev;
    void *next;
} free_ptr_t;
#define MIN_BLOCK_SIZE	(sizeof (free_ptr_t))
#define BLOCK_ALIGN (sizeof(void *) * 2)
#define	MEM_ALIGN		  ((BLOCK_ALIGN) - 1)
#define ROUNDUP_SIZE(_r)          (((_r) + MEM_ALIGN) & ~MEM_ALIGN)
  size = (size < MIN_BLOCK_SIZE) ? MIN_BLOCK_SIZE : ROUNDUP_SIZE(size);
  //  size+=tlsf_mem_header_size();
#endif

  //  if(size<8)
  //  size=8;

  assert(size < poolmem->max_mem_size);


  LOCK();

  {
#if ANALYSE_FRAGMENTATION
    poolmem->num_allocs[size/POINTERSIZE]++;
#endif

    element=poolmem->free[size/POINTERSIZE];

    if(element!=NULL){
      poolmem->free[size/POINTERSIZE] = element->next;
      //fprintf(stderr,"  RHEPP0 %d (size: %d)\n",((char*)element)-((char*)poolmem->mem),size);
      element->next=NULL;
      goto exit;
    }
    
    element = (poolmem_element_t*) poolmem->mem_free;
    poolmem->mem_free += size;
    
    if(poolmem->mem_free > poolmem->mem_free_end){
      poolmem->mem_free -= size;
      element=NULL;
#if USE_SYSTEM_ALLOC_IF_OUT_OF_MEMORY
      element = calloc(1,size);
#endif
      goto exit;
    }
    
    //fprintf(stderr,"   POOLMEM ALLOC %d (size: %d) Element: %p\n",((char*)element)-((char*)poolmem->mem),size,element);
  }

 exit:
  UNLOCK();
#ifdef DEBUG_DOUBLEFREE	  
  if(element!=NULL)
    ((char*)element)[16]='u';
#endif

  return element;
}

// brute force. For testing fragmentation only. Not very effectful.
// Making this operation very light is easy, but requires double
// linked lists for the pools. Considering the tiny difference in
// fragmentation tests, it doesn't seem to be worth it.
// Dissapointing.
static void poolmem_defragment(poolmem_t *poolmem){
  int size;
  for(size=0;size<=4096;size+=POINTERSIZE){
    poolmem_element_t *prev=NULL;
    poolmem_element_t *element=poolmem->free[size/POINTERSIZE];
    while(element!=NULL){
      char *start = (char*)element;
      char *end   = start+size;
      if(end==poolmem->mem_free){
        //fprintf(stderr,"found %d\n",size);
        poolmem->mem_free = start;
        if(prev==NULL)
          poolmem->free[size/POINTERSIZE] = element->next;
        else
          prev->next = element->next;
        element->next = NULL;
        poolmem_defragment(poolmem);
        return;
      }
      prev    = element;
      element = element->next;
    }
  }
}

// Approx max 8 instructions.
//
ST void poolmem_free(poolmem_t *poolmem,void *v_start,size_t size){
#if ADD_TLSF_HEADER
typedef struct free_ptr_struct {
    void *prev;
    void *next;
} free_ptr_t;
#define MIN_BLOCK_SIZE	(sizeof (free_ptr_t))
#define BLOCK_ALIGN (sizeof(void *) * 2)
#define	MEM_ALIGN		  ((BLOCK_ALIGN) - 1)
#define ROUNDUP_SIZE(_r)          (((_r) + MEM_ALIGN) & ~MEM_ALIGN)
  size = (size < MIN_BLOCK_SIZE) ? MIN_BLOCK_SIZE : ROUNDUP_SIZE(size);
  //  size+=tlsf_mem_header_size();
#endif
  //if(size<8)
  //  size=8;


  //fprintf(stderr,"   POOLMEM FREE. Size: %d, pos: %p\n",size,start);
#ifdef DEBUG_DOUBLEFREE
  //fprintf(stderr,"start[16]: -%c- (%d)\n",start[16],(int)start[16]);
  assert(start[16]!='a');
  assert(start[16]=='u');
  start[16]='a';
#endif

  LOCK();

#if (!DONT_REDUCE_FREE_STACK)
  char *start = v_start;
  char *end   = start+size;
  if(end==poolmem->mem_free){
    poolmem->mem_free=start;
#  if USE_DEFRAGMENTER
    poolmem_defragment(poolmem);
#  endif
  }else
#endif
    {
      poolmem_element_t *element=v_start;
      element->next=poolmem->free[size/POINTERSIZE];
      poolmem->free[size/POINTERSIZE]=element;
    }
  
  UNLOCK();
}

#ifdef TESTING
int main(){
  poolmem_t *pm=create_poolmem(1024*104,NULL);
  void *ai=poolmem_alloc(pm,127);
  poolmem_free(pm,ai);
  void *ai2=poolmem_alloc(pm,126);
  printf("size: %d %p %p %p, size:%d\n",sizeof(poolmem_element_pre_t),pm->mem,ai,ai2,poolmem_getsize(ai2));
  return 0;
}
#endif
