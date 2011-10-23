
struct das_poolmem_element{
  struct das_poolmem_element *next;
} typedef poolmem_element_t;

struct poolmem{
  char *mem;
  char *mem_free;
  char *mem_free_end;
  poolmem_element_t **free;
#if ANALYSE_FRAGMENTATION
  int *num_allocs;
#endif
  int max_mem_size;
} typedef poolmem_t;


// Returns actualy size, which is: align_up(size) + sizeof(poolmem_element_pre_t);
#define poolmem_getsize(a) ((poolmem_element_t*)(((char*)a)-sizeof(poolmem_element_pre_t)))->size

ST poolmem_t *poolmem_create(size_t size,void *mem,int max_mem_size);
ST void poolmem_delete(poolmem_t *poolmem);
ST void *poolmem_alloc(poolmem_t *poolmem,size_t size);
ST void poolmem_free(poolmem_t *poolmem,void *start,size_t size);

