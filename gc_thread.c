/******************************************************/
/******************************************************/
/*****************    Main gc thread  *****************/
/******************************************************/
/******************************************************/


static meminfo_t *unfreed_meminfos      = NULL;
static meminfo_t *unfreed_meminfos_last = NULL;
static semaphore_t *unfree_meminfos     = NULL;
static semaphore_t *ready_to_unfree_meminfos = NULL;


/*
get_new_meminfos(mv,num_new_allocations):
  for i = 0 to num_new_allocations 
      from = arb_read(
                 mv.arb_reader,
                 sizeof(struct meminfo_minimal)
              )
      to   = get_pool(mv.meminfo_pool)

      to.start     = from.start
      to.end       = from.start+from.size
      to.marked    = false
      to.interiors = {}

      put(to, mv.all_meminfos)
      put_hash(mv.hash_table,to.start,to)
*/

void get_new_meminfos(mark_variables_t *mv,int num_new_allocations){
  int i;
  for(i=0;i<num_new_allocations;i++){
    meminfo_minimal_t *from = stack_read(mv->reader,sizeof(meminfo_minimal_t));
    //meminfo_t         *to   = poolmem_alloc(mv->poolmem,sizeof(meminfo_t));
    meminfo_t         *to   = pool_get(mv->meminfo_pool,false);
    size_t             size;

    //fprintf(stderr,"i/num: %d/%d\n",i,num_new_allocations);
    assert(from != NULL);
    assert(to != NULL);

    to->interiors      = NULL;
    to->interior_owner = NULL;

    to->start = minimal_read(from,mv->heap_start,mv->atomic_heap_start,&size,&to->is_atomic);

    assert(to->start != NULL);
    assert(size > 0);

    //fprintf(stderr,"got new. Size: %d, type: %d\n",size,to->is_atomic);

    to->end      = to->start+size;
    to->marked   = false;

    to->next = mv->all_meminfos;
    mv->all_meminfos = to;

    hash_put(mv->hash,&to->hash_elem,to->start);
  }
}




#include "snapshot.c"



/*
prepare_new_gc(mv, heap):
  mv.all_meminfos         = heap.all_meminfos
  mv.heap_start           = heap.start
  mv.heap_end             = heap.end
  mv.snapshot_heap_offset = snapshot_heap - heap.start

  arbo_switch(current_heap.ringbuffer)
  heap.arb_writer         = heap.ringbuffer.writer
  mv.arb_reader           = heap.ringbuffer.reader
*/


static void prepare_new_gc(mark_variables_t *mv,tar_heap_t *heap){
  mv->all_meminfos        = heap->all_meminfos;

  mv->heap_start          = GET_RAW_MEM(heap);
  mv->heap_end            = GET_CONT_HEAP_END(heap,heap->mheap);

  mv->atomic_heap_start   = GET_RAW_ATOMIC_MEM();
  mv->atomic_heap_end     = GET_CONT_ATOMIC_HEAP_END(); //heap,atomic_heap);

  mv->snapshot_mem_offset = snapshot_mem - mv->heap_start;
  mv->hash                = heap->hash;

  twostacks_switch(heap->twostacks);
  heap->writer            = heap->twostacks->writer;
  mv->reader              = heap->twostacks->reader;  

  mv->num_new_allocations = stack_size(mv->reader) / sizeof(meminfo_minimal_t);
}


/*
gc_thread():
  mv              = sys_alloc(sizeof(struct mark_variables))}
  mv.meminfo_pool = init_pool(sizeof(struct meminfo))}

  while true:
     wait(collector_ready)
       heap = current_heap 
       reset_roots()
       take_root_snapshot(global_vars_start, global_vars_end)
       take_heap_snapshot(heap)
       prepare_new_gc(mv, heap)
       num_new_allocations = arb_space(mv.arb_reader) / sizeof(struct meminfo_minimal)
     signal(heap.dsp_function_can_run)

     if num_new_allocations > 0:
        set_lower_priority()
          get_new_meminfos(mv,num_new_allocations)
          run_mark(mv, root_start, root_end)
          run_sweep()
        set_higher_priority()
*/


#if 0
static char *cacheclearer=NULL;
#endif

static void mark_and_sweep_thread(void){
  set_mark_thread_prio();
  
  while(true){

    ///////////////
    mark_variables_t *mv=semaphore_wait(mark_and_sweep_ready);
#if (!NO_SNAPSHOT)
    BENCH_SNAP_START(mv->heap->bench_mark);
#endif

    ///////////////
    get_new_meminfos(mv,mv->num_new_allocations);


    ///////////////
    run_mark(mv, roots_start, roots_end);
#if (NO_SNAPSHOT)
    int i;
    for(i=0;i<num_roots;i++){
      //fprintf(stderr,"%d: running mark: %p-%p\n",i,root_starts[i],root_ends[i]);
      run_mark(mv, root_starts[i],root_ends[i]);
    }
#endif

    ///////////////
    reset_roots();
#if NO_SNAPSHOT
    BENCH_SNAP_END(mv->heap->bench_partial_snap);
    semaphore_signal(mv->heap->audio_function_can_run);
#else
    BENCH_SNAP_END(mv->heap->bench_mark);
#endif


    ///////////////
    run_sweep(mv);


#if 0
    if(cacheclearer==NULL)
      cacheclearer=malloc(4*1024*1024);
    memcpy(cacheclearer,GET_RAW_MEM(mv->heap->mheap),heap_size);
    memcpy(cacheclearer+heap_size,GET_RAW_MEM(mv->heap->mheap),heap_size);
    memcpy(cacheclearer+(heap_size*2),GET_RAW_MEM(mv->heap->mheap),heap_size);
    memcpy(cacheclearer+(heap_size*3),GET_RAW_MEM(mv->heap->mheap),heap_size);
#endif
  }
}


