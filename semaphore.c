
#include <semaphore.h>

/* Extended posix semaphores. Only one thread for signalling and one
   thread for waiting. It might work with more than two threads, but
   I haven't given it any thought.

   Advantages:
   * A void* object can be sent when signaled, and recieved when waiting.
   * semaphore_is_waiting should be working properly.
*/


struct{
  sem_t sem;
  bool is_waiting;
  void *val;
  bool need_ack;
  sem_t ack;
}typedef semaphore_t;



static semaphore_t *semaphore_create(int init_val){
  //int init_val=can_run?1:0;
  semaphore_t *sem=my_calloc(1,sizeof(semaphore_t));
  sem_init(&sem->sem,0,init_val);
  sem_init(&sem->ack,0,init_val);

  return sem;
}

static void semaphore_delete(semaphore_t *sem){
  sem_destroy(&sem->sem);
  sem_destroy(&sem->ack);
  free(sem);
}

static bool semaphore_is_waiting(semaphore_t *sem){
  int semval;

  return sem->is_waiting;

  if(sem->is_waiting==false)
    return false;

  sem_getvalue(&sem->sem,&semval);

  if(semval>0)
    return false;

  return true;
}


static void *semaphore_wait(semaphore_t *sem){
  /*
  int semval;
  sem_getvalue(&sem->sem,&semval);
  fprintf(stderr,"val: %d %p\n",semval,sem);
  */

  sem->is_waiting=true;

  while(sem_wait(&sem->sem)==-1);
  // In this position, both is->waiting is true, and sem_getvalue is 0.
  // Therefore, it might be necessary to call semaphore_signal_ack to
  // make sure semaphore_is_waiting always works correctly.
  //   Or, just set is_waiting to false when calling signal.
  //sem->is_waiting=false;

  if(sem->need_ack==true){
    sem->need_ack=false;
    sem_post(&sem->ack);
  }

  //sem_getvalue(&sem->sem,&semval);
  //fprintf(stderr,"val now: %d %p, waitret: %d strerror(%s)\n",semval,sem,waitret,strerror(errno));
  return sem->val;
}

static bool semaphore_is_signaled(semaphore_t *sem){
  int semval;

  sem_getvalue(&sem->sem,&semval);
  if(semval>0)
    return true;
  return false;
}

static void semaphore_signal(semaphore_t *sem){
  sem->is_waiting=false;
  sem_post(&sem->sem);
}

static void semaphore_signal_val(semaphore_t *sem,void *val){
  sem->val=val;
  sem->is_waiting=false;
  semaphore_signal(sem);
}

static void semaphore_signal_ack(semaphore_t *sem){
  sem->need_ack=true;
  sem_post(&sem->sem);
  sem_wait(&sem->ack);
}

static void semaphore_signal_val_ack(semaphore_t *sem,void *val){
  sem->val=val;
  semaphore_signal_ack(sem);
}





