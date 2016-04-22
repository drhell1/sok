#include "rfi.h"
#include <sok.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct
{
	char *name;
	void *sok;
} Client;

void set_name(void *data, char *name)
{
	printf("name: %s\n", name);
	((Client*)data)->name = strdup(name);
}

void add(void *data, int a, int b)
{
	printf("%d + %d = %d\n", a, b, a + b);
}

LOCAL(Local,
	SHARED_FUNC(set_name, char*),
	SHARED_FUNC(add, int, int)
);

void * init(void *ptr)
{
	Client *this = calloc(1, sizeof(*this));
	this->sok = ptr;
	return this;
}

void des(void *ptr)
{
	free(ptr);
}

int main(int argc, char **argv)
{
	SSOK_Server *serv = SSOK_Server_new(5008, init, Local_called, des);
	SSOK_Server_run(serv, 0);
	SSOK_Server_destroy(serv);
	return 0;
}
