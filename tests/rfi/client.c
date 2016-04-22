#include <sok.h>
#include <stdlib.h>
#include <stdio.h>
#include "rfi.h"

REMOTE(Remote,
	REMOTE_FUNC(set_name, char*),
	REMOTE_FUNC(add, int, int)
);

int main(int argc, char **argv)
{
	size_t name_size;
	int a, b;
	char *name;

	Remote *remote = Remote_new(SOK_Client_send, NULL);
	SOK_Client *sok = SOK_Client_new("127.0.0.1", 5008, NULL, remote);
	remote->send_data = sok;

	SOK_Client_connect(sok);

	printf("Enter name: ");
	getline(&name, &name_size, stdin);

	remote->set_name(remote, name);

	printf("Enter 2 numbers: ");
	scanf("%d %d", &a, &b);

	remote->add(remote, a, b);

	SOK_Client_wait(sok);
	return 0;
}
