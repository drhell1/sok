#include <time.h>
#include "sok.h"
#include "rfi.h"
#include <stdio.h>

SERVER(Server, REMOTE_FUNC(get_time));

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

int main(int argc, char **argv)
{
	int n;
	char buffer[256];
	int sockfd = SOK_Client_init("127.0.0.1", 5001);

	Server *serv = Server_new(sendfunc, &sockfd);
	while(1)
	{
		serv->get_time(serv);
		memset(buffer, 0, 256);
		n = read(sockfd, buffer, 256);
		if (n < 0)
		{
			perror("Cannot read from socket!\n");
			break;
		}
		else if (n == 0)
		{
			puts("Disconnected from server most like");
			break;
		}
		RFI_called(NULL, buffer);
	}

	Server_free(serv);
	close(sockfd);
	return 0;
}
