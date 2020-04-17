CFLAGS=-std=c11 -g -static

Ccc: Ccc.c

test: Ccc
				./test.sh

clean:
				rm -f Ccc *.o *~ tmp*

.PHONY: test clean