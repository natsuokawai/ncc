CFLAGS=-std=c11 -g -fno-common

ncc: main.o
		$(CC) -o ncc main.o $(LDFLAGS)

test: ncc
		./test.sh

clean:
		rm -f ncc *.o *~ tmp*

.PHONY: test clean