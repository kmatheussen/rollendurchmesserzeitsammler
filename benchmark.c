#include <errno.h>
#include <unistd.h>
#include <rollendurchmesserzeitsammler.c>

tar_heap_t *heap;

int audio_thread(int blocksize){

  int i;
  void *root[blocksize];

  if(tar_entering_audio_thread(heap)==false){
    fprintf(stderr,"Using too much cpu.");
    return -1;
  }

  for(i=0;i<blocksize;i++){
    root[blocksize]=tar_alloc(heap,20);
    // Do various things with root[blocksize];
  }

  if(tar_leave_audio_thread(heap,true)){
    tar_add_root(heap,&root[0],&root[51]);
    // Perhaps want to add stacks, registers, etc. as well here.
    tar_run_gc(heap,true);
  }

  return 0;
}


static void print_error(FILE *where,char *fmt, ...) {
  char temp[10000];
  va_list ap;
  va_start(ap, fmt);{
    vsnprintf (temp, 9998, fmt, ap);
  }va_end(ap);
  //syslog(LOG_INFO,temp);
  fprintf(where,"tar snapshot benchmark: %s\n",temp);
}

static int set_pid_priority(pid_t pid,int policy,int priority,char *message,char *name){
  struct sched_param par={0};
  par.sched_priority=priority;
  if((sched_setscheduler(pid,policy,&par)!=0)){
    print_error(stderr,message,pid,name,strerror(errno));
    return 0;
  }
  return 1;
}

void set_realtime(void){
  set_pid_priority(0,SCHED_FIFO,60,"Unable to set SCHED_FIFO for %d (\"%s\"). (%s)", "the xmessage fork");

}

void* threadstart_realtime(void *arg){
  tar_threadstart threadstart=(tar_threadstart)arg;
  set_realtime();
  threadstart();
  return NULL;
}

void create_realtime_thread(tar_threadstart threadstart){
  pthread_t *thread;
  thread=calloc(sizeof(pthread_t*),1);
  pthread_create(thread,NULL,threadstart_realtime,threadstart);
}

void* threadstart_mark(void *arg){
  tar_threadstart threadstart=(tar_threadstart)arg;
  threadstart();
  return NULL;
}

void create_mark_thread(tar_threadstart threadstart){
  pthread_t *thread;
  thread=calloc(sizeof(pthread_t*),1);
  pthread_create(thread,NULL,threadstart_mark,threadstart);
}


int num_iterations=5000;

int main(){
   set_realtime();
   tar_init(1024*1024, // Size of the atomic heap
             100*1024*1024,  // Size of non-atomic heap
             //tar_find_num_cpus(),
	     2,
	     create_realtime_thread,
	     create_mark_thread
	     );
    heap=tar_new_heap(); // Creates a new heap contaning non-atomic memory.
    {
      int i;
      for(i=0;i<num_iterations;i++){
        int a;
	take_snapshot(heap,&a);
	exit(0);
	usleep(5);
      }
    }
    return 0;
}
