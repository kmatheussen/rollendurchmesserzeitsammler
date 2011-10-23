/******************************************************/
/******************************************************/
/***********             Sweep               **********/
/******************************************************/
/******************************************************/


/*
run_sweep_free      = SEMAPHORE_INIT(0)
sweep_mem           = NULL
sweep_mem_size      = 0
*/

static semaphore_t *free_thread_is_ready;
static semaphore_t *run_free_thread;

static meminfo_minimal_t *sweep_mem;
static int sweep_mem_size = 0;



/*
free_thread():
  while true:
    heap = wait(run_sweep_free)

    slock1_lock(heap.sync_lock)

      for i=0 to sweep_mem_size:
          minimal = sweep_mem[i]
          mem     = minimal.mem
          size    = minimal.size
          for i1=0 to size step 64:
              slock1_pause(heap.sync_lock)
              for i2=i1 to min(size,i1+64):
                  mem[i2]=0
          free(heap.mheap,mem,size)
          // Inside critical section, don't need atomic instruction:
          current_heap.alloced_mem -= size

    slock1_unlock(heap.sync_lock)
*/

static void free_thread_clear_memblock(tar_heap_t *heap, char *mem, size_t size){
  int              i2;
  const int        inc       = POINTERSIZE*128;
  for(i2=0 ; i2<size ; i2+=inc){
    int free_size;
    BENCH_SNAP_END(heap->bench_free);
    sync_thread1_pause(heap->free_sync);
    BENCH_SNAP_START(heap->bench_free);
    free_size = GC_MIN(size-i2,inc);
#ifndef DEBUG_DOUBLEFREE
    memset(mem+i2,0,free_size);
#endif
  }
}

static void free_thread(){
  tar_heap_t *heap;

  set_free_thread_prio();

  while(true){

    semaphore_signal(free_thread_is_ready);
    heap = semaphore_wait(run_free_thread);

    void *heap_start          = GET_RAW_MEM(heap);
    void *local_atomic_heap_start   = GET_RAW_ATOMIC_MEM();

    sync_thread1_lock(heap->free_sync);{
      BENCH_SNAP_START(heap->bench_free);
      int i;
      for(i=0;i<sweep_mem_size;i++){
        meminfo_minimal_t *minimal   = &sweep_mem[i];
        char              *mem;
        size_t             size;
	bool               is_atomic;

        mem = minimal_read(minimal, heap_start, local_atomic_heap_start, &size, &is_atomic);

        //fprintf(stderr,"Freeing. %p Size: %d, type: %d %p\n",mem,size,is_atomic,GET_RAW_ATOMIC_MEM());

	if(is_atomic){

	  // Maybe the atomic heap always should be TLSF...
	  free_thread_clear_memblock(heap,mem,size);

	  FREE_MEM(atomic_heap, mem, size);

	  heap->allocated_atomic_mem -= size;
	  assert(heap->allocated_atomic_mem >= 0);

#if FIND_LARGEST_MEM
          allocated_atomic_mem -= size;
#endif

	}else{
#         if CLEAR_MEMORY_DURING_ALLOCATION
 	    BENCH_SNAP_END(heap->bench_free);
	    sync_thread1_pause(heap->free_sync);
	    BENCH_SNAP_START(heap->bench_free);
#         else
	    free_thread_clear_memblock(heap,mem,size);
#         endif
            FREE_MEM(heap->mheap, mem, size);
	  heap->allocated_mem -= size;
	  assert(heap->allocated_mem >= 0);
	}

      }
      BENCH_SNAP_END(heap->bench_free);
    }sync_thread1_unlock(heap->free_sync);

  }
}




static void free_init(void){
  free_thread_is_ready = semaphore_create(0);
  run_free_thread = semaphore_create(0);
  sweep_mem       = my_calloc(sizeof(meminfo_minimal_t),MAX_FREE_ELEMENTS);
  free_creator(free_thread);
}



