
/******************************************************/
/******************************************************/
/***********            Mark                 **********/
/******************************************************/
/******************************************************/



/*
get_snapshot_mempos(mv,heap_mempos):
  return heap_mempos + mv.snapshot_mem_offset

find_mem(mv,address):
  foreach mem in mv.all_meminfos:
    if address>=mem.start && address<mem.end:
       return mem
  return NULL

run_mark(mv,start,end):
  for mempos  = start to end:
      address = *mempos
      if address >= mv.heap_start
      && address <  mv.heap_end:
         mem = find_mem(mv,address)
         if mem!=NULL && mem.marked==false:
            mem.marked = true
            start      = get_snapshot_mempos(mv,mem.start)
            end        = get_snapshot_mempos(mv,mem.end)
            run_mark(mv,start,end)
*/

static char *get_snapshot_mempos(mark_variables_t *mv,char *heap_mempos){
  return heap_mempos + mv->snapshot_mem_offset;
}

static meminfo_t *find_mem(mark_variables_t *mv,char *address){
  char *vmem     = hash_get(mv->hash,address);
  meminfo_t *mem = vmem==NOT_FOUND_POINTER?NOT_FOUND_POINTER:(meminfo_t*)(vmem - sizeof(void*));

  if(mem!=NOT_FOUND_POINTER){
    if(mem->interior_owner==NOT_FOUND_POINTER)
      return NULL; //false address
    else if(mem->interior_owner != NULL)
      return mem->interior_owner;    
    else
      return mem;
  }

  // May be interior pointer
  for(mem=mv->all_meminfos ; mem!=NULL ; mem=mem->next){
    if(address >= mem->start && address < mem->end){
      //meminfo_t *interior=poolmem_alloc(mv->poolmem,sizeof(meminfo_t));
      meminfo_t *interior=pool_get(mv->meminfo_pool,false);

      interior->interior_owner = mem;

      interior->next          = mem->interiors;
      mem->interiors          = interior;

      hash_put(mv->hash,&interior->hash_elem,address);
      return mem;
    }
  }

  // It's a false address.
  //mem                 = poolmem_alloc(mv->poolmem,sizeof(meminfo_t));
  mem                  = pool_get(mv->meminfo_pool,false);
  mem->interior_owner  = NOT_FOUND_POINTER;
  mem->next            = mv->false_addresses;
  mv->false_addresses  = mem;
  hash_put(mv->hash,&mem->hash_elem,address);

  return NULL;
}

static void run_mark(mark_variables_t *mv,char *start,char *end){
  char *mempos;
  
  assert(end>=start);

  for(mempos=start ; mempos<end ; mempos+=ALIGNMENT){  
  //for(mempos=start;mempos<end;mempos+=2){  
    //for(i=start-4;i<end;i+=1){
    char *address=(char*)*(long*)mempos;
    
    if  (address >= mv->heap_start &&
         address <  mv->heap_end){

      meminfo_t *mem=find_mem(mv,address);
      if(mem!=NULL && mem->marked==false){
        mem->marked = true;

        //fprintf(stderr,"mem: %p, start: %p, end: %p, size: %d\n",mem,mem->start,mem->end,mem->end-mem->start);

#if NO_SNAPSHOT
	char *start = mem->start;
	char *end   = mem->end;
#else
	char *start = get_snapshot_mempos(mv,mem->start);
	char *end   = get_snapshot_mempos(mv,mem->end);
	assert(start      >= snapshot_mem);
	assert(end        <= snapshot_mem+heap_size);
#endif
	assert(mem->start >= mv->heap_start);
	assert(mem->end   <= mv->heap_end);

        run_mark(mv,start,end);
      }

    }else if  (address >= mv->atomic_heap_start &&
	       address <  mv->atomic_heap_end){
      meminfo_t *mem=find_mem(mv,address);
      if(mem!=NULL && mem->marked==false){
	mem->marked=true;
	
	assert(mem->start >= mv->atomic_heap_start);
	assert(mem->end   <= mv->atomic_heap_end);
      }
    }
  }
}










