
VERSION=0.1.6

PREFIX=/usr/local

HBGC_PATH=../gc-7.1

CONFFILE = /etc/rollencurchmesserzeitsammler.conf

# Set USE_TLSF in config.h instead
USE_TLSF=
#USE_TLSF=-DUSE_TLSF

TLSF_PATH=TLSF-2.4/src

OPTIONS=

OPT=-O3 -march=native 
# -malign-double
# -fno-builtin-memcpy -funroll-loops

CC=gcc

CFLAGS=-Wall -g -I$(TLSF_PATH) -I. -I$(HOME)/site/include $(USE_TLSF) $(OPT) $(OPTIONS) -DWHICH_XMESSAGE=\"`which xmessage`\" -Wno-unused-function -DCONFFILE=\"$(CONFFILE)\"
# -std=c99 -D_GNU_SOURCE

LDFLAGS=-lpthread -L$(HOME)/site/lib

TAR=rollendurchmesserzeitsammler

DIST=../$(TAR)-$(VERSION)

all: lib$(TAR).a $(CONFFILE)
	which xmessage

install: lib$(TAR).a $(TAR).h
	cp -f $(TAR).h $(PREFIX)/include/
	cp -f lib$(TAR).a $(PREFIX)/lib/

uninstall:
	rm -f $(PREFIX)/$(TAR).h
	rm -f $(PREFIX)/$(TAR).a

dist: clean
	rm -fr /tmp/$(TAR)-$(VERSION)
	rm -fr $(TAR)-$(VERSION)
	mkdir /tmp/$(TAR)-$(VERSION)
	cp -a * /tmp/$(TAR)-$(VERSION)/
	mv /tmp/$(TAR)-$(VERSION) .
	tar cvf $(TAR)-$(VERSION).tar $(TAR)-$(VERSION)
	gzip $(TAR)-$(VERSION).tar
	marcel_upload $(TAR)-$(VERSION).tar.gz
	ls -la $(TAR)-$(VERSION)
	rm -fr $(TAR)-$(VERSION)

lib$(TAR).a: $(TAR).o dyn_load.o various.o $(TLSF_PATH)/tlsf.o
	ar r lib$(TAR).a $(TAR).o $(TLSF_PATH)/tlsf.o dyn_load.o various.o

dyn_load.o: $(HBGC_PATH)/dyn_load.c
	$(CC) -I$(HBGC_PATH)/include ../gc-7.1/dyn_load.c -c

$(TAR).o: $(TAR).c Makefile config.h
	$(CC) $(CFLAGS) $(TAR).c -c

various.o: various.c

test: lib$(TAR).a
	$(CC)  test.c $(CFLAGS) $(LDFLAGS)

benchmark: $(TLSF_PATH)/tlsf.o
	$(CC) $(CFLAGS) benchmark.c $(LDFLAGS)
	sh -c 'time ./a.out'

clean:
	rm -f *~ */*~ a.out graincloud.db *.o  $(TLSF_PATH)/tlsf.o lib$(TAR).a graincloud *.tar.gz find_worst_case


get_stack_address.o: get_stack_address.c
	$(CC) get_stack_address.c -c

#OPTS=-msse3 -msse2 -msse -march=prescott

graincloud: graincloud.c stalinwrapper.c $(TLSF_PATH)/tlsf.o Makefile lib$(TAR).a get_stack_address.o
	$(CC) stalinwrapper.c get_stack_address.o lib$(TAR).a $(CFLAGS) $(LDFLAGS) -ljack -lpcl -o graincloud -lgc -malign-double -ffast-math -fomit-frame-pointer -freg-struct-return 

#-O3 -fthread-jumps           -falign-functions  -falign-jumps           -falign-loops  -falign-labels           -fcaller-saves           -fcrossjumping           -fcse-follow-jumps  -fcse-skip-blocks           -fdelete-null-pointer-checks           -fexpensive-optimizations           -fgcse  -fgcse-lm            -foptimize-sibling-calls           -fpeephole2           -fregmove           -freorder-blocks  -freorder-functions           -frerun-cse-after-loop            -fsched-interblock  -fsched-spec           -fschedule-insns  -fschedule-insns2           -fstrict-aliasing -fstrict-overflow           -ftree-pre           -ftree-vrp -fno-inline-functions -funswitch-loops -fpredictive-commoning -fgcse-after-reload -ftree-vectorize 

#	./graincloud

snapshotbench:
	$(CC) stalinwrapper.c $(TAR).c $(CFLAGS) $(LDFLAGS) -ljack -lpcl -lgc -ffast-math -fomit-frame-pointer -malign-double -freg-struct-return -o graincloud -march=athlon-xp -DBENCHSNAPSHOT
	./graincloud

find_worst_case: find_worst_case.c dyn_load.o various.o $(TLSF_PATH)/tlsf.o
	$(CC) find_worst_case.c -o find_worst_case $(TLSF_PATH)/tlsf.o dyn_load.o various.o $(CFLAGS) $(LDFLAGS) 

graincloud.c: graincloud.scm
	which stalin
	rm -f graincloud.c
	stalin -On -d0 -d1 -d5 -d6 -split-even-if-no-widening -Ob -Om -Or -Ot -q -c -d -architecture IA32-align-double graincloud.scm

$(CONFFILE):
	make calculate_snapshot_performance
	./calculate_snapshot_performance $(CONFFILE)

calculate_snapshot_performance: calculate_snapshot_performance.c
	gcc calculate_snapshot_performance.c $(CFLAGS) -o calculate_snapshot_performance -lpthread -lrt

#-no-clone-size-limit 

# -no-clone-size-limit
# 
#-d0 -d1 -d5 -d6 -On -q -d -no-clone-size-limit -split-even-if-no-widening -copt -O2


$(TLSF_PATH)/tlsf.o:
	cd  $(TLSF_PATH) && make

rollendurchmesserzeitsammler.o: rollendurchmesserzeitsammler.c

