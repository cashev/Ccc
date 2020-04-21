CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

Ccc: $(OBJS)
				$(CC) -o Ccc $(OBJS) $(LDFLAGS)

$(OBJS): Ccc.h

test: Ccc
				./test.sh

clean:
				rm -f Ccc *.o *~ tmp*

.PHONY: test clean