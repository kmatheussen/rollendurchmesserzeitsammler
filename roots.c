
/*
reset_roots():
  roots_end = roots_start

take_root_snapshot(start,end):
  size = end - start
  for i = 0 to size:
      roots_end[i] = start[i]
  roots_end += size
*/

static int num_roots=0;
static void *root_starts[1024];
static void *root_ends[1024];

static void take_root_snapshot(char *start,char *end){
  size_t size = end - start;

  if(size==0)
    return;

  if(roots_end + size > roots_start + MAX_ROOTS_SIZE){
    fprintf(stderr,"Error in Rollendurchmesserzeitsammler. Roots too small. Increase MAX_ROOTS_SIZE (%u/%u)\n",(unsigned)MAX_ROOTS_SIZE,(unsigned)(roots_end+size-roots_start));
    abort();
  }
  //fprintf(stderr,"take root snapshot %p %p %p %d\n",roots_end,start,end,(int)size);
  memcpy(roots_end,start,size);
  //fprintf(stderr,"Got it. take root snapshot %p %p\n",start,end);
  roots_end += size;
}


static void reset_roots(){
  roots_end = roots_start;
  num_roots=0;
}


// Copy scheduled concurrent root snapshots
static void take_roots_snapshot(tar_heap_t *heap){
  int i;
  for(i=0;i<num_roots;i++){
    take_root_snapshot(root_starts[i],root_ends[i]);
  }
  num_roots=0;
}


void tar_add_root(tar_heap_t *heap,void *start,void *end){
  if(end<start){
    void *temp=end;
    end=start;
    start=temp;
  }
  take_root_snapshot(start,end);
}

// Schedule concurrent root snapshots
void tar_add_root_concurrently(tar_heap_t *heap,void *start,void *end){
#if 0
  tar_add_root(heap,start,end);
#else
  if(end<start){
    void *temp=end;
    end=start;
    start=temp;
  }
  //fprintf(stderr,"%d: tar_add_root_conc: %p-%p\n",num_roots,start,end);
  root_starts[num_roots]=start;
  root_ends[num_roots]=end;
  num_roots++;
  //take_root_snapshot(start,end);
#endif
}

