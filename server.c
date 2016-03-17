#include <time.h>
#include <stdio.h>
#include "rfi.h"
#include "sok.h"

SERVER(R_Client, REMOTE_FUNC(print_time, int, int, int));

typedef struct
{
	R_Client *call;
	void *sok_data;
} Client;

void get_time(Client *cli)
{
	time_t epoch_time = time(NULL);
	struct tm *tm_p = localtime(&epoch_time);
	cli->call->print_time(cli->call, tm_p->tm_hour, tm_p->tm_min, tm_p->tm_sec);
}

void *Client_new(void *sok_data)
{
	Client *cli = malloc(sizeof(Client));
	cli->sok_data = sok_data;
	cli->call = R_Client_new(SOK_Server_send, sok_data);
	return (void*)cli;
}

void Client_received(void *ptr, char *buffer)
{
	Client *this = ptr;
	/* send func */
	RFI_called(this, buffer);
}

void Client_free(void *ptr)
{
	Client *this = ptr;
	/* cli destroy */
	R_Client_free(this->call);
	free(this);
}

HOST(SHARED_FUNC(get_time));

int main(int argc, char **argv)
{
	SOK_Server(5001, Client_new, Client_received, Client_free);
	return 0;
}
