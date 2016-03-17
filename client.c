#include <time.h>
#include "sok.h"
#include "rfi.h"
#include <stdio.h>

SERVER(Server, REMOTE_FUNC(get_time));

typedef struct
{
	Server *call;
	void *sok_data;
} Server_t;

void print_time(void *data, int hour, int min, int sec )
{
	printf("\b\b\b\b\b\b\b\b%.2d:%.2d:%.2d", hour, min, sec);
	fflush(stdout);
}

void sendfunc(void *data, char *buffer)
{
	write(*(int*)data, buffer, 256);
}

HOST(SHARED_FUNC(print_time, int, int, int));

void *Server_t_new(void *sok_data)
{
	Server_t *serv = malloc(sizeof(Server_t));
	serv->sok_data = sok_data;
	serv->call = Server_new(sendfunc, serv->sok_data);
	return (void*)serv->call;
}

void Server_t_free(void *ptr)
{
	Server_t *this = ptr;
	Server_free(this->call);
}

void Server_t_request(void *ptr)
{
	Server *serv = ptr;
	serv->get_time(serv);
}

int main(int argc, char **argv)
{
	SOK_Client("127.0.0.1", 5001, Server_t_new, Server_t_request, RFI_called,
			Server_t_free);
	return 0;
}
