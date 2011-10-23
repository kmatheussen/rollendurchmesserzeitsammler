

/* This ringbuffer handles the situation where there
   are more than one reader and one or more writers.
   Plus that there is no waiting either in the reader
   or writer. Ie. it may lock, but it will never wait
   using for instance the usleep() function.
*/

#define inc_semaphore sem_post
#define dec_semaphore sem_wait

struct ringbuffer{
  char *data;
  int size;
  int datasize;
  int read;
  int write;
  sem_t data_ready;
  //int num_free_elements;
  sem_t unread;
} typedef ringbuffer_t;


static inline void rb_read(ringbuffer_t *rb,void *dest){
  int read;
  sem_wait(&rb->data_ready);

  // Atomically get and set the rb->read variable. (no locks)
  read=atomic_inc_round_and_round(&rb->read,rb->size);
  memcpy(dest,&rb->data[read*rb->datasize],rb->datasize);

  //atomic_add(&rb->num_free_elements,1);
  sem_post(&rb->unread);
}

static inline bool rb_write(ringbuffer_t *rb,void *src,bool multiple_writers,bool can_block){

  if(can_block)
    sem_wait(&rb->unread); // Ouch, can't do this when marking. (might deadlock) (why not just use trywait instead of atomic stuff?)
  else
    if(sem_trywait(&rb->unread)==-1)
      return false;

  //                     // Yes, that will work fine. sem_trywait seems to be just an atomic decrement operation. http://www.koders.com/c/fid65FA3D2FC99DA8F14430BA6CEA5BAA322F8E9F58.aspx

#if 0
  {
    int num_free_elements=atomic_add_get_oldval(&rb->num_free_elements,-1);
    
    if(num_free_elements <= 0){
      atomic_add(&rb->num_free_elements,1);
      return false;
    }
  }
#endif

  {
    int write;  

    if(multiple_writers){
      write=atomic_inc_round_and_round(&rb->write,rb->size);
    }else{
      write=rb->write;
      rb->write++;
      if(rb->write==rb->size)
	rb->write=0;
    }
    
    //fprintf(stderr,"Writing %d\n",write);
    memcpy(&rb->data[write*rb->datasize],src,rb->datasize);
  }

  sem_post(&rb->data_ready);

  return true;
}

#if 0
static bool rb_all_jobs_done(ringbuffer_t *rb){
  return rb->num_free_elements==rb->size-2 ? true : false;
  //  return rb->write==rb->read?true:false;
}
#endif

static ringbuffer_t *rb_new(int size,int datasize){
  ringbuffer_t *rb=calloc(1,sizeof(ringbuffer_t));
  rb->data=calloc(datasize,size);
  rb->size=size;
  rb->datasize=datasize;

  sem_init(&rb->data_ready,0,0);
  sem_init(&rb->unread,0,size-2);
  //rb->num_free_elements = size-2;

  return rb;
}
