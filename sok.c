#include "sok.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

/* Client */

enum SOK_ERROR
{
	EINIT=-1
};

typedef struct SOK_Client
{
	char *addr;
	int port;
	int sockfd;

	pthread_t listen_thread;
	void(*cli_receive_callback)(void*,char*);
	void *data;
} SOK_Client;

void SOK_Client_send(void *data, char *buffer)
{
	SOK_Client *client = (SOK_Client*)data;
	write(client->sockfd, buffer, SOK_BUFFER_SIZE);
}

static void * SOK_Client_main(void *data)
{
	SOK_Client *client = (SOK_Client*)data;
	int n;
	char buffer[SOK_BUFFER_SIZE];
	while(1)
	{
		memset(buffer, 0, SOK_BUFFER_SIZE);
		n = read(client->sockfd, buffer, SOK_BUFFER_SIZE);
		if (n < 0)
		{
			/* TODO: add error cannot read from socket */
			break;
		}
		else if (n == 0)
		{
			/* TODO: add msg disconnected from server */
			break;
		}
		client->cli_receive_callback(client->data, buffer);
	}
	return NULL;
}

void SOK_Client_destroy(SOK_Client *client)
{
	close(client->sockfd);
	free(client);
}

SOK_Client * SOK_Client_new(char *addr, int port,
		void(*cli_receive_callback)(void*,char*), void *data)
{
	SOK_Client *this = malloc(sizeof(SOK_Client));

	this->cli_receive_callback = cli_receive_callback;
	this->data = data;

	this->addr = addr;
	this->port = port;

	return this;
}

int SOK_Client_connect(SOK_Client *this)
{
	/* Connecting to server */

	struct sockaddr_in serv_addr = {0};
	struct hostent *host;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	host = gethostbyname(this->addr);
	serv_addr.sin_family = AF_INET;
	memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)host->h_addr_list[0],
			host->h_length);
	serv_addr.sin_port = htons(this->port);
	if(connect(sockfd, (struct sockaddr*)&serv_addr,
				sizeof(serv_addr)) < 0)
	{
		return 0;
	}
	this->sockfd = sockfd;

	/* -------------------- */

	/* Starting listen loop thread */
	if(pthread_create(&this->listen_thread, NULL, SOK_Client_main,
			(void*)this))
	{
		/* TODO: add error */
		return 0;
	}
	/* ---------------------------- */

	return 1;
}

void SOK_Client_set_send_data(SOK_Client *this, void *data)
{
	this->data = data;
}

void SOK_Client_wait(SOK_Client *this)
{
	pthread_join(this->listen_thread, NULL);
}

/* Server */

struct SSOK_Client;

typedef struct SSOK_Server
{
	int sockfd;

	/* Client Construction info */
	void*(*cli_init)(void*);
	void(*cli_receive_callback)(void*,char*);
	void(*cli_destroy)(void*);
	/* ------------------------ */

	struct SSOK_Client **clients;
	unsigned int clients_num;

	void *server_data;
} SSOK_Server;

struct SSOK_Client
{
	int sockfd;
	pthread_t thr;
	void *data;
	void(*receive_callback)(void*,char*);
	void(*destroy_callback)(void*);
	SSOK_Server *server;
};

static inline unsigned long upo2(unsigned long v) /* upperpower of 2 */
{
	v--;
	v |= v >> 1; v |= v >> 2;
	v |= v >> 4; v |= v >> 8;
	v |= v >> 16; v++;
	return v;
}

static inline void SSOK_Server_add_client(SSOK_Server *this,
		struct SSOK_Client *client)
{
	/* TODO: LOCK */

	int n0 = upo2(this->clients_num);
	int n1 = upo2(this->clients_num + 1);
	if(n0 < n1)
	{
		this->clients = realloc(this->clients, n1 * sizeof(struct SSOK_Client*));
	}

	this->clients[this->clients_num] = client;
	this->clients_num++;

	/* !LOCK */
}

static inline int SSOK_Server_bind(int port)
{
	struct sockaddr_in addr;
	memset((char *)&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		return EINIT;
	}
	return sockfd;
}

