
#if ANALYSE_FRAGMENTATION

static double fragm_start_time;
static const int fragm_size=100000;

struct fragm_t{
  float time;

  int upper;
  int allocated;
  float fragmentation;

  int upper_atomic;
  int allocated_atomic;
  float fragmentation_atomic;
} typedef fragm_t;


static void start_fragmentation_analysis(tar_heap_t *heap){
  heap->fragmentation_info = calloc(fragm_size,sizeof(fragm_t));
  fragm_start_time=get_ms();
}

void tar_end_fragmentation_analysis(tar_heap_t *heap){
  int i = 0;

  // x=time, y=memory
  {
    fragm_t *fragms = (fragm_t*)heap->fragmentation_info;
    FILE *file = fopen(FRAGMENTATION_FILENAME,"w");
    
    fprintf(file,"#time\t upper\t allocated\t fragm(%%)\t upper_a\t allocated_a\t fragm_a(%%)\n");
    
    for(i=0;i<heap->fragmentation_info_size;i++){
      fprintf(file,"%0.3f\t %d\t %d\t %0.1f\t \t %d\t %d\t %0.1f\n",
              fragms[i].time,
              fragms[i].upper,
              fragms[i].allocated,
              fragms[i].fragmentation,
              fragms[i].upper_atomic,
              fragms[i].allocated_atomic,
              fragms[i].fragmentation_atomic);
    }
    
    fclose(file);
  }

#if (!USE_TLSF)
  // x=memory block size, y=number of allocations
  {
    FILE *file = fopen(HISTOGRAM_FILENAME,"w");
    fprintf(file,"#num\t memblock size\t number of allocations\n");
    poolmem_write_statistics(heap->mheap,file);
    fclose(file);

    file = fopen(HISTOGRAM_FILENAME "_atomic","w");
    fprintf(file,"#num\t memblock size\t number of allocations\n");
    poolmem_write_statistics(atomic_heap,file);
    fclose(file);
  }
#endif
  
}


static void add_fragmentation_data(tar_heap_t *heap){
  if(heap->fragmentation_info_size == fragm_size)
    return; // full

  fragm_t *fragms  = (fragm_t*)heap->fragmentation_info;
  fragm_t  *fragm  = &(fragms[heap->fragmentation_info_size++]);
  fragm->time      = (get_ms() - fragm_start_time) / 1000.0f;

  // non-atmoic;
  float upper          = GET_CONT_HEAP_SIZE(heap,heap->mheap);
  float used           = heap->allocated_mem;
  fragm->upper         = (int)upper;
  fragm->allocated     = (int)used;
  fragm->fragmentation = 100.0 * (upper-used) / upper;

  // atomic
  upper                       = GET_CONT_ATOMIC_HEAP_END() - GET_RAW_ATOMIC_MEM();
  if(GET_CONT_ATOMIC_HEAP_END()==NULL) // hack
    upper = 0;
  used                        = allocated_atomic_mem;
  fragm->upper_atomic         = (int)upper;
  fragm->allocated_atomic     = (int)used;
  fragm->fragmentation_atomic = 100.0 * (upper-used) / upper;
}


#  define START_FRAGMENTATION_ANALYSIS(heap) start_fragmentation_analysis(heap)
#  define ADD_FRAGMENTATION_DATA(heap) add_fragmentation_data(heap)
#else
#  define START_FRAGMENTATION_ANALYSIS(heap) 
void tar_end_fragmentation_analysis(tar_heap_t *heap){
  return;
}
#  define ADD_FRAGMENTATION_DATA(heap)
#endif

#  define END_FRAGMENTATION_ANALYSIS(heap) tar_end_fragmentation_analysis(heap)
