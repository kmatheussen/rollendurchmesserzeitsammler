
#ifdef TEST
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#endif



// The minimal stuff is here to spare some memory on 64 bit OS.


struct{
  uint32_t pos;
  uint32_t size; // last bit is set if atomic.
}typedef meminfo_minimal_t;



static void minimal_write(meminfo_minimal_t *minimal,void *start,void *pointer,size_t size,bool is_atomic){
  minimal->pos  = ((char*)pointer) - ((char*)start);  
  if(is_atomic)
    minimal->size = size | 1;
  else
    minimal->size = size;
}

static void *minimal_read(meminfo_minimal_t *minimal,void *start,void *atomic_start,size_t *size,bool *is_atomic){
  uint32_t m_pos=minimal->pos;
  uint32_t m_size=minimal->size;

  if(m_size&1){
    *is_atomic = true;
    m_size--;
    start      = atomic_start;
  }else{
    *is_atomic = false;
  }

  *size=m_size;
  return ((char*)start) + m_pos;
}


/*
26 bits: 67.108.864 (pointer)
 1 bit : atomic
-2 bit : size alignment
-2 bit : pointer alignment
---------------------------------
23 bit

32 - 23 =  9 =     1.024
40 - 23 = 17 =   131.072
48 - 23 = 26 =
*/


#if 0
struct{
  uint8_t a1;
  uint8_t a2;
  uint8_t a3;
  uint8_t a4;
  uint8_t b;
}typedef meminfo_minimal2_t;

static void minimal_write(meminfo_minimal_t *minimal,void *start,void *pointer,size_t size,bool is_atomic){
  //meminfo_minimal2_t *minimal2=(meminfo_minimal2_t*)minimal;
  uint32_t a1;
  uint32_t a2;
  uint8_t   b;

  assert(size<=131072-4);

  a1    = ((char*)pointer) - ((char*)start);
  assert(a1 <= 67108864-4);
 
  a1    = a1 << 6;

  a2    = size >> 8;

  //fprintf(stderr,"write, a1: %x, a2: %x (%x)\n",a1,a2,~((2<<7)-1));

  b     = size >> 1;

  {
    uint32_t a = a1 | a2;
    if(is_atomic)
      b = b | 1;
    minimal->a1 = (a << 24);
    minimal->a2 = (a << 16);
    minimal->a3 = (a << 8);
    minimal->a4 = a;
    minimal->b = b;
  }
}

static void minimal_read(meminfo_minimal_t *minimal,void *start,void *atomic_start,void **pointer,size_t *size,bool *is_atomic){
  //meminfo_minimal2_t *minimal2=(meminfo_minimal2_t*)minimal;
  uint32_t a1
    =  (minimal->a1 << 24)
    |  (minimal->a2 << 16)
    |  (minimal->a3 << 8)
    |  (minimal->a4);
  uint32_t a2 = a1;
  uint8_t   b = minimal->b;

  if(b&1){
    *is_atomic = true;
    b--;
    start      = atomic_start;
  }else{
    *is_atomic = false;
  }
  
  a1       = (a1 >> 6) & (~3);
  *pointer = ((char*)start)+a1;

  a2       = a2 & ((2<<8)-1);
  a2       = a2 << 8;
  
  *size    = a2 | (b << 1);

  //fprintf(stderr,"read, a1: %x (%x), a2: %x, b: %x (%x)\n",a1,minimal->a,a2,b,~((2<<8)-1));
}

#endif



#ifdef TEST
int main(){
  char mem[1000];
  char *pointer=mem+67108864-4;
  size_t size=131072-4;
  //size_t size=260;
  meminfo_minimal_t minimal;

  minimal_write(&minimal,mem,pointer,size,true);

  {
    char *pointer2;
    size_t size2;
    bool b;
    minimal_read(&minimal,mem,mem,(void**)&pointer2,&size2,&b);
    fprintf(stderr,"point: %p/%p \t size: %x/%x (double: %x), %d %d\n",pointer,pointer2,(int)size,(int)size2,(int)size2*2,b,sizeof(meminfo_minimal_t));
  }
  return 0;
}
#endif
