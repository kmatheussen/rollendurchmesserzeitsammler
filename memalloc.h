
#define POINTERSIZE (sizeof(void*))

#define ALIGN_SIZE POINTERSIZE
#define MAX_ALLOC_DIVIDE 2

#define ALIGN_UP(value,alignment) (((uintptr_t)value + alignment - 1) & -alignment)

#define ALIGN_UP_ALIGN(p) ALIGN_UP(p,ALIGN_SIZE)
#define ALIGN_UP_POINTER(p) ALIGN_UP(p,POINTERSIZE)



#define GET_RAW_MEM(heap) (heap->mheap_start)
#define GET_RAW_ATOMIC_MEM() (atomic_heap_start)
//#define GET_RAW_MEM(heap) ((char*)(heap->mheap))
//#define GET_RAW_ATOMIC_MEM() ((char*)(atomic_heap))
#define GET_CONT_ATOMIC_HEAP_END()  (largest_used_atomic_mem) // only available when BENCHMARK is 1


#if USE_TLSF




#include <tlsf.h>

typedef void* memtype_t;

static void *CREATE_MEMHEAP(int size){
  void *ret=my_calloc(size,1);
  init_memory_pool(size,ret);
  return ret;
}

#define FREE_MEMHEAP(mheap) free(mheap)

#define FREE_MEM(mheap,start,size) free_ex(start,mheap)

#define CALLOC_MEM(mheap,size1,size2) calloc_ex(size1,size2,mheap)
#define MALLOC_MEM(mheap,size) malloc_ex(size,mheap)

#define GET_CONT_HEAP_SIZE(heap,mheap) (heap->largest_used_mem - ((char*)mheap))
#define GET_CONT_HEAP_END(heap,mheap)  (heap->largest_used_mem)


#else // !USE_TLSF



#define ST static

#include "poolmemalloc.c"

typedef poolmem_t* memtype_t;

#define CREATE_MEMHEAP(size) poolmem_create(size,NULL,max_mem_size)
#define FREE_MEMHEAP(mheap) poolmem_delete(mheap)

#define FREE_MEM(mheap,start,size) poolmem_free(mheap,start,size)

ST void *CALLOC_MEM(memtype_t mheap,size_t size1,size_t size2){
  size_t size=size1*size2;
  void *ret=poolmem_alloc(mheap,size);
  if(ret!=NULL)
    memset(ret,0,size);
  return ret;
}

#define MALLOC_MEM(mheap,size) poolmem_alloc(mheap,size)

//#define GET_RAW_MEM(mheap) ((poolmem_t*)mheap)->mem

#define GET_CONT_HEAP_SIZE(heap,mheap) ( (mheap)->mem_free - (mheap)->mem )
#define GET_CONT_HEAP_END(heap,mheap)  ((char*)(mheap)->mem_free)

#endif // !USE_TLSF
