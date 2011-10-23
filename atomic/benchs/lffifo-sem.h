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

#ifndef __LFFIFO_SEM__
#define __LFFIFO_SEM__

#include <pthread.h>
#include "lffifo.h"

#define fifo_end(ff)	0
pthread_mutex_t 	gSemaphore;

unsigned long fifosize (fifo * ff) {
	return ff->count.value;
}

void fifoinit (fifo* ff)
{
	static int init=0;
	ff->count.value = 0;
	ff->head = 0;
	ff->tail = (fifocell*)&ff->head;
	if (!init) {
		pthread_mutex_init (&gSemaphore, 0);
		init=1;
	}
}

void fifoput (fifo * ff, fifocell* cl) 
{
	cl->link = fifo_end(ff);
	pthread_mutex_lock(&gSemaphore);
	ff->tail->link = cl;
	ff->tail = cl;
	ff->count.value++;
	pthread_mutex_unlock(&gSemaphore);
}

fifocell * fifoget (fifo * ff) 
{
	fifocell * head, *next;
	pthread_mutex_lock(&gSemaphore);
	head = ff->head;
	if (head == fifo_end(ff)) {
		pthread_mutex_unlock(&gSemaphore);
		return 0;
	}	
	next = head->link;
	if (next == fifo_end(ff)) {
		ff->tail = (fifocell*)&ff->head;
	}	
	ff->head = next;
	ff->count.value--;
	pthread_mutex_unlock(&gSemaphore);
	return head;
}

#endif
