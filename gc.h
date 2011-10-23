
#define GC_malloc_atomic(size) tar_alloc_atomic(heap,size)
//#define GC_malloc_atomic(size) tar_alloc(heap,size)
#define GC_malloc(size) tar_alloc(heap,size)

// This does not seem to be correct! (no! its very wrong! :-)
//#define GC_malloc_uncollectable(size) tar_malloc_atomic_uncollectable(size)

//#define GC_free(mem) tar_free_atomic_uncollectable(mem)

