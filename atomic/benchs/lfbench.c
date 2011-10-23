/*

  Copyright © Grame 2001-2005

  This library is free software; you can redistribute it and modify it under 
  the terms of the GNU Library General Public License as published by the 
  Free Software Foundation version 2 of the License, or any later version.

  This library is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public 
  License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  Grame Research Laboratory, 9, rue du Garet 69001 Lyon - France
  research@grame.fr

*/

 
#include <stdio.h>
#include <time.h>
#include "pthread.h"

#include "lflifo.h"

#define MAXTHREADS	10
#define MAXCOUNT 6*MAXTHREADS

typedef struct pont {
	long 	limit;
	long 	index;
	lifocell* first;
	long	stopped;
	long	duration;
} pont;

	
/*
 *	Variables globales
 */
#define pile lifo
#define POP lfpop
#define PUSH lfpush
#define LASTCELL(c)		((c) == 0)

#define LIMIT	500000

pile	gstack;
lifocell tbl[MAXCOUNT];

// init stack with cells
void initstack (pile *f)
{
	int i;
	lfinit (f);
	for(i=0; i < MAXCOUNT; i++) {
		PUSH (f, &tbl[i]);
	}
//	printf("init stack : size %ld (i=%d)\n", lfsize(f), i);
}

long stacktest (long n)
{
	lifocell * temp[6];
	int i; clock_t t;
	
	t = clock();
	while (n--) {
		for (i=0; i<6; i++) {
			temp[i] = POP(&gstack);
			if (temp[i] == 0) printf ("erreur\n");
		}
		for (i=0; i<6; i++) PUSH(&gstack, temp[i]);
	}
	return clock() - t;
}

void stacktestthread(pont* p)
{
	p->duration = stacktest(p->limit);
	p->stopped = 1;
}


//-----------------------------------------------------------------
static long count (pile * ff)
{
	long count = 0;
	lifocell * cl;
	
	cl = (lifocell *)ff->top;
//	while (!LASTCELL(cl)) {
	while (cl) {
		count++;
		cl = cl->link;
		if (count > MAXCOUNT + 10)
			return -1;
	}
	return count;
}

void checkstack (pile * f, int n)
{
	int i = count(f);
	if (i != n) { 
		printf("erreur dans la pile %p : count=%d, (should be %d)\n", f, i, n);
		printf("       fifo size: %ld %p\n", lfsize(f), lfavail(f));
	}
}

void bench (int max)
{
	pthread_t	fils[MAXTHREADS];
	pont		bridge[MAXTHREADS]; 
	int			i, end, th;
	float		perf;
	
	for (th=1; th <= max; th++) {
		initstack (&gstack);
	  	printf("threads count:\t %d \t", th); fflush (stdout);
	  	for (i=0; i<th; i++) {
		  	bridge[i].limit = LIMIT;
		  	bridge[i].stopped = 0; 
		  	pthread_create(&fils[i], NULL, (void*)&stacktestthread, &bridge[i]);
	  	}
	  	do {
		  	end = 1; for (i=0; i<th; i++)  end &= bridge[i].stopped;
	  	} while ( end == 0 );
		perf = 0;
		for (i=0; i<th; i++) { perf += bridge[i].duration; }
		perf = (perf * 1000000) / (6 * CLOCKS_PER_SEC);
		perf /= (th * LIMIT);
		printf (" perf (in us per pop/push):\t %2f\n", perf); fflush (stdout);
		checkstack(&gstack, MAXCOUNT);
  	}

}
	

int main()
{
	printf("-------- Lock free lifo stack bench ----------\n");
	bench (8);
	return 0;
}
