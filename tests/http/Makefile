CC = cc

all:
	$(CC) -g3 -ansi -std=c11 -c ../../sok.c -o sok.o
	$(CC) -I../../ -lssl -lcrypto -lpthread sok.o server.c -o server -g3

clean:
	-rm sok.o
	-rm server
.PHONY: test all clean install uninstall
