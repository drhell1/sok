#include "sok.h"

#include <alloca.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <time.h>

/* Common */

static void SSL_show_cers(SSL* ssl)
{
	X509 *cert;
	char *line;

	cert = SSL_get_peer_certificate(ssl);
	if(cert != NULL)
	{
		printf("Server certificates:\n");
		line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
		printf("Subject: %s\n", line);
		free(line);
		line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
		printf("Issuer: %s\n", line);
		free(line);
		X509_free(cert);
	}
	else
		printf("No certificates.\n");
}

/* Client */

enum SOK_ERROR
{
	EINIT=-1
};


typedef struct
{
	char *buffer;
	size_t len;
	unsigned int request_id;
	SOK_Client *client;
} CallbackInfo;

typedef struct
{
	unsigned int id;
	pthread_mutex_t mutex;
	char **bytes;
	size_t *size;
} SOK_request;

typedef struct SOK_Client
{
	char *addr;
	int port;
	int sockfd;

	SSL *ssl;
	SSL_CTX *ssl_ctx;
	int use_ssl;

	SOK_request *requests;
	unsigned int requests_num;

	pthread_t listen_thread;
	sok_request_cb cli_request_callback;
	void *data;
	int request_async;
} SOK_Client;

static void SOK_Client_connect_socket(SOK_Client *this)
{
	struct hostent *host;
	struct sockaddr_in addr = {0};

	if((host = gethostbyname(this->addr)) == NULL)
	{
		perror(this->addr);
		abort();
	}
	this->sockfd = socket(PF_INET, SOCK_STREAM, 0);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(this->port);
	addr.sin_addr.s_addr = *(long*)(host->h_addr_list[0]);
	if(connect(this->sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
	{
		close(this->sockfd);
		perror(this->addr);
		abort();
	}

	if(this->use_ssl)
	{
		this->ssl = SSL_new(this->ssl_ctx);
		SSL_set_fd(this->ssl, this->sockfd);
	}
}

static inline int SOK_Client_write(SOK_Client *this, const void *buffer, size_t len)
{
	return this->use_ssl ? SSL_write(this->ssl, buffer, len)
			: write(this->sockfd, buffer, len);
}

static inline int SOK_Client_read(SOK_Client *this, void *buffer, size_t len)
{
	return this->use_ssl ? SSL_read(this->ssl, buffer, len)
			: read(this->sockfd, buffer, len);
}

void SOK_Client_send(SOK_Client *this, char *buffer, size_t len)
{
	const char code = 'n';
	SOK_Client_write(this, &code, sizeof(code));
	SOK_Client_write(this, &len, sizeof(len));
	SOK_Client_write(this, buffer, len);
}

void SOK_Client_use_ssl(SOK_Client *this)
{
	this->use_ssl = 1;
}

char *SOK_Client_request(SOK_Client *this, char *buffer, size_t len,
		size_t *result_len)
{
	int i, n = this->requests_num;
	SOK_request *request = NULL;
	char *result = NULL;

	for(i = 0; i < n; i++)
	{
		if(this->requests[i].bytes == NULL)
		{
			request = &this->requests[i];
		}
	}
	if(request == NULL)
	{
		this->requests = realloc(this->requests, sizeof(SOK_request) * n);
		this->requests_num++;
		request = &this->requests[n];
	}
	request->size = result_len;
	request->bytes = &result;
	request->id = n;
	pthread_mutex_init(&request->mutex, NULL);
	pthread_mutex_lock(&request->mutex);

	const char code = 'R';
	SOK_Client_write(this, &code, sizeof(code));
	SOK_Client_write(this, &request->id, sizeof(request->id));
	SOK_Client_write(this, &len, sizeof(len));
	SOK_Client_write(this, buffer, len);

	pthread_mutex_lock(&request->mutex);

	request->bytes = NULL;
	pthread_mutex_destroy(&request->mutex);

	return result;
}

void SOK_Client_return_request(SOK_Client *this, unsigned int id, size_t len, char *buffer)
{
	int i = 0;
	for(i = 0; i < this->requests_num; i++)
	{
		if(this->requests[i].id == id)
		{
			SOK_request *request = &this->requests[i];
			if(request->size)
			{
				(*request->bytes) = malloc(len);
				memcpy(*request->bytes, buffer, len);
				*request->size = len;
			}
			pthread_mutex_unlock(&request->mutex);
			return;
		}
	}

}

static void *CallbackInfo_call(CallbackInfo *this)
{
	size_t result_size = 0;
	SOK_Client *client = this->client;

	if(this->request_id == -1)
	{
		client->cli_request_callback(client->data, this->buffer, this->len,
				&result_size);
	}
	else
	{
		const char code = 'r';

		char *result_buffer = client->cli_request_callback(client->data,
				this->buffer, this->len, &result_size);

		SOK_Client_write(client, &code, sizeof(code)); /* 'r' */
		SOK_Client_write(client, &this->request_id,
				sizeof(this->request_id));
		SOK_Client_write(client, &result_size, sizeof(result_size));
		SOK_Client_write(client, result_buffer, result_size);
	}

	free(this->buffer);
	free(this);
}

void SOK_Client_call_callback(SOK_Client *this, char *buf, size_t len,
		unsigned int req)
{
	CallbackInfo *cbi = malloc(sizeof(*cbi));
	cbi->buffer = buf;
	cbi->len = len;
	cbi->request_id = req;
	cbi->client = this;

	if(this->request_async)
	{
		pthread_t thr;
		pthread_create(&thr, NULL, (sok_thread_cb)CallbackInfo_call, cbi);
	}
	else
	{
		CallbackInfo_call(cbi);
	}
}

static inline int SOK_Client_receive(SOK_Client *this)
{
	int n;
	size_t len;
	char type;

	n = SOK_Client_read(this, &type, sizeof(type));

	if (n < 0)
	{
		/* TODO: add error cannot read from socket */
		perror("Cannot read from socket.\n");
		return 0;
	}
	else if (n == 0)
	{
		/* TODO: add msg disconnected from server */
		perror("Disconnected from server.\n");
		return 0;
	}

	if(type == 'R') /* REQUEST */
	{
		size_t result_size;
		unsigned int request_id;
		SOK_Client_read(this, &request_id, sizeof(request_id));

		SOK_Client_read(this, &len, sizeof(size_t));
		char *buffer = malloc(len);
		SOK_Client_read(this, buffer, len);

		SOK_Client_call_callback(this, buffer, len, request_id);

	}
	else if(type == 'r') /* REQUEST RETURN */
	{
		unsigned int request_id;

		SOK_Client_read(this, &request_id, sizeof(request_id));

		SOK_Client_read(this, &len, sizeof(size_t));
		char *buffer = alloca(len);
		SOK_Client_read(this, buffer, len);

		SOK_Client_return_request(this, request_id, len, buffer);
	}
	else
	{
		size_t result_size;

		SOK_Client_read(this, &len, sizeof(size_t));
		char *buffer = alloca(len);
		SOK_Client_read(this, buffer, len);

		SOK_Client_call_callback(this, buffer, len, -1);
		this->cli_request_callback(this->data, buffer, len, &result_size);
	}

	return 1;
}

static void * SOK_Client_main(void *data)
{
	SOK_Client *this = (SOK_Client*)data;
	while(SOK_Client_receive(this));
	return NULL;
}

void SOK_Client_destroy(SOK_Client *this)
{
	unsigned int i;
	for(i = 0; i < this->requests_num; i++)
	{
		SOK_request *request = &this->requests[i];
		if(request->bytes)
		{
			/* fprinf(stderr, "Exiting with requests pending.\n"); */
			exit(1);
		}
	}
	free(this->requests);
	close(this->sockfd);
	SSL_free(this->ssl);
	SSL_CTX_free(this->ssl_ctx);
	free(this);
}

SOK_Client * SOK_Client_new(char *addr, int port,
		sok_request_cb cli_request_callback, void *data, int async)
{
	SOK_Client *this = malloc(sizeof(SOK_Client));

	this->use_ssl = 0;
	this->ssl = NULL;
	this->ssl_ctx = NULL;

	this->requests = NULL;
	this->requests_num = 0;

	this->cli_request_callback = cli_request_callback;
	this->data = data;
	this->request_async = async;

	this->addr = addr;
	this->port = port;

	return this;
}

static inline void SOK_Client_init_ssl_ctx(SOK_Client *this)
{
	const SSL_METHOD *method;
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
	method = TLSv1_2_client_method();
	this->ssl_ctx = SSL_CTX_new(method);
	if(this->ssl_ctx == NULL)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}
}

int SOK_Client_connect(SOK_Client *this)
{
	/* Connecting to server */

	if(this->use_ssl)
	{
		SOK_Client_init_ssl_ctx(this);
	}

	SOK_Client_connect_socket(this);


	if(this->use_ssl && SSL_connect(this->ssl) == -1)
	{
		ERR_print_errors_fp(stderr);
	}
	/* -------------------- */

	/* Starting listen loop thread */
	if(pthread_create(&this->listen_thread, NULL, (sok_thread_cb)SOK_Client_main,
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

typedef struct SSOK_Server
{
	int sockfd;
	int port;
	int closing;

	pthread_t thr;

	char *ssl_cert;
	char *ssl_key;
	SSL_CTX *ssl_ctx;

	/* Client Construction info */
	sok_cli_init_cb cli_init;
	sok_request_cb cli_request_callback;
	sok_cli_destroy_cb cli_destroy;
	/* ------------------------ */

	SSOK_Client **clients;
	unsigned int clients_num;

	void *server_data;
} SSOK_Server;

typedef struct SSOK_Client
{
	int sockfd;

	SSL *ssl;

	unsigned int requests_num;
	SOK_request *requests;

	pthread_t thr;
	void *data;
	sok_request_cb request_callback;
	sok_cli_destroy_cb destroy_callback;
	SSOK_Server *server;
} SSOK_Client;

static inline unsigned long upo2(unsigned long v) /* upperpower of 2 */
{
	v--;
	v |= v >> 1; v |= v >> 2;
	v |= v >> 4; v |= v >> 8;
	v |= v >> 16; v++;
	return v;
}

static inline void SSOK_Server_add_client(SSOK_Server *this,
		SSOK_Client *client)
{
	/* TODO: LOCK */

	int n0 = upo2(this->clients_num);
	int n1 = upo2(this->clients_num + 1);
	if(n0 < n1)
	{
		this->clients = realloc(this->clients, n1 * sizeof(SSOK_Client*));
	}

	this->clients[this->clients_num] = client;
	this->clients_num++;

	/* !LOCK */
}

static inline int SSOK_Client_read(SSOK_Client *this, void *ptr, size_t len)
{
	return this->ssl ? SSL_read(this->ssl, ptr, len)
			: read(this->sockfd, ptr, len);
}

static inline int SSOK_Client_write(SSOK_Client *this, const void *ptr, size_t len)
{
	return this->ssl ? SSL_write(this->ssl, ptr, len)
			: write(this->sockfd, ptr, len);
}

void SSOK_Client_send_http(SSOK_Client *this, char *buffer, size_t len)
{
	char time_buffer[500];
	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	strftime(time_buffer, sizeof(time_buffer), "%a, %d %b %Y %H:%M:%S %Z", &tm);

	const char format[] = "HTTP/1.1 200 OK\n"
		"Date: %s\n"
		"Server: Sok\n"
		"Content-Type: text/html\n"
		"Content-Length: %lu\n"
		"Accept-Ranges: bytes\n"
		"Connection: close\n"
		"\n";
	char *format_result = alloca(
		sizeof(format) +
		200 /* size of time */ +
		16 /* size of length chars */
	);
	size_t len2 = sprintf(format_result, format, time_buffer, len);

	SSOK_Client_write(this, format_result, len2);
	SSOK_Client_write(this, buffer, len);
}

void SSOK_Client_send(SSOK_Client *this, char *buffer, size_t len)
{
	const char code = 'n';
	SSOK_Client_write(this, &code, sizeof(code));
	SSOK_Client_write(this, &len, sizeof(len));
	SSOK_Client_write(this, buffer, len);
}

static inline void SSOK_Client_join(SSOK_Client *this)
{
	pthread_join(this->thr, NULL);
}

static inline void SSOK_Client_disconnect(SSOK_Client *this)
{
	close(this->sockfd);
}

static inline void SSOK_Client_destroy(SSOK_Client *this)
{
	close(this->sockfd);
	if(this->destroy_callback)
	{
		this->destroy_callback(this->data);
	}
	SSL_free(this->ssl);
	free(this);
}

void SSOK_Server_destroy(SSOK_Server *this)
{
	int i;
	for(i = 0; i < this->clients_num; i++)
	{
		SSOK_Client *client = this->clients[i];

		SSOK_Client_disconnect(client);
	}

	for(i = 0; i < this->clients_num; i++)
	{
		SSOK_Client *client = this->clients[i];

		SSOK_Client_join(client);
		SSOK_Client_destroy(client);
	}
	this->closing = 1;
	close(this->sockfd);
	pthread_join(this->thr, NULL);
	SSL_CTX_free(this->ssl_ctx);
	free(this);
}

void SSOK_Server_broadcast(SSOK_Server *this, char *message, size_t len,
		SSOK_Client *except)
{
	int i;
	for(i = 0; i < this->clients_num; i++)
	{
		SSOK_Client *client = this->clients[i];
		if(client != except)
		{
			SSOK_Client_send(client, message, len);
		}
	}
}

void * SSOK_Client_get_server_data(const SSOK_Client *this)
{
	return this->server;
}

void SSOK_Client_return_request(SSOK_Client *this, unsigned int id,
		size_t len, char *buffer)
{
	int i = 0;
	for(i = 0; i < this->requests_num; i++)
	{
		if(this->requests[i].id == id)
		{
			SOK_request *request = &this->requests[i];
			memcpy(request->bytes, buffer, len);
			*request->size = len;
			pthread_mutex_unlock(&request->mutex);
			return;
		}
	}

}

static inline int SSOK_Client_receive(SSOK_Client *this)
{
	int n;
	size_t len;
	char type = '\0';

	n = SSOK_Client_read(this, &type, sizeof(type));
	if(n < 0)
	{
		/* TODO: add error */
		SSOK_Client_destroy(this);
		return 0;
	}
	else if(n == 0)
	{
		SSOK_Client_destroy(this);
		return 0;
	}

	if(type == 'R') /* REQUEST */
	{
		size_t result_size;
		unsigned int request_id;
		SSOK_Client_read(this, &request_id, sizeof(request_id));

		SSOK_Client_read(this, &len, sizeof(size_t));
		char *buffer = alloca(len);
		SSOK_Client_read(this, buffer, len);

		const char code = 'r';
		char *result_buffer =
			this->request_callback(this->data, buffer, len, &result_size);

		SSOK_Client_write(this, &code, sizeof(code)); /* 'r' */
		SSOK_Client_write(this, &request_id, sizeof(request_id));
		SSOK_Client_write(this, &result_size, sizeof(result_size));
		SSOK_Client_write(this, result_buffer, result_size);
	}
	else if(type == 'r') /* REQUEST RETURN */
	{
		unsigned int request_id;

		SSOK_Client_read(this, &request_id, sizeof(request_id));

		SSOK_Client_read(this, &len, sizeof(size_t));
		char *buffer = alloca(len);
		SSOK_Client_read(this, buffer, len);

		SSOK_Client_return_request(this, request_id, len, buffer);
	}
	else if(type == 'n')
	{
		size_t result_size;

		SSOK_Client_read(this, &len, sizeof(size_t));
		char *buffer = alloca(len);
		SSOK_Client_read(this, buffer, len);

		this->request_callback(this->data, buffer, len, &result_size);
	}
	else if(type == 'G')
	{
		char buffer[1001];
		buffer[0] = type;
		SSOK_Client_read(this, buffer+1, 1000);
		printf("maybe its http: %s\n", buffer);
		size_t res_size = 0;
		char *res_buffer = this->request_callback(this->data, buffer, 1000, &res_size);
		SSOK_Client_send_http(this, res_buffer, res_size);
	}
	else
	{
		char buffer[1000];
		SSOK_Client_read(this, buffer, 1000);
		printf("Corrupted message: %c%s\n", type, buffer);
		exit(1);
	}

	return 1;
}

static void * SSOK_client_thread(SSOK_Client *this)
{
	while(SSOK_Client_receive(this));
}

SSOK_Client * SSOK_Client_new(int cli_socket, sok_cli_init_cb cli_init,
		sok_request_cb cli_request_callback, sok_cli_destroy_cb cli_destroy)
{
	SSOK_Client *this = malloc(sizeof(*this));
	this->ssl = NULL;
	this->requests = NULL;
	this->requests_num = 0;
	this->sockfd = cli_socket;
	this->data = cli_init?cli_init(this):this;
	this->request_callback = cli_request_callback;
	this->destroy_callback = cli_destroy;
	return this;
}

static void SSOK_Server_bind(SSOK_Server *this)
{
	struct sockaddr_in addr = {0};

	this->sockfd = socket(PF_INET, SOCK_STREAM, 0);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(this->port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(this->sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
	{
		perror("Can't bind port");
		abort();
	}
	if(listen(this->sockfd, 10) != 0)
	{
		perror("Can't configure listening port");
		abort();
	}
}

static inline void SSOK_Server_init_ssl_ctx(SSOK_Server *this)
{
	const SSL_METHOD *method;
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();
	method = TLSv1_2_server_method();
	this->ssl_ctx = SSL_CTX_new(method);
	if(this->ssl_ctx == NULL)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}
}

void SSOK_Server_set_ssl_certificate(SSOK_Server *this, char *cert, char *key)
{
	this->ssl_cert = cert;
	this->ssl_key = key;

}

void SSOK_Server_load_certificates(SSOK_Server *this)
{
	if(SSL_CTX_use_certificate_file(this->ssl_ctx, this->ssl_cert,
				SSL_FILETYPE_PEM) <= 0)
	{
		fprintf(stderr, "Cert file failed to load.\n");
		ERR_print_errors_fp(stderr);
		abort();
	}
	if(SSL_CTX_use_PrivateKey_file(this->ssl_ctx, this->ssl_key,
				SSL_FILETYPE_PEM) <= 0)
	{
		fprintf(stderr, "Key file failed to load.\n");
		ERR_print_errors_fp(stderr);
		abort();
	}
	/* verify private key */
	if(!SSL_CTX_check_private_key(this->ssl_ctx))
	{
		fprintf(stderr, "Private key does not match the public certificate\n");
		abort();
	}
}

static void * SSOK_Server_main(SSOK_Server *this)
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
			if(this->closing)
			{
				return NULL;
			}
			/* TODO: add error */
			continue;
		}
		SSOK_Client *serv_cli = SSOK_Client_new(cli_socket,
				this->cli_init, this->cli_request_callback, this->cli_destroy);

		if(this->ssl_ctx)
		{
			serv_cli->ssl = SSL_new(this->ssl_ctx);
			SSL_set_fd(serv_cli->ssl, cli_socket);

			if(SSL_accept(serv_cli->ssl) == -1 )
			{
				ERR_print_errors_fp(stderr);
			}

		}

		SSOK_Server_add_client(this, serv_cli);

		pthread_attr_t thr_attr;
		pthread_attr_init(&thr_attr);
		pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_DETACHED);
		if(pthread_create(&serv_cli->thr, &thr_attr, (sok_thread_cb)SSOK_client_thread,
				(void*)serv_cli))
		{
			/* TODO: add error */
		}
	}
	return NULL;

}

void SSOK_Server_run(SSOK_Server *this, int async)
{
	if(this->ssl_cert)
	{
		SSOK_Server_init_ssl_ctx(this);
		SSOK_Server_load_certificates(this);
	}

	this->closing = 0;

	SSOK_Server_bind(this);

	if(pthread_create(&this->thr, NULL, (sok_thread_cb)SSOK_Server_main,
				(void*)this))
	{
		/* TODO: add error */
	}
	if(!async)
	{
		pthread_join(this->thr, NULL);
	}
}

SSOK_Server * SSOK_Server_new(int port, sok_cli_init_cb cli_init,
		sok_request_cb cli_request_callback, sok_cli_destroy_cb cli_destroy)
{
	SSOK_Server *this = malloc(sizeof(SSOK_Server));
	this->clients = NULL; this->clients_num = 0;
	this->port = port;

	this->ssl_ctx = NULL;
	this->ssl_cert = NULL;
	this->ssl_key = NULL;

	this->cli_init = cli_init;
	this->cli_request_callback = cli_request_callback;
	this->cli_destroy = cli_destroy;

	return this;
}

