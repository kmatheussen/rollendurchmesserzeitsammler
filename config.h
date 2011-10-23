
#define MAX_ROOTS_SIZE (1024*1024)

#define MAX_RB_ELEMENTS (1024*1024)

#define MAX_FREE_ELEMENTS (1024*1024)

#define SNAPSHOT_COPY_SIZE (16*1024)
#define SNAPSHOT_DIVIDE "4"

#define POOL_SIZE_FACTOR 0.5

#define NOT_FOUND_POINTER ((void*)-1)

#define NDEBUG 1


// Whether to free memory during alloc or during free.
// During alloc is better for the CPU cache, during free causes less instructions.
// (benchmarks show minimal difference)
#define CLEAR_MEMORY_DURING_ALLOCATION 0


// In case Rollendurchmesserzeitsammler is unable to automatically
// detect speed of processor, it can be set here.
#define CPU_MHZ 2667
// turn off TURBO!

#define NUM_CPUS 4
#define MUTATOR_CPU 0
#define COLLECTOR_CPU 1

// Turns on benchmarking. Uses TSC, so it should not affect
// performance severly turning it on. Lots of stuff happens though.
#define BENCHMARK 1


// Writes files with fragmentation info (BENCHMARK must also be 1!)
#define ANALYSE_FRAGMENTATION 0
#define FRAGMENTATION_FILENAME "/tmp/fragmentation.data"
#define HISTOGRAM_FILENAME "/tmp/histogram.data"


// Turns the gc into a fairly traditional non-realtime mark-and-sweep collector.
#define NO_SNAPSHOT 0


// I get a 16% decrease in snapshot time on an intel dual core 2 computer setting
// this to 1. Whether the combined extra spent time is wasted or not, I don't
// know. Because of this combined with uncertainty about realtime behaviour of doing snapshots
// in parallel, PARALLEL_SNAPSHOT is set to 0 by default.
#define PARALLEL_SNAPSHOT 0


// Significantly improves performance and consistant CPU time for NO_SNAPSHOT. If not, only a small difference.
#define BOUND_THREADS_TO_ONE_CPU 1


// For debugging. Significantly lowers the performance.
#define ALWAYS_FULL_SNAPSHOT 0

// Only for debugging. Not sure the gc behaves strictly correct when this one is set.
//#define DEBUG_DOUBLEFREE 1




#define USE_TLSF 0

// To avoid unpredictable fragmentation (poolmem only) (makes no difference in practice)
#define DONT_REDUCE_FREE_STACK 1

// Dangerous... (not implemented for TLSF)
#define USE_SYSTEM_ALLOC_IF_OUT_OF_MEMORY 0

// Only for poolmemalloc. Very un-O(1). For testing only. (DONT_REDUCE_FREE_STACK must be 0)
#define USE_DEFRAGMENTER 0

// Only for comparing fragmentation between TSLF and poolmem
#define ADD_TLSF_HEADER 0


