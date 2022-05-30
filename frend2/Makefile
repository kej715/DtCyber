# Makefile for Linux build of frend2.
#   /mrr  2006-02-03
LIBS    = -lm -lpthread
LDFLAGS =
INCL    = -I..

CFLAGS  = -O2 -I. $(INCL) -std=gnu99

OBJS    =   frend2.o               \
            frend2_helpers.o       \
            msufrend_util.o       

frend2: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
 
msufrend_util.o : ../msufrend_util.c
	$(CC) $(CFLAGS) -c ../msufrend_util.c

all: clean frend2

clean:
	rm -f *.o frend2

.c.o:
	$(CC) $(CFLAGS) -c $*.c

