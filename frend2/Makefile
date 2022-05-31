INCL    = -I..

OBJS    =   frend2.o               \
            frend2_helpers.o       \
            ../msufrend_util.o       

frend2: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

all: frend2

clean:
	rm -f *.o frend2

.c.o:
	$(CC) $(CFLAGS) $(INCL) -c $*.c

