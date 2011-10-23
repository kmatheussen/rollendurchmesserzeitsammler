

// Haven't succeeded making this work properly.
// Maybe futexes works.


#define BL_BUSY_LENGTH (1024*1024);

static int bl_cpu_num = 0;
static semaphore_t *bl_semaphore[NUM_CPUS];
static int bl_please_stop[NUM_CPUS] = {0};

static void busy_loop_thread(void){
  int cpu = atomic_add_get_oldval(&bl_cpu_num,1);

  bound_thread_to_cpu(cpu_nums[cpu]);
  set_realtime(SCHED_FIFO,audio_priority - 2);

  while(true){
    semaphore_wait(bl_semaphore[cpu]);
    //fprintf(stderr,"starting to busy-loop %d\n",cpu);
    while(bl_please_stop[cpu]==0){
      double start = get_ms();
      while( (get_ms() - start) < 0.01);
      //sched_yield();
    }
    //fprintf(stderr,"stopping busy-loop %d\n",cpu);
  }
}


static void start_busy_loop(int cpu){
  bl_please_stop[cpu] = 0;
  semaphore_signal(bl_semaphore[cpu]);
}

static void stop_busy_loop(int cpu){
  bl_please_stop[cpu] = 1;
}

static void start_all_busy_loop_except(int das_cpu){
  int cpu=0;
  for(cpu=0;cpu<NUM_CPUS;cpu++){
    if(cpu!=das_cpu)
      start_busy_loop(cpu);
  }
}

static void stop_all_busy_loop_except(int das_cpu){
  int cpu=0;
  for(cpu=0;cpu<NUM_CPUS;cpu++){
    if(cpu!=das_cpu)
      stop_busy_loop(cpu);
  }
}

static void init_busylooper(void){
  int cpu=0;
  for(cpu=0;cpu<NUM_CPUS;cpu++){
    bl_semaphore[cpu] = semaphore_create(0);
    snapshot_creator(busy_loop_thread);
  }
}

