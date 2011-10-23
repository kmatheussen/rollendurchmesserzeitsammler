#include <ucontext.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <jack/jack.h>
#include <jack/thread.h>
#include <pcl.h>

#include "rollendurchmesserzeitsammler.h"


// For DATASTART and DATAEND
#include <gc/gc.h>

// Using DATASTART and DATAEND
#include "gcconfig.h"


static coroutine_t dsp_coroutine;
static float *block;
static jack_port_t *port;
static jack_client_t *client;

static void *stack_top;
static void *stack_bot;

static void *heap;


static int get_blocksize(void){
  return jack_get_buffer_size(client);
}

static void out_sample(int i,float val){
  //printf("%d: val: %f\n",i,val);
  block[i]=val;
}


static void schemedebug(int val){
  //fprintf(stderr,"sd: %d\n",val);
}

//static void schemedebug2(void *val){
//  fprintf(stderr,"sd2: %p\n",val);
//}

ucontext_t ctxt;

// set_stack_address and launch_coroutine sets stack top and bot values.
// This needs to be done in a separately compiled file, so that gcc won't
// do any smart optimisations.
extern void set_stack_address(void **stack_bot,int down);
extern void launch_coroutine(int (*func)(void));


int scheme_main(void);
static void dsp_coroutine_func(void *arg){
  int dummy;
  stack_top=&dummy;
  launch_coroutine(scheme_main);
}


static void block_ready(void){
  //int dummy;

  // This is not necessary. Remove if everything works, and scratch head if it stops working after being removed.
#if 0
  if (getcontext(&ctxt) < 0){
    fprintf(stderr,"Getcontext failed.\n");
    exit(-5);
  }
#endif
  //tar_add_root(heap,&ctxt,((char*)&ctxt)+sizeof(ucontext_t));

  //stack_bot=&dummy; //get_stack_address();
  set_stack_address(&stack_bot,2);

  //printf("Stack bot: %p, top: %p. size: %dk\n",stack_bot,stack_top,(((char*)stack_top)-((char*)stack_bot))/1024);
  co_resume();
}

static float between(float start,float end){
  return start + (end-start)*((float)random()/(float)RAND_MAX);
}

static int intbetween(int start,int end){
  return start + (end-start)*((float)random()/(float)RAND_MAX);
}


#define main scheme_main
#  include "graincloud.c"
#undef main

//int stack[32*4096];

static int process (jack_nframes_t nframes, void *arg)
{

  tar_init_block((float)nframes/44100.0f);


  //fprintf(stderr,"entering\n");
  tar_before_using_heap(heap);
  //fprintf(stderr,"got it\n");


  block = jack_port_get_buffer (port, nframes);
#ifdef BENCHSNAPSHOT
  tar_alloc(heap,64);
#else
  co_call(dsp_coroutine);
#endif
  //fprintf(stderr,"Stack bot: %p, top: %p\n",stack_bot,stack_top);

  if(tar_after_using_heap(heap)){

    // Add stack
    tar_add_root_concurrently(heap,stack_bot,stack_top);
    
    // Add registers
    tar_add_root(heap,dsp_coroutine,((char*)dsp_coroutine)+sizeof(ucontext_t));

    // Add data
    tar_add_root_concurrently(heap,((char*)DATASTART),((char*)DATAEND));

    //    printf("Schemedata size:  %p %p %d\n",DATASTART,DATAEND,((char*)DATAEND)-((char*)DATASTART));
    //printf("Schemedata stack: %p %p %d\n",&stack[0],&stack[32*4096+1],0);

    // Signal gc to run.
    tar_start_gc(heap);
    //tar_start_gc(heap,true);
  }
  //fprintf(stderr,"exiting\n");
  return 0;
}





int main(int argc,char **argv){

  // Set the values accessed by DATASTART and DATAEND
  GC_init();

  srandom(51234); // To get same data every time when benchmarking.

  system("/hom/kjetism/site/bin/rt_sanitycheck");

  dsp_coroutine=co_create(dsp_coroutine_func,NULL,NULL,32*4096);
  //fprintf(stderr,"ai: %p\n",dsp_coroutine);

  client = jack_client_new ("schemetest");

  if(client==NULL){
    fprintf(stderr,"Could not connect to jack\n");
    exit(1);
  }


  fprintf(stderr,"1\n");
  // Init gc
  tar_init(1024*1024*10, // Static heap. (size doesn't matter for performance)
           5*1024*1024, // Non-static heap. (the larger the size, the more cpu usage)
           1024*128,  // Maximum size of allocated memory.
           jack_client_real_time_priority(client),
           1.0f
           );

  heap=tar_create_heap();
  fprintf(stderr,"2\n");
  // init jack.

  port = jack_port_register (client, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  // Init scheme code.
  fprintf(stderr,"ai-1\n");
  co_call(dsp_coroutine);
  fprintf(stderr,"ai0\n");
#if 0
  int i;
  for(i=0;i<4;i++){
    fprintf(stderr,"ai1\n");
    co_call(dsp_coroutine);
    fprintf(stderr,"ai2\n");
  }
  exit(0);
#endif

  fprintf(stderr,"3\n");

  // Start process.
  jack_set_process_callback (client, process, NULL);
  tar_touch_heaps(heap);
  jack_activate (client);
  jack_connect (client, jack_port_name(port),"system:playback_1");
  jack_connect (client, jack_port_name(port),"system:playback_2");

  fprintf(stderr,"4\n");
  if(argc>1)
    sleep(atoi(argv[1]));
  else{
    char in[1000];    
    gets(in);//sleep(10);
  }

#if 0
  int i;
  for(i=0;i<4;i++){
    co_call(dsp_coroutine);
  }
#endif

  //tar_printstats();

  tar_bench_print(heap);

  return 0;
}
