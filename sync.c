/*
  What:
  - Syncronized locks.

  Problem:
  - Two threads each having a critical section.
  - The two threads can not run inside their critical sections simultaniously.
  - Thread 2 can not be interrupted for a longer period. Thread 1 can though.

  Solution:
  - When thread 1 is inside the critical section, it has to call sync_point_for_thread1
    at regular intervals to check if thread 2 needs to enter its critical section.
    sync_point_for_thread1 will not return until thread 2 leaves its critical section.
  - Thread 1 must have equal or higher priority than thread 2 to avoid priority inversion.
    Hei, vil ikke dette føre til at thread 2 ikke vil kjøre før thread 1 er ferdig? Oops.
    Nja, for multi-core maskiner er ikke det noe problem da.

  Additional features:
  - "Thread 2" locks a shared mutex, so scenarios like the following work fine:
    lock
     lock
      lock
      unlock
     unlock
     lock
     unlock
    unlock
  - all *thread2* functions are threadsafe.
 */



struct{
  volatile bool please_pause;
  volatile bool is_pausing;
  mutex_t lock;
  semaphore_t *go;
}typedef sync_t;




static sync_t *sync_create(void){
  sync_t *sync=calloc(1,sizeof(sync_t));
  sync->please_pause=false;
  sync->is_pausing=false;
  mutex_init(&sync->lock);
  sync->go=semaphore_create(0);
  return sync;
}

static void sync_delete(sync_t *sync){
  mutex_destroy(&sync->lock);
  semaphore_delete(sync->go);
  free(sync);
}


/************************************************/
/************************************************/
/*        Functions for thread 1                */
/************************************************/
/************************************************/

static void sync_thread1_lock(sync_t *sync){
  mutex_lock_exclusive(&sync->lock);
}

static void sync_thread1_unlock(sync_t *sync){
  //sync->is_pausing=false;
  mutex_unlock_exclusive(&sync->lock);
}

// * Only call this from inside a critical section
// * sync_point must be called regularly for thread 2 to run realtime.
static void sync_thread1_pause(sync_t *sync){
  if(sync->please_pause==true){
    sync->is_pausing=true;
    mutex_unlock_exclusive(&sync->lock);
    semaphore_wait(sync->go);
    mutex_lock_exclusive(&sync->lock);
    sync->is_pausing=false;
  }
}



/************************************************/
/************************************************/
/*        Functions for thread 2                */
/************************************************/
/************************************************/

// I'm not sure calling sync_thread2_enter right after calling sync_thread2_leave
// will work...
static void sync_thread2_lock(sync_t *sync){
  sync->please_pause=true;
  mutex_lock_shared(&sync->lock); // Wait for it.
}

// * Returns true in case thread 1 was/is not inside a critical section.
static bool sync_thread2_unlock(sync_t *sync){
  bool was_pausing=sync->is_pausing;

  sync->please_pause=false;

  if(mutex_unlock_shared(&sync->lock))
    if(sync->is_pausing)
      semaphore_signal(sync->go);

  return was_pausing;
}

