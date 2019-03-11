CC=gcc
CFLAGS=-c -Wall

all: test_static test_shared


test_static: libfind.a test_find.o
	$(CC) -g test_find.o -L. -lfind -o test_static

libfind.a:
	make -f make_lib libfind.a

test_find.o: test_find.c
	$(CC) $(CFLAGS) -g test_find.c -o test_find.o


test_shared: libfind.so test_find.o
	$(CC) -g test_find.o -L. -lfind -o test_shared	
	
libfind.so:
	make -f lib.mk shared


clean:
	rm -f *.o *.a *.so test_static test_shared
