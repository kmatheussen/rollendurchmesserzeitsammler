
#include <pthread.h>


struct{
  pthread_mutex_t lock;
  int num_visitors;
}typedef mutex_t;


static void mutex_init(mutex_t *mutex){
  pthread_mutex_init(&mutex->lock,NULL);
  mutex->num_visitors=0;
}

static void mutex_destroy(mutex_t *mutex){
  pthread_mutex_destroy(&mutex->lock);
}

static void mutex_lock_exclusive(mutex_t *mutex){
  pthread_mutex_lock(&mutex->lock);
}
static void mutex_unlock_exclusive(mutex_t *mutex){
  pthread_mutex_unlock(&mutex->lock);
}


static bool mutex_lock_shared(mutex_t *mutex){
  if(atomic_add_get_oldval(&mutex->num_visitors,1)==0){
    pthread_mutex_lock(&mutex->lock);
    return true;
  }
  return false;
}

static bool mutex_unlock_shared(mutex_t *mutex){
  if(atomic_add_get_oldval(&mutex->num_visitors,-1)==1){
    pthread_mutex_unlock(&mutex->lock);
    return true;
  }
  return false;
}
