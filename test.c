
#define RUNNING_TEST 1

#include "rollendurchmesserzeitsammler.c"


#include <stdarg.h>
#include <errno.h>

#include <pthread.h>

static pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;
#define LOCK2() pthread_mutex_lock(&lock2)
#define UNLOCK2() pthread_mutex_unlock(&lock2)



#define DEBUG

#ifdef DEBUG
#  define D(a) a
static void bug(const char *fmt, ...){
  va_list argp;
  va_start(argp,fmt);
  vfprintf(stderr,fmt,argp);
  va_end(argp);
}
#else
#  define D(a)
#endif




int main(){

#if 0
  if(0){
    meminfo_minimal_t das_minimal;
    meminfo_minimal_t *minimal=&das_minimal;

    minimal->size=100;
    MINIMAL_SET_ATOMIC(minimal);
    fprintf(stderr,"minimal->size: %d, size: %d, atomic? %d\n",minimal->size,MINIMAL_GET_SIZE(minimal),MINIMAL_IS_ATOMIC(minimal));
    return 0;
  }
#endif

  //int stack_start;
  tar_heap_t *heap;
  fprintf(stderr,"hello1\n");
  tar_init(1024*1024*10,
	   1024*1024*1,
	   1024*128,
	   60,
	   1.0f
	   );

  {
    tar_heap_t *heaps[10];
    heaps[0]=tar_create_heap();
    heaps[1]=tar_create_heap();
    heaps[2]=tar_create_heap();
    tar_delete_heap(heaps[1],false);
    tar_delete_heap(heaps[0],false);
    tar_delete_heap(heaps[2],false);
  }


  //sleep(50);
  fprintf(stderr,"hello2\n");
  {
    heap=tar_create_heap();
  }
  fprintf(stderr,"hello3\n");
#if 0
  int val=200;
  int val2=__exchange_and_add(&val,-1);
  int val3=atomic_add_get_newval(&val,100);
  // result now: val=299, val2=200, val3=299
  printf("memsize: %d, size: %d %d (%d %d %d)\n",sizeof(tar_mem_t),sizeof(pthread_mutex_t),sizeof(size_t),val,val2,val3); // 24 is too much to put into tar_mem_t.
  char *mem=tar_alloc_atomic(heap,100);
  printf("Alloc something: %p %p %p %p %p\n",mem,tar->atomic_mem,tar->atomic_mem->next,tar->atomic_mem_gc,freelist_nonstatic);
  exit(0);
#endif

#if 0
  void *mem=calloc_ex(2,50,tar->heap);
  D(bug("heapstart: %p, mem: %p\n",tar->heap,mem));
  return 0;
#endif


  //int num_frames=1024;
  int num_extra=0;
  int iterations=0;

  size_t max_used=0;
  size_t max_atomic_used=0;

  //set_snapshot_thread_prio();

  while(1){
    usleep(1000000*0.00005f);
    tar_init_block(0.00005f);
    fprintf(stderr,"hello4\n");

    tar_before_using_heap(heap);

    D(bug("hupp6\n"));
    int frame;
    void **mem;
    //for(frame=0;frame<50*((float)random()/(float)RAND_MAX);frame++){
    for(frame=0;frame<50;frame++){
      //D(bug("num_live: %d\n",num_live));
      //LOCK2();
      mem=tar_alloc(heap,sizeof(void*)*128);
      assert(mem!=NULL);

      mem[0]=tar_alloc_atomic(heap,100);
      assert(mem[0]!=NULL);

      mem[20]=tar_alloc(heap,20);
      assert(mem[20]!=NULL);

      // make some iterior pointers
      mem[20]=((char*)mem[20])+4;
      mem[21]=((char*)mem[20])+4;
      mem[22]=((char*)mem[20])+10;

      // Add some false addresses. (sometimes they are not false though)
      mem[24]=((char*)mem-4);
      mem[25]=((char*)mem-8);
      mem[26]=((char*)mem-12);
      mem[27]=((char*)mem-16);

      //UNLOCK2();
      num_extra+=sizeof(void*)*128;
      //usleep(3);
      /* ... */
    }

    size_t used=tar_get_used_mem(heap);
    size_t atomic_used=tar_get_used_atomic_mem(heap);

    max_used=GC_MAX(used,max_used);
    max_atomic_used=GC_MAX(atomic_used,max_atomic_used);

    //minimal_num_bytes();
    D(bug("GAKK **mems address: %p, mems val: %p, \tdyn mem: %5uk\t(%5uk), \tatomic mem: %5uk\t(%5uk)\n",&mem,0,used/1024,max_used/1024,atomic_used/1024,max_atomic_used/1024));

    //usleep(100000);
    if(tar_after_using_heap(heap)){
      tar_add_root_concurrently(heap,&mem,(&mem)+1);
      //tar_add_set(tar,&globals->pointers_start,&globals->pointers_end);
      D(bug("STARTING GC\n"));
      //usleep(5000);
      tar_start_gc(heap);
    }
    //usleep(1000000);
    //sleep(2);
    //    if(frame<3)
    //  break;
    iterations++;
    if(iterations++>1000)
      break;
  }
  tar_delete_heap(heap,false);

  {
    tar_heap_t *heap[10];
    heap[0]=tar_create_heap();
    heap[1]=tar_create_heap();
    heap[2]=tar_create_heap();
    tar_delete_heap(heap[1],false);
    tar_delete_heap(heap[0],false);
    tar_delete_heap(heap[2],false);
  }

  fprintf(stderr," SUCCESS!\n");
  return 0;
}

