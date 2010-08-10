TOPDIR=../
include $(TOPDIR)Rules.make

all:

test-x86.o: test-x86.c ../config.h x86*.h
	$(CC) test-x86.c -o $@ -c

test-x86: test-x86.o
	$(CC) -o $@ $^ -lm

clean:
	rm -f *.o *$(LIB_SUFFIX) test-x86

install:

uninstall:
