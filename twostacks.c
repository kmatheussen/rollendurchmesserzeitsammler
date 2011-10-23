/******************************************************/
/******************************************************/
/***********           stack                 **********/
/******************************************************/
/******************************************************/

struct{
  char *start;
  char *curr;
  char *end;
}typedef stack_t;


static size_t stack_size(stack_t *stack){
  return stack->end - stack->curr;
}


static void *stack_write(stack_t *stack,size_t size){
  void *ret=stack->curr;
  stack->curr+=size;
  if(stack->curr > stack->end){
    stack->curr-=size;
    return NULL;
  }
  return ret;
}

static void *stack_read(stack_t *stack,size_t size){
  return stack_write(stack,size);
}

static void stack_unwrite(stack_t *stack,size_t size){
  stack->curr-=size;
}

static stack_t *stack_create(size_t size){
  stack_t *stack = my_calloc(1,sizeof(stack_t));
  stack->start   = my_calloc(1,size);
  stack->curr    = stack->start;
  stack->end     = stack->start+size;
  return stack;
}

static void stack_delete(stack_t *stack){
  free(stack->start);
  free(stack);
}


/******************************************************/
/******************************************************/
/***********         twostacks               **********/
/******************************************************/
/******************************************************/

struct{
  stack_t *reader;
  stack_t *writer;
  size_t size;
}typedef twostacks_t;


static twostacks_t *twostacks_create(size_t size){
  twostacks_t *twostacks=my_calloc(1,sizeof(twostacks_t));

  twostacks->size=size;

  twostacks->reader = stack_create(size);
  twostacks->writer = stack_create(size);

  // Make reader empty.
  twostacks->reader->curr = twostacks->reader->end;

  return twostacks;
}

static void twostacks_delete(twostacks_t *twostacks){
  stack_delete(twostacks->reader);
  stack_delete(twostacks->writer);
  free(twostacks);
}

// Must not be called unless twostacks_space(twostacks->reader)==0
static void twostacks_switch(twostacks_t *twostacks){
  stack_t *new_reader = twostacks->writer;
  stack_t *new_writer = twostacks->reader;

  assert(stack_size(new_writer)==0);

  new_reader->end  = new_reader->curr;
  new_reader->curr = new_reader->start;

  new_writer->end  = new_writer->start + twostacks->size;
  new_writer->curr = new_writer->start;

  twostacks->writer=new_writer;
  twostacks->reader=new_reader;
}



