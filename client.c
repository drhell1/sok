#include <time.h>
#include <assert.h>
#include "sok.h"
#include <stdio.h>

void on_receive(void *data, char *buffer, size_t len)
{
	printf("received: %s\n", buffer);
}

int main(int argc, char **argv)
{
	SOK_Client *sok_client = SOK_Client_new("127.0.0.1", 5001, on_receive, NULL);

	if(!SOK_Client_connect(sok_client))
	{
		return 1;
	}

	char message[] = "Hello from client.\n";
	SOK_Client_send(sok_client, message, sizeof(message));

	SOK_Client_wait(sok_client);
	SOK_Client_destroy(sok_client);
	return 0;
}
