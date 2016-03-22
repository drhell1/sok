#include <time.h>
#include <assert.h>
#include "sok.h"
#include "rfi.h"
#include <stdio.h>

REMOTE(Server, REMOTE_FUNC(get_time));

void print_time(void *data, int hour, int min, int sec )
{
	printf("\b\b\b\b\b\b\b\b%.2d:%.2d:%.2d", hour, min, sec);
	fflush(stdout);
}

HOST(SHARED_FUNC(print_time, int, int, int));

void on_receive(void *data, char *buffer)
{
	Server *serv = (Server*)data;
	RFI_called(serv, buffer);
	serv->get_time(serv);
}

int main(int argc, char **argv)
{
	Server *serv = Server_new(SOK_Client_send, NULL);
	SOK_Client *sok_client = SOK_Client_init("127.0.0.1", 5001, on_receive, serv);
	assert(sok_client);

	serv->send_data = sok_client;
	serv->get_time(serv);

	while(1);

	SOK_Client_destroy(sok_client);
	Server_free(serv);
	return 0;
}
