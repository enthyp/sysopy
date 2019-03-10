CC=gcc
CFLAGS=-c -Wall

all: test_static test_shared


test_static: libfind.a test_find.o
	$(CC) test_find.o -L. -lfind -o test_static

libfind.a:
	make -f make_lib libfind.a

test_find.o: test_find.c
	$(CC) $(CFLAGS) test_find.c -o test_find.o


test_shared: libfind.so test_find.o
	$(CC) test_find.o -L. -lfind -o test_shared	
	
libfind.so:
	make -f make_lib libfind.so


clean:
	rm -f *.o *.a *.so 
