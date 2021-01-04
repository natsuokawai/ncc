CFLAGS=-std=c11 -g -fno-common
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

ncc: $(OBJS)
		$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): ncc.h

test: ncc
		./test.sh

clean:
		rm -f ncc *.o *~ tmp*

.PHONY: test clean
