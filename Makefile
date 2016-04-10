CC = cc

all:
	$(CC) -fPIC -O2 -ansi -std=c11 -c sok.c -o sok.o
	$(CC) -shared sok.o -o libsok.so

go:
	$(CC) -fPIC -g3 -ansi -std=c11 -c sok.c -o sok.o

test: go
	$(CC) -L. -lssl -lcrypto -lpthread sok.o server.c -o server -g3
	$(CC) -L. -lssl -lcrypto -lpthread sok.o client.c -o client -g3

install:
	cp libsok.so /usr/lib
	cp sok.h /usr/include

clean:
	-rm sok.o
	-rm libsok.so
	-rm server
	-rm client

uninstall:
	-rm /usr/lib/libsok.so
	-rm /usr/include/sok.h

.PHONY: test all clean install uninstall
