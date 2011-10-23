
CFLAGS := $(CFLAGS) -Wall -DPROCESSCLOCK

all : lfbench ffbench ffbench2 fftest fflock

lfbench : lfbench.c ../lflifo.o
	$(CC) $(CFLAGS) -O3 lfbench.c -I.. -lpthread  ../lflifo.o -o lfbench

ffbench : ffbench.c ../lffifo.o
	$(CC) $(CFLAGS) -O3 ffbench.c -I.. -lpthread  ../lffifo.o -o ffbench

fflock : ffbench.c
	$(CC) $(CFLAGS) -DFFSEM -O3 ffbench.c -I.. -lpthread -o fflock

ffbench2 : ffbench2.c ../lffifo.o
	$(CC) $(CFLAGS) -O3 ffbench2.c -I.. -lpthread  ../lffifo.o -o ffbench2

fftest : fftest.c ../lffifo.o
	$(CC) $(CFLAGS) -O3 fftest.c -I.. -lpthread  ../lffifo.o -o fftest

clean :
	rm fflock fftest ffbench ffbench2 lfbench ../lffifo.o ../lflifo.o
