
// (not thread safe)

struct pool_elem{
  struct pool_elem *next;
};

struct{
  struct pool_elem *elems;
  int size;
  int num_elems;
} typedef pool_t;


static void pool_append_list(pool_t *pool,void *first_elem,void *last_elem){
  struct pool_elem *first=first_elem;
  struct pool_elem *last=last_elem;
  last->next          = pool->elems;
  pool->elems         = first;
}

static void pool_put(pool_t *pool,void *elem){
  struct pool_elem *p=elem;
  p->next=pool->elems;
  pool->elems=p;
  atomic_add(&pool->num_elems,1);
}

static void pool_addelems(pool_t *pool,int num_elems){
  int i;
  char *elems=my_calloc(num_elems,pool->size);
  for(i=0;i<num_elems*pool->size;i+=pool->size)
    pool_put(pool,&(elems[i]));
}

static pool_t *pool_create(int size,int num_elems){
  pool_t *pool=calloc(1,sizeof(pool_t));
  pool->size=ALIGN_UP_POINTER(size);;
  pool_addelems(pool,num_elems);
  return pool;
}

static void *pool_get(pool_t *pool,bool alloc_new){
  //fprintf(stderr,"  POOOL_GET %d\n",pool->size);
  struct pool_elem *ret=pool->elems;

  assert(pool->num_elems>20);

  if(ret==NULL){
    if(alloc_new==true){
      pool_addelems(pool,512);
      return pool_get(pool,false);
    }else{
      fprintf(stderr,"Error. Pool empty.\n");
      abort();
    }
  }
  pool->elems=ret->next;
  atomic_add(&pool->num_elems,-1);

  return ret;
}

