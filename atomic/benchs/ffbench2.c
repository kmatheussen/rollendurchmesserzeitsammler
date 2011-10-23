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
#include "lffifo.h"

#define STEP		5
#define MAXCOUNT 	30
#define NSEC		20		// each test step duration (in sec)

fifo		gstack;
fifocell	tbl[MAXCOUNT+1];

//-----------------------------------------------------------------
// init stack with cells
static void initstack (fifo *f, long count) {
	int i;
	fifoinit (f);
	for(i=0; i < count; i++) {
		fifoput (f, &tbl[i]);
	}
}

static long stacktest (long n) {
	fifocell *c; 
	fifo *f = &gstack;
	clock_t t = clock();
	while (n--) {
		c = fifoget(f);
		if (!c) {
			printf("error: pop returned null\n");
			break;
		}
		else fifoput(f, c);
	}
	return clock() - t;
}

//-----------------------------------------------------------------
static long getlooplen ()
{
	double d; 
	long initlen=1000000;
	initstack (&gstack, 5);
	d = stacktest(initlen);
	d /= CLOCKS_PER_SEC;
	d = initlen * NSEC / d;
	return (long)d;	
}

//-----------------------------------------------------------------
void bench (int max, int step, long looplen)
{
	int		i;
	double	perf;
	
	for (i=0; i <= max; i+=step) {
		initstack (&gstack, i+1);
		perf = stacktest(looplen);
		perf /= looplen;
		perf *= 1000000 / CLOCKS_PER_SEC;
		printf ("stack size:\t %d\tperf (in us per pop/push):\t %2f\n", i, perf);
  	}
}

int main()
{
	printf("-------- Lock free fifo stack bench ----------\n");
	bench (MAXCOUNT, STEP, getlooplen());
	return 0;
}
