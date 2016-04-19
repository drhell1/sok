#include <time.h>
#include <stdio.h>
#include <string.h>
#include "../../sok.h"

void *init(void *sok)
{
	printf("new connection\n");
}

void destroy(void *ptr)
{
	printf("connection ended\n");
}

char *received(void *ptr, char *buffer, size_t len, size_t *res_size)
{
	char response[] = "kek";
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
