
#include "atomicity/atomicity.h"

static inline void atomic_add(int *mem,int inc){
  __atomic_add(mem,inc);
}

static inline int atomic_add_get_newval(int *mem,int inc){
  int oldval=__exchange_and_add(mem,inc);
  return oldval+inc;
}

static inline int atomic_add_get_oldval(int *mem,int inc){
  return __exchange_and_add(mem,inc);
}

static inline int atomic_inc_round_and_round(int *mem,int size){
  int ret;

  ret=atomic_add_get_oldval(mem,1);


  /*
    The modulo test below works like this:
    if ret == (size*n)-1, where n>=1

    ...and where n could theoretically be >=2 if there has been a long
    break at this point at a previous execution. (better be safe, although
    for size>=(num_cpus*10), I have my doubts...)
  */
  if( 0 == ( (1+ret) % size))
    atomic_add(mem,-size);

  while(ret>=size)
    ret-=size;

  return ret;
}

