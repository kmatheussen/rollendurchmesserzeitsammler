
#include <stdio.h>
#include <assert.h>
#include "pthread.h"

#include "lffifo.h"

typedef struct testcell {
	struct fifocell* volatile link;	/* next cell in the list */
	long value[2];
} testcell;

#define Q 21
testcell tbl[Q];
fifo 	f;

	
#define ITER 	10000000 //0 //100000000
#define THR		20

void * runtest(void* param)
{
	long id = (long) param;
	int i;	
	for ( i= 0; i < ITER; i++) {
		testcell* c = (testcell*)fifoget(&f);
		assert(c);
		if (c) {
			c->value[0] = id;
			c->value[1] ++;
			fifoput(&f,(fifocell*)c);
		}
	}
	pthread_exit(NULL);
	return 0;
}

int main()
{
	int i;
	fifoinit(&f); //, &tbl[0]);
	for (i = 1; i<Q; i++) {
		fifoput(&f, (fifocell*)&tbl[i]);
	}
	
	printf("Fifo test preamble...");
	for (i = 0; i < 1002; i++) {
		fifocell* c = fifoget(&f);
		assert(c);
		fifoput(&f, c);
	}
	printf("\n");
	
	printf("Running fifo test with %d threads and %d iterations...\n", THR, ITER);
	{	// test avec des threads
		pthread_t threads[THR];
		for (i = 0; i < THR; i++) {
			pthread_create(&threads[i], NULL, runtest, (void*) i);
		}
		for (i = 0; i < THR; i++) {
			pthread_join(threads[i], NULL);
		}
	}
		
	printf("Fifo size = %ld\n", fifosize(&f));	
	{
		testcell* c;
		long 	n = 0;
		i = 0;
		while ((c = (testcell*)fifoget(&f))) {
			printf(" %2d : %p [%ld,%10ld]\n", i, c, c->value[0], c->value[1]);
			n += c->value[1];
//			printf(" %d : %p[%d,%d]\n", i, c, c->fContent.fData[0], c->fContent.fData[1]);
			i++;
		}
		printf("Total = %ld\n", n);
		assert (n == ITER*THR);
		assert (i+1 == Q);
	}
	return 0;
}