void SSOK_Client_send(void *data, char *buffer)
{
	struct SSOK_Client *serv_cli = data;
	write(serv_cli->sockfd, buffer, SOK_BUFFER_SIZE);
}

static inline void SSOK_Client_join(struct SSOK_Client *this)
{
	pthread_join(this->thr, NULL);
}

static inline void SSOK_Client_disconnect(struct SSOK_Client *this)
{
	close(this->sockfd);
}

static inline void SSOK_Client_destroy(struct SSOK_Client *this)
{
	close(this->sockfd);
	if(this->destroy_callback)
	{
		this->destroy_callback(this->data);
	}
	free(this);
}

void SSOK_Server_destroy(SSOK_Server *this)
{
	int i;
	for(i = 0; i < this->clients_num; i++)
	{
		struct SSOK_Client *client = this->clients[i];

		SSOK_Client_disconnect(client);
	}

	for(i = 0; i < this->clients_num; i++)
	{
		struct SSOK_Client *client = this->clients[i];

		SSOK_Client_join(client);
		SSOK_Client_destroy(client);
	}
	close(this->sockfd);
	free(this);
}

void SSOK_Server_broadcast(SSOK_Server *this, char *message,
		struct SSOK_Client *except)
{
	int i;
	for(i = 0; i < this->clients_num; i++)
	{
		struct SSOK_Client *client = this->clients[i];
		if(client != except)
		{
			SSOK_Client_send(client, message);
		}
	}
}

void * SSOK_Client_get_server_data(const struct SSOK_Client *this)
{
	return this->server;
}

static void * SSOK_client_thread(void *data)
{
	struct SSOK_Client *serv_cli = data;
	char buffer[SOK_BUFFER_SIZE];
	while(1)
	{
		memset(buffer, 0, SOK_BUFFER_SIZE);
		int n = read(serv_cli->sockfd, buffer, SOK_BUFFER_SIZE);
		if (n < 0)
		{
			/* TODO: add error */
			SSOK_Client_destroy(serv_cli);
			return NULL;
		}
		else if (n == 0)
		{
			SSOK_Client_destroy(serv_cli);
			return NULL;
		}
		serv_cli->receive_callback(serv_cli->data, buffer);
	}
}

struct SSOK_Client * SSOK_Client_new(int cli_socket, void*(*cli_init)(void*),
		void(*cli_receive_callback)(void*,char*), void(*cli_destroy)(void*))
{
	struct SSOK_Client *this = malloc(sizeof(*this));
	this->sockfd = cli_socket;
	this->data = cli_init(this);
	this->receive_callback = cli_receive_callback;
	this->destroy_callback = cli_destroy;
	return this;
}

void SSOK_Server_run(SSOK_Server *this)
{
	while(1)
	{
		listen(this->sockfd, 5);
		struct sockaddr_in client_addr;
		socklen_t cli_len = sizeof(struct sockaddr_in);
		int cli_socket = accept(this->sockfd, (struct sockaddr*)&client_addr,
				&cli_len);
		if(cli_socket < 0)
		{
			/* TODO: add error */
			continue;
		}

		struct SSOK_Client *serv_cli = SSOK_Client_new(cli_socket,
				this->cli_init, this->cli_receive_callback, this->cli_destroy);

		SSOK_Server_add_client(this, serv_cli);

		pthread_attr_t thr_attr;
		pthread_attr_init(&thr_attr);
		pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED);
		if(pthread_create(&serv_cli->thr, &thr_attr, SSOK_client_thread,
				(void*)serv_cli))
		{
			/* TODO: add error */
		}
	}
}

SSOK_Server * SSOK_Server_new(int port, void*(*cli_init)(void*),
		void(*cli_receive_callback)(void*,char*), void(*cli_destroy)(void*))
{
	SSOK_Server *this = malloc(sizeof(SSOK_Server));
	this->clients = NULL; this->clients_num = 0;
	this->sockfd = SSOK_Server_bind(port);

	this->cli_init = cli_init;
	this->cli_receive_callback = cli_receive_callback;
	this->cli_destroy = cli_destroy;

	return this;
}

