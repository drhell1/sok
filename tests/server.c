#include <time.h>
#include <stdio.h>
#include "../sok.h"

void received(void *ptr, char *buffer, size_t len)
{
	printf("received from client: %s\n", buffer);
	char response[] = "Got your message\n";
	SSOK_Client_send(ptr, response, sizeof(response));
}

int main(int argc, char **argv)
{
	SSOK_Server *serv = SSOK_Server_new(5001, NULL, received, NULL);

	SSOK_Server_set_ssl_certificate(serv, "certificate.crt", "privateKey.key");
	SSOK_Server_run(serv);

	SSOK_Server_destroy(serv);
	return 0;
}
