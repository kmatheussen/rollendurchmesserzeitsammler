

/*

- Hver heap har hver sin hash-tabell.

- Når en heap frigjøres, kan alle meminfo-objekter
  frigjøres med bare en operasjon fra innsiden:

  heap->meminfos->last->next = mv.mark_variables
  mv.mark_variables = heap->meminfos

  (husk at heap->meminfos må først inkorporere alle interior pointers også)

  Hmm, jo, men alt mulig kan jo gjøres etter at sweep har kjørt da.

*/



struct das_hash_elem{
  struct das_hash_elem *next;
  void *key;
}typedef hash_elem_t;


struct{
  //hash_elem_t *elems;
  void *not_found;
  int table_size;
  hash_elem_t *table[];
}typedef hash_t;



static hash_t *hash_create(int table_size,void *not_found){
  hash_t *hash=my_calloc(1,sizeof(hash_t) + (table_size*sizeof(hash_elem_t*)));
  hash->not_found=not_found;
  hash->table_size=table_size;
  return hash;
}

static void hash_delete(hash_t *hash){
  free(hash);
}

static int hash_get_tablenum(hash_t *hash,void *pointer){
  char *v=pointer;
  //return 0;
  return (((intptr_t)v)/POINTERSIZE) % hash->table_size;
}

static void hash_put(hash_t *hash,hash_elem_t *elem,void *key){
  int num          = hash_get_tablenum(hash,key);

  elem->key        = key;
  
  elem->next       = hash->table[num];
  hash->table[num] = elem;
}

static void *hash_get(hash_t *hash,void *key){
  int num=hash_get_tablenum(hash,key);
  hash_elem_t *elem=hash->table[num];

  while(elem!=NULL){
    if(elem->key==key)
      return elem;
    elem=elem->next;
  }
  return hash->not_found;
}

static void hash_remove(hash_t *hash,void *key){
  hash_elem_t *prev=NULL;
  int num=hash_get_tablenum(hash,key);
  hash_elem_t *elem=hash->table[num];

  while(elem!=NULL){
    hash_elem_t *next=elem->next;
    if(elem->key==key){
      if(prev==NULL)
	hash->table[num]=next;
      else
	prev->next=next;
      return;
    }
    prev=elem;
    elem=next;
  }

  fprintf(stderr,"Error. Trying to free element not in hash table\n");
  assert(0);
  abort();
}
