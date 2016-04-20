CC = cc

all:
	$(CC) -g3 -ansi -std=c11 -c ../../sok.c -o sok.o
	$(CC) -lssl -lcrypto -lpthread sok.o server.c -o server -g3
	$(CC) -lssl -lcrypto -lpthread sok.o client.c -o client -g3

clean:
	-rm sok.o
	-rm server
	-rm client
.PHONY: test all clean install uninstall
