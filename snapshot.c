
#include "busylooper.c"

#include "memcpy.c"
//#define my_memcpy memcpy11
#define my_memcpy memcpy
//#define my_memcpy memcpy_sse
//#define my_memcpy memcpy4


static double snapshot_time=0.0;


static void init_snapshot(void){
  long double snapshot_speed;
  FILE *file = fopen(CONFFILE,"r");
  if(file==NULL){
    fprintf(stderr,"%s not found\n",CONFFILE);
    abort();
  }
  fscanf(file,"%Lf",&snapshot_speed);
  fclose(file);


  snapshot_time = ((double)heap_size) / ((1024.0*1024.0)*snapshot_speed);

  fprintf(stderr,"Snapshot speed: %Lf MB. Snapshot time: %fms, Snapshot size: %fMB/ms\n",snapshot_speed,snapshot_time,heap_size/(1024.0*1024.0));
}

//MB/s

/*
take_heap_snapshot(heap):
  \new{if at_least_one_second_since_last_full_copy(current_heap):}
     \new{size = heap_size}
  \new{else:}
     \new{size = heap.mheap.freemem - heap.start}

  for i = 0 \new{size}:
      snapshot_mem[i] = heap.start[i]
*/


#if PARALLEL_SNAPSHOT
semaphore_t *heap_semaphore;
semaphore_t *heap_semaphore_finished;
void *par_from;
void *par_to;
size_t par_size;

static void par_snapshot_thread(void){
  heap_semaphore=semaphore_create(0);
  heap_semaphore_finished=semaphore_create(0);
  set_snapshot_thread_prio(1);
  while(true){
    semaphore_wait(heap_semaphore);
    my_memcpy(par_to,par_from,par_size);
    //my_memcpy(par_to,par_from,512*1024);
    semaphore_signal(heap_semaphore_finished);
  }
}
#endif

// Actually, somewhere between half and full snapshot.
static void take_full_snapshot(tar_heap_t *heap){
  char   *dest      = snapshot_mem;
  char   *src       = (char*)GET_RAW_MEM(heap);
  double  start     = get_ms();
  int     last_size = 0;
  int     so_far    = GET_CONT_HEAP_SIZE(heap,heap->mheap);

  if(half_heap_size > so_far)
    so_far=half_heap_size;

  my_memcpy(dest,src,so_far);

  for(;;){

    double time=get_ms() - start;
    if(false
       || time >= snapshot_time
       || so_far == heap_size
       )
      {
	//fprintf(stderr,"size: %d\n",so_far);
        BENCH_ADD_DATA(heap->bench_snapshot_length,((int)(so_far/1024)));
	return;
      }

    last_size = so_far;
    so_far += SNAPSHOT_COPY_SIZE;
    if(so_far > heap_size)
      so_far = heap_size;

    int size = so_far - last_size;
    my_memcpy(dest+last_size,src+last_size,size);
  }

  BENCH_ADD_DATA(heap->bench_snapshot_length,((int)(so_far/1024)));
}

// Called from gc_thread
#if PARALLEL_SNAPSHOT

#warning PARALLEL_SNAPSHOT has unpredictable execution time
static void take_heap_snapshot(tar_heap_t *heap){
  size_t size =
    heap->take_full_snapshot 
    ? heap_size 
    : GET_CONT_HEAP_SIZE(heap,heap->mheap);


  if(heap->take_full_snapshot){
    int half=size/2;
    char *from=GET_RAW_MEM(heap);
    par_to=snapshot_mem+half;
    par_from=from+half;
    par_size=size-half;
    semaphore_signal(heap_semaphore);
    my_memcpy(snapshot_mem,from,half);
    //my_memcpy(snapshot_mem,from,512*1024);
    semaphore_wait(heap_semaphore_finished);
  }else
    my_memcpy(snapshot_mem,GET_RAW_MEM(heap),size);
}

#else

#if 0
min: 3600.000000,       average: 4152.000000,   max: 4384.000000
min: 3952.000000,       average: 4126.856934,   max: 4272.000000
#endif

static void take_heap_snapshot(tar_heap_t *heap){
  //fprintf(stderr,"starting snapshot\n");
  if(heap->take_full_snapshot){
    //start_all_busy_loop_except(SNAPSHOT_CPU1);
    take_full_snapshot(heap);
    //stop_all_busy_loop_except(SNAPSHOT_CPU1);
  }else
    my_memcpy(snapshot_mem,GET_RAW_MEM(heap),GET_CONT_HEAP_SIZE(heap,heap->mheap));

  //fprintf(stderr,"ending snapshot\n");
}

#endif


