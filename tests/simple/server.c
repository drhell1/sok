#include <time.h>
#include <stdio.h>
#include <string.h>
#include "../../sok.h"

char *received(void *ptr, char *buffer, size_t len, size_t *res_size)
{
	printf("received from client: %s\n", buffer);

	char response[] = "response";
	*res_size = sizeof(response);
	return strdup(response);
}

int main(int argc, char **argv)
{
	SSOK_Server *serv = SSOK_Server_new(5001, NULL, received, NULL);

	/* SSOK_Server_set_ssl_certificate(serv, "certificate.crt", "privateKey.key"); */
	SSOK_Server_run(serv, 0);

	SSOK_Server_destroy(serv);
	return 0;
}
