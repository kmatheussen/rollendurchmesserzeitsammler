

typedef void (*tar_threadstart)(void);

typedef void (*tar_threadcreator)(tar_threadstart);

static int cpu1=MUTATOR_CPU;
static int cpu2=COLLECTOR_CPU;

#define AUDIO_CPU cpu1
#define SNAPSHOT_CPU1 cpu2 // Needs to be at cpu2 to achieve more consistant performance.
#define SNAPSHOT_CPU2 4 //(!SNAPSHOT_CPU1) (only for parallel snapshot)
#define NONSNAPSHOT_SNAPSHOT_CPU cpu1
#define MARK_CPU cpu2
#define NONSNAPSHOT_MARK_CPU cpu1
#define FREE_CPU cpu1

static int cpu_nums[NUM_CPUS]={0,1,2,3};


static void print_error(FILE *where,char *fmt, ...) {
  char temp[10000];
  va_list ap;
  va_start(ap, fmt);{
    vsnprintf (temp, 9998, fmt, ap);
  }va_end(ap);
  //syslog(LOG_INFO,temp);
  fprintf(where,"librollendurchmesserzeistsammler / threads.c: %s\n",temp);
}

static void bound_thread_to_cpu(int cpu){
#if BOUND_THREADS_TO_ONE_CPU
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu,&set);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set);
  //fprintf(stderr,"pthread_setaffinity_np returned: %d\n",sched_setaffinity(0, sizeof(cpu_set_t), &set));
#endif
}

static void bound_audio_thread(void){
  bound_thread_to_cpu(AUDIO_CPU);
}

static int set_pthread_priority(pthread_t pthread,int policy,int priority,char *message,char *name){
  struct sched_param par={0};
  par.sched_priority=priority;

  //if((sched_setscheduler(pid,policy,&par)!=0)){
  if ((pthread_setschedparam(pthread, policy, &par)) != 0) {
    print_error(stderr,message,policy==SCHED_FIFO?"SCHED_FIFO":policy==SCHED_RR?"SCHED_RR":policy==SCHED_OTHER?"SCHED_OTHER":"SCHED_UNKNOWN",priority,getpid(),name,strerror(errno));
    return 0;
  }
  return 1;
}

static void set_realtime(int type, int priority){
  //bound_thread_to_cpu(0);
  set_pthread_priority(pthread_self(),type,priority,"Unable to set %s/%d for %d (\"%s\"). (%s)", "a gc thread");
}

struct{
  int policy;
  int priority;
} typedef priority_t;

static int get_pid_priority(pid_t pid){
  struct sched_param par;
  sched_getparam(pid,&par);
  return par.sched_priority;
}

static priority_t *get_priority(void){
  priority_t *priority=malloc(sizeof(priority_t));
  priority->policy=sched_getscheduler(0);
  priority->priority=get_pid_priority(0);
  return priority;
}

static  void set_priority(priority_t *priority){
  set_realtime(priority->policy,priority->priority);
}

static void delete_priority(priority_t *priority){
  free(priority);
}

///////////////

static void set_free_thread_prio(void){
  set_realtime(SCHED_FIFO,
               audio_priority >= 2
               ? GC_MAX(sched_get_priority_min(SCHED_FIFO),audio_priority-1)
               : audio_priority);
}

static void* thread_start_free(void *arg){
  tar_threadstart threadstart=(tar_threadstart)arg;
  bound_thread_to_cpu(FREE_CPU);
  threadstart();
  return NULL;
}

static void free_creator(tar_threadstart threadstart){
  pthread_t *thread;
  thread=calloc(sizeof(pthread_t*),1);
  pthread_create(thread,NULL,thread_start_free,threadstart);
}

///////////////

static void set_snapshot_thread_prio(int num){
#if NO_SNAPSNOT
  bound_thread_to_cpu(NONSNAPSHOT_SNAPSHOT_CPU);
#else
  if(num==0)
    bound_thread_to_cpu(SNAPSHOT_CPU1);
  else
    bound_thread_to_cpu(SNAPSHOT_CPU2);
#endif
  set_realtime(SCHED_FIFO,audio_priority);
  //set_realtime(SCHED_FIFO,99);
}

static void* thread_start_snapshot(void *arg){
  tar_threadstart threadstart=(tar_threadstart)arg;
  threadstart();
  return NULL;
}

static void snapshot_creator(tar_threadstart threadstart){
  pthread_t *thread;
  thread=calloc(sizeof(pthread_t*),1);
  pthread_create(thread,NULL,thread_start_snapshot,threadstart);
}

///////////////

static void set_mark_thread_prio(void){
#if NO_SNAPSHOT
  set_snapshot_thread_prio(0);
#else
  set_realtime(SCHED_RR,sched_get_priority_min(SCHED_RR));
#endif
}

static void* thread_start_mark(void *arg){
  tar_threadstart threadstart=(tar_threadstart)arg;
#if (!NO_SNAPSHOT)
  bound_thread_to_cpu(MARK_CPU);
#else
  bound_thread_to_cpu(NONSNAPSHOT_MARK_CPU);
#endif
  threadstart();
  return NULL;
}

static void mark_creator(tar_threadstart threadstart){
  pthread_t *thread;
  thread=calloc(sizeof(pthread_t*),1);
  pthread_create(thread,NULL,thread_start_mark,threadstart);
}