static mark_variables_t *global_mv;
static void fill_in_mark_variables(mark_variables_t *mv){
  memcpy(mv,global_mv,sizeof(mark_variables_t));
}

static void free_meminfo_list(meminfo_t *first,meminfo_t *last,bool collector_is_running){

  if(collector_is_running){
    semaphore_wait(ready_to_unfree_meminfos);{
      unfreed_meminfos      = first;
      unfreed_meminfos_last = last;
    }semaphore_signal(unfree_meminfos);
  }else{
    pool_append_list(global_mv->meminfo_pool,first,last);
  }
}

static void free_atomic_meminfo_list(meminfo_t *first,meminfo_t *last,bool collector_is_running){

  {
    static mark_variables_t mv;
    fill_in_mark_variables(&mv); // Make a copy of global_mv to avoid false sharing. (don't know how much of a difference it might make though...)
    {
      priority_t *priority=get_priority();
      set_mark_thread_prio();                // To avoid a priority inversion which theoretically could cause premature out-of-memory. (just in case)
      free_meminfos(&mv,first);              // free_meminfos should be thread safe.
      set_priority(priority);
      delete_priority(priority);
    }
  }

  free_meminfo_list(first,last,collector_is_running);
}


static void gc_thread(void){
  mark_variables_t mv={0};
  global_mv=&mv;

  mv.meminfo_pool = pool_create(sizeof(meminfo_t),POOL_SIZE_FACTOR*heap_size); //512*1024);
  //mv.poolmem      = poolmem_create(POOL_SIZE_FACTOR*heap_size,NULL);

  set_snapshot_thread_prio(0);

  while(true){
    tar_heap_t *heap=semaphore_wait(collector_ready);
    assert(semaphore_is_waiting(mark_and_sweep_ready)==true); // What did I think about when writing this...?
    mv.heap = heap;

#if (!NO_SNAPSHOT)
    // First full snapshot sometimes takes significantly longer time to execute.
    //
    // This is a quick hack to take a full snapshot the first time, but not block or
    // run a garbage collection on it. This
    // problem needs a better solution though, but I'm not quite sure why it happens yet.
    //
    // The obvious reason for the first slow-down is because the CPU cache's are cold, but various tries to warm
    // up the various caches, and even tries to cool it to make it constantly cold,
    // doesn't make much difference. So it might not be a CPU cache issue...
    //
    // An interesting observation is that this does not happen with stalinwrapper, only snd/rt-stalin.scm.
    //
    // The problem could be one of: Timing doesn't work, kernel scheduler reschedules and runs
    // something else for a while the first time snapshot is taken, the CPU cache is really cold despite my tests,
    // a bug in snd/rt-stalin.scm, something with cpufreq, memset doesn't always do it's job
    // right away (could explain why my tries to make the cache warm has failed), something else.
    if(heap->take_full_snapshot && heap->first_full_snapshot_is_taken==false){
      reset_roots();
      heap->first_full_snapshot_is_taken=true;
      semaphore_signal(heap->audio_function_can_run);
      take_heap_snapshot(heap);      
      continue;
    }
#endif

#if (!NO_SNAPSHOT)
    BENCH_SNAP_START(heap->bench_roots_snap);{
      take_roots_snapshot(heap);
    }BENCH_SNAP_END(heap->bench_roots_snap);

    BENCH_SNAP_START(heap->take_full_snapshot ? heap->bench_full_snap : heap->bench_partial_snap);{
      take_heap_snapshot(heap);
    }BENCH_SNAP_END(heap->take_full_snapshot?heap->bench_full_snap:heap->bench_partial_snap);
    prepare_new_gc(&mv,heap);
    semaphore_signal(heap->audio_function_can_run);
#else
    prepare_new_gc(&mv,heap);
    BENCH_SNAP_START(heap->bench_partial_snap);
#endif

    if(mv.num_new_allocations>0){
      semaphore_signal_val(mark_and_sweep_ready,&mv);
    }else{
      reset_roots();
#if NO_SNAPSHOT
      BENCH_SNAP_END(heap->bench_partial_snap);
      semaphore_signal(heap->audio_function_can_run);
#endif
    }

  }
}


static void init_gc_thread(void){
  unfree_meminfos          = semaphore_create(0);
  ready_to_unfree_meminfos = semaphore_create(1);
}

