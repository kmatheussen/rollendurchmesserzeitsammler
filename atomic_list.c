

typedef lifocell atomic_list_t;

static atomic_list_t *atomic_list_create(){
  atomic_list_t *list=calloc(1,sizeof(lifocell));
  lfinit(list);
  return list;
}

static void *atomic_list_getFirst(atomic_list_t *list){
  
}




