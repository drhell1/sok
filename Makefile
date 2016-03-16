CC = gcc

all:
	$(CC) server.c -lpthread -o server -g3
	$(CC) client.c -lpthread -o client -g3