/*
run_sweep():
  survived_mem={}
  foreach mem in current_heap.all_meminfos:
    if mem.marked==true:
       put(mem,survived_mem)
       mem.marked=false
    else:
       size = mem.end-mem.start
       for i=0 to size:
           mem.start[i]=0
       free(current_heap.start, mem.start, mem.end)
       atomic_inc(current_heap.alloced_mem, -size)
       put_pool(meminfo_pool,mem)
  current_heap.all_meminfos=survived_mem
*/

#if 0
static int times_in_list(meminfo_t *list,meminfo_t *mem){
  int ret=0;
  while(list!=NULL){
    if(list==mem)
      ret++;
    list=list->next;
  }
  return ret;
}
#endif


// Should be thread safe.
static void free_meminfos(mark_variables_t *mv, meminfo_t *mem){
  meminfo_minimal_t *sm = sweep_mem;
  int num_elements = 0;

  semaphore_wait(free_thread_is_ready);

  while(mem!=NULL){

    if(num_elements==MAX_FREE_ELEMENTS){
      sweep_mem_size = num_elements;
      semaphore_signal_val(run_free_thread, mv->heap);
      semaphore_wait(free_thread_is_ready);
      num_elements = 0;
    }

    size_t size = mem->end - mem->start;
    meminfo_minimal_t *minimal=&sm[num_elements++];
    
    assert(size>0);
    assert(mem->is_atomic==true  || mem->start >= mv->heap_start);
    assert(mem->is_atomic==true  || mem->end   <= mv->heap_end);
    assert(mem->is_atomic==false || mem->start >= mv->atomic_heap_start);
    assert(mem->is_atomic==false || mem->end   <= mv->atomic_heap_end);
    
    minimal_write(minimal,
		  mem->is_atomic ? mv->atomic_heap_start : mv->heap_start,
		  mem->start,
		  size,
		  mem->is_atomic);
    
    mem = mem->next;
  }

  sweep_mem_size = num_elements;
  semaphore_signal_val(run_free_thread,mv->heap);  
}



// Should/must be optimized in various ways. (why, how? (maybe i have already optimized it...))

static void run_sweep(mark_variables_t *mv){
  meminfo_t  *free_mem = NULL;
  meminfo_t  *prev     = NULL;
  tar_heap_t *heap     = mv->heap;
  meminfo_t  *mem      = mv->all_meminfos;


  BENCH_SNAP_START(heap->bench_sweep);

  // Sweep
  {
    while(mem!=NULL){
      meminfo_t *next = mem->next;
      
      if(mem->marked==true){
	mem->marked = false;
	prev = mem;
      }else{
	mem->next = free_mem;
	free_mem  = mem;
	if(prev==NULL){
	  mv->all_meminfos = next;
	}else{
	  prev->next = next;
	}
      }
      
      mem = next;
    }
    heap->all_meminfos = mv->all_meminfos;
  }

  // Free
  {
    BENCH_SNAP_END(heap->bench_sweep); // Not quite accurate, but sweep performance is not very important to benchmark.
    free_meminfos(mv,free_mem);
    BENCH_SNAP_START(heap->bench_sweep);
  }

  // Return hash elements + free interior pointers
  {
    mem = free_mem;
    while(mem!=NULL){
      meminfo_t *next=mem->next;      
      meminfo_t *interior=mem->interiors;
      hash_remove(mv->hash,mem->hash_elem.key);

      // free interior pointers.
      while(interior!=NULL){
        meminfo_t *next=interior->next;
        hash_remove(mv->hash,interior->hash_elem.key);
        //poolmem_free(mv->poolmem,interior,sizeof(meminfo_t));
        pool_put(mv->meminfo_pool,interior);
        interior=next;
      }

      pool_put(mv->meminfo_pool,mem);
      //poolmem_free(mv->poolmem,mem,sizeof(meminfo_t));
      mem=next;
    }
  }


  // Clear false addresses
  {
    mem = mv->false_addresses;
    while(mem!=NULL){
      meminfo_t *next=mem->next;
      hash_remove(mv->hash,mem->hash_elem.key);
      //poolmem_free(mv->poolmem,mem,sizeof(meminfo_t));
      pool_put(mv->meminfo_pool,mem);
      mem=next;
    }
    mv->false_addresses = NULL;
  }

  BENCH_SNAP_END(heap->bench_sweep);
}

