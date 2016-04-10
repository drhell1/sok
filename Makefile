CC = cc

all:
	$(CC) -fPIC -O2 -ansi -std=c11 -c sok.c -o sok.o
	$(CC) -shared sok.o -o libsok.so

install:
	cp libsok.so /usr/lib
	cp sok.h /usr/include

clean:
	-rm sok.o
	-rm libsok.so

uninstall:
	-rm /usr/lib/libsok.so
	-rm /usr/include/sok.h

.PHONY: test all clean install uninstall
