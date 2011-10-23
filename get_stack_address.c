
// Must be placed in a separately compiled file so that the C compiler
// won't optimize the dummy address to be placed before other data.

void set_stack_address(void **stack_bot,int down){
  int dummy;
  if(down==0)
    *stack_bot=&dummy;
  else
    set_stack_address(stack_bot,down-1);
}

int launch_coroutine(int (*func)(void)){
  return func();
}
