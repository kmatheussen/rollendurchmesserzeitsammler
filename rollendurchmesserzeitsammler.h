#ifndef __rollendurchmesserzeitsammler_h__
#define __rollendurchmesserzeitsammler_h__

#include <stdbool.h>
#include <stdlib.h>

typedef struct _tar_heap tar_heap_t;

void tar_init(int new_atomic_heap_size,
	      int new_heap_size,
	      int max_mem_size,
              int audio_priority_setting,
	      float time_between_full_snapshot);

void *tar_create_heap(void);
void tar_delete_heap(tar_heap_t *heap,bool audio_is_running);

void *tar_alloc(tar_heap_t *heap,size_t size);
void *tar_alloc_atomic(tar_heap_t *heap,size_t size);

size_t tar_get_used_mem(tar_heap_t *heap);
size_t tar_get_used_atomic_mem(tar_heap_t *heap);

void tar_dont_gc_now(tar_heap_t *heap);
//#define TAR_DONT_GC_NOW(heap) ((heap)->start_new_gc)=false; // Faster.

void tar_init_block(float duration);

void tar_before_using_heap(tar_heap_t *heap);
bool tar_after_using_heap(tar_heap_t *heap);

void tar_add_root(tar_heap_t *heap,void *start,void *end);
void tar_add_root_concurrently(tar_heap_t *heap,void *start,void *end);

void tar_start_gc(tar_heap_t *heap);

int tar_get_dynamic_roots_for(char *pointer,char **start,char **end);

void tar_bench_print(tar_heap_t *heap);
void tar_touch_heaps(tar_heap_t *heap);
void end_fragmentation_analysis(tar_heap_t *heap);

//void tar_dont_snapshot_now(tar_heap_t *heap);

#endif
