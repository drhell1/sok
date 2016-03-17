#ifndef SOCK_H
#define SOCK_H

#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

enum SOK_ERROR
{
	EINIT=-1
};

#ifndef BUFFER_SIZE
	#define BUFFER_SIZE 256
#endif

#ifndef SOCKET_TYPE
	#define SOCKET_TYPE SOCK_STREAM
#endif

#ifndef CONN_TYPE
	#define CONN_TYPE AF_INET
#endif

struct SOK_Server_Client
{
	int sockfd;
	pthread_t thr;
	void *data;
	void(*receive_callback)(void*,char*);
	void(*destroy_callback)(void*);
};

struct SOK_Server
{
	int sockfd;
	void*(*cli_init)(void*);
	void(*cli_receive_callback)(void*,char*);
	void(*cli_destroy)(void*);
};

static inline int SOK_Client_init(char *addr, int port)
{
	struct sockaddr_in serv_addr = {0};
	struct hostent *host;
	int sockfd = socket(CONN_TYPE, SOCKET_TYPE, 0);
	host = gethostbyname(addr);
	serv_addr.sin_family = CONN_TYPE;
	memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)host->h_addr,
			host->h_length);
	serv_addr.sin_port = htons(port);
	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		return EINIT;
	}
	return sockfd;
}

static inline int SOK_Server_init(int port)
{
	struct sockaddr_in addr;
	memset((char *)&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = CONN_TYPE;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);
	int sockfd = socket(CONN_TYPE, SOCKET_TYPE, 0);
	if(bind(sockfd, (struct sockaddr*)&addr, sizeof(typeof(addr))) < 0)
	{
		return EINIT;
	}
	return sockfd;
}

static void SOK_Server_send(void* data, char* buffer)
{
	struct SOK_Server_Client *cli = data;
	write(cli->sockfd, buffer, BUFFER_SIZE);
}

static inline void SOK_Server_destroy(struct SOK_Server *serv)
{
	close(serv->sockfd);
	free(serv);
}

static void * SOK_Server_client_thread(void *data)
{
	struct SOK_Server_Client *cli = data;
	char buffer[BUFFER_SIZE];
	while(1)
	{
		memset(buffer, 0, BUFFER_SIZE);
		int n = read(cli->sockfd, buffer, BUFFER_SIZE);
		if (n < 0)
		{
			/* TODO: add error */
			close(cli->sockfd);
			if(cli->destroy_callback)
			{
				cli->destroy_callback(cli);
			}
			free(cli);
			return NULL;
		}
		else if (n == 0)
		{
			close(cli->sockfd);
			if(cli->destroy_callback)
			{
				cli->destroy_callback(cli);
			}
			free(cli);
			return NULL;
		}
		cli->receive_callback(cli->data, buffer);
	}
}

static inline void SOK_Server_main(struct SOK_Server *serv)
{
	char buffer[BUFFER_SIZE];
	while(1)
	{
		listen(serv->sockfd, 5);
		struct sockaddr_in client_addr;
		socklen_t cli_len = sizeof(struct sockaddr_in);
		int cli_socket = accept(serv->sockfd, (struct sockaddr*)&client_addr,
				&cli_len);
		if(cli_socket < 0)
		{
			/* TODO: add error */
			continue;
		}
		struct SOK_Server_Client *client = malloc(sizeof(struct
				SOK_Server_Client));

		client->data = serv->cli_init(client);
		client->sockfd = cli_socket;
		client->receive_callback = serv->cli_receive_callback;
		client->destroy_callback = serv->cli_destroy;

		pthread_attr_t thr_attr;
		pthread_attr_init(&thr_attr);
		pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED);
		if(pthread_create(&client->thr, &thr_attr, SOK_Server_client_thread,
				(void*)client))
		{
			/* TODO: add error */
		}
	}
}

static inline void SOK_Server(int port, void*(*cli_init)(void*),
		void(*cli_receive_callback)(void*,char*), void(*cli_destroy)(void*))
{
	struct SOK_Server *serv = malloc(sizeof(struct SOK_Server));
	serv->sockfd = SOK_Server_init(port);
	serv->cli_init = cli_init;
	serv->cli_receive_callback = cli_receive_callback;
	serv->cli_destroy = cli_destroy;
	SOK_Server_main(serv);
	SOK_Server_destroy(serv);
}
#endif /* SOCK_H */
