#include <time.h>
#include <assert.h>
#include "../../sok.h"
#include <stdio.h>
#include <stdlib.h>

char *on_receive(void *data, char *buffer, size_t len, size_t *res_size)
{
	printf("received: %s\n", buffer);
	*res_size = 0;
	return NULL;
}

int main(int argc, char **argv)
{
	SOK_Client *sok_client = SOK_Client_new("127.0.0.1", 5001, on_receive, NULL, 1);

	/* SOK_Client_use_ssl(sok_client); */

	if(!SOK_Client_connect(sok_client))
	{
		return 1;
	}

	SOK_Client_send(sok_client, "kek", sizeof("kek"));

	char message[] = "Hello from client.";
	size_t s;
	char *response = SOK_Client_request(sok_client, message, sizeof(message), &s);
	printf("%s\n", response);
	free(response);

	SOK_Client_wait(sok_client);
	SOK_Client_destroy(sok_client);
	return 0;
}
