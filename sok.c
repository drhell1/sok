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

/* Client */

enum SOK_ERROR
{
	EINIT=-1
};

void ShowCerts(SSL* ssl)
{
	X509 *cert;
	char *line;

	cert = SSL_get_peer_certificate(ssl);	/* Get certificates (if available) */
	if ( cert != NULL )
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

typedef struct SOK_Client
{
	char *addr;
	int port;
	int sockfd;

	SSL *ssl;
	SSL_CTX *ssl_ctx;
	int use_ssl;

	pthread_t listen_thread;
	void(*cli_receive_callback)(void*,char*,size_t);
	void *data;
} SOK_Client;

static void SOK_Client_connect_socket(SOK_Client *this)
{
	struct hostent *host;
	struct sockaddr_in addr = {0};

	if ( (host = gethostbyname(this->addr)) == NULL )
	{
		perror(this->addr);
		abort();
	}
	this->sockfd = socket(PF_INET, SOCK_STREAM, 0);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(this->port);
	addr.sin_addr.s_addr = *(long*)(host->h_addr_list[0]);
	if ( connect(this->sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
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

static inline int SOK_Client_write(SOK_Client *this, void *buffer, size_t len)
{
	return this->use_ssl?SSL_write(this->ssl, buffer, len):write(this->sockfd, buffer, len);
}

static inline int SOK_Client_read(SOK_Client *this, void *buffer, size_t len)
{
	return this->use_ssl?SSL_read(this->ssl, buffer, len):read(this->sockfd, buffer, len);
}

void SOK_Client_send(void *data, char *buffer, size_t len)
{
	SOK_Client *client = (SOK_Client*)data;
	SOK_Client_write(client, &len, sizeof(size_t));
	SOK_Client_write(client, buffer, len);
}

void SOK_Client_use_ssl(SOK_Client *this)
{
	this->use_ssl = 1;
}

static inline int SOK_Client_receive(SOK_Client *this)
{
	int n;
	size_t len;
	n = SOK_Client_read(this, &len, sizeof(size_t));
	if (n < 0)
	{
		/* TODO: add error cannot read from socket */
		perror("cannot read from socket.\n");
		return 0;
	}
	else if (n == 0)
	{
		/* TODO: add msg disconnected from server */
		perror("disconnected from server.\n");
		return 0;
	}
	char *buffer = alloca(len);
	n = SOK_Client_read(this, buffer, len);
	/* TODO: add verification */
	this->cli_receive_callback(this->data, buffer, len);
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
	close(this->sockfd);
	SSL_free(this->ssl);
	SSL_CTX_free(this->ssl_ctx);
	free(this);
}

SOK_Client * SOK_Client_new(char *addr, int port,
		void(*cli_receive_callback)(void*,char*,size_t), void *data)
{
	SOK_Client *this = malloc(sizeof(SOK_Client));

	this->use_ssl = 0;
	this->ssl = NULL;
	this->ssl_ctx = NULL;

	this->cli_receive_callback = cli_receive_callback;
	this->data = data;

	this->addr = addr;
	this->port = port;

	return this;
}

static inline void SOK_Client_init_ssl_ctx(SOK_Client *this)
{
	const SSL_METHOD *method;
	SSL_library_init();
	OpenSSL_add_all_algorithms();		/* Load cryptos, et.al. */
	SSL_load_error_strings();			/* Bring in and register error messages */
	method = SSLv2_client_method();		/* Create new client-method instance */
	this->ssl_ctx = SSL_CTX_new(method);			/* Create new context */
	if ( this->ssl_ctx == NULL )
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
	int port;

	char *ssl_cert;
	char *ssl_key;
	SSL_CTX *ssl_ctx;

	/* Client Construction info */
	void*(*cli_init)(void*);
	void(*cli_receive_callback)(void*,char*,size_t);
	void(*cli_destroy)(void*);
	/* ------------------------ */

	struct SSOK_Client **clients;
	unsigned int clients_num;

	void *server_data;
} SSOK_Server;

struct SSOK_Client
{
	int sockfd;

	SSL *ssl;

	pthread_t thr;
	void *data;
	void(*receive_callback)(void*,char*,size_t);
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

static inline int SSOK_Client_read(struct SSOK_Client *this, void *ptr, size_t len)
{
	return this->ssl?SSL_read(this->ssl, ptr, len):read(this->sockfd, ptr, len);
}

static inline int SSOK_Client_write(struct SSOK_Client *this, void *ptr, size_t len)
{
	return this->ssl?SSL_write(this->ssl, ptr, len):write(this->sockfd, ptr, len);
}

void SSOK_Client_send(void *data, char *buffer, size_t len)
{
	struct SSOK_Client *serv_cli = data;
	SSOK_Client_write(serv_cli, &len, sizeof(size_t));
	SSOK_Client_write(serv_cli, buffer, len);
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
	SSL_free(this->ssl);
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
	SSL_CTX_free(this->ssl_ctx);
	close(this->sockfd);
	free(this);
}

void SSOK_Server_broadcast(SSOK_Server *this, char *message, size_t len,
		struct SSOK_Client *except)
{
	int i;
	for(i = 0; i < this->clients_num; i++)
	{
		struct SSOK_Client *client = this->clients[i];
		if(client != except)
		{
			SSOK_Client_send(client, message, len);
		}
	}
}

void * SSOK_Client_get_server_data(const struct SSOK_Client *this)
{
	return this->server;
}

static inline int SSOK_Client_receive(struct SSOK_Client *this)
{
	int n;
	size_t len;
	n = SSOK_Client_read(this, &len, sizeof(size_t));
	if (n < 0)
	{
		/* TODO: add error */
		SSOK_Client_destroy(this);
		return 0;
	}
	else if (n == 0)
	{
		SSOK_Client_destroy(this);
		return 0;
	}
	char *buffer = alloca(len);
	n = SSOK_Client_read(this, buffer, len);
	/* TODO: add verification */
	this->receive_callback(this->data, buffer, len);
	return 1;
}

static void * SSOK_client_thread(void *data)
{
	struct SSOK_Client *serv_cli = data;
	while(SSOK_Client_receive(serv_cli));
}

struct SSOK_Client * SSOK_Client_new(int cli_socket, void*(*cli_init)(void*),
		void(*cli_receive_callback)(void*,char*,size_t),
		void(*cli_destroy)(void*))
{
	struct SSOK_Client *this = malloc(sizeof(*this));
	this->ssl = NULL;
	this->sockfd = cli_socket;
	this->data = cli_init?cli_init(this):this;
	this->receive_callback = cli_receive_callback;
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
	if ( bind(this->sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
	{
		perror("can't bind port");
		abort();
	}
	if ( listen(this->sockfd, 10) != 0 )
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
	method = SSLv2_server_method();
	this->ssl_ctx = SSL_CTX_new(method);
	if (this->ssl_ctx == NULL)
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
	if ( SSL_CTX_use_certificate_file(this->ssl_ctx, this->ssl_cert, SSL_FILETYPE_PEM) <= 0 )
	{
		fprintf(stderr, "Cert file failed to load.\n");
		ERR_print_errors_fp(stderr);
		abort();
	}
	if ( SSL_CTX_use_PrivateKey_file(this->ssl_ctx, this->ssl_key, SSL_FILETYPE_PEM) <= 0 )
	{
		fprintf(stderr, "Key file failed to load.\n");
		ERR_print_errors_fp(stderr);
		abort();
	}
	/* verify private key */
	if ( !SSL_CTX_check_private_key(this->ssl_ctx) )
	{
		fprintf(stderr, "Private key does not match the public certificate\n");
		abort();
	}
}

void SSOK_Server_run(SSOK_Server *this)
{
	if(this->ssl_cert)
	{
		SSOK_Server_init_ssl_ctx(this);
		SSOK_Server_load_certificates(this);
	}

	SSOK_Server_bind(this);
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
		if(pthread_create(&serv_cli->thr, &thr_attr, SSOK_client_thread,
				(void*)serv_cli))
		{
			/* TODO: add error */
		}
	}
}

SSOK_Server * SSOK_Server_new(int port, void*(*cli_init)(void*),
		void(*cli_receive_callback)(void*,char*,size_t), void(*cli_destroy)(void*))
{
	SSOK_Server *this = malloc(sizeof(SSOK_Server));
	this->clients = NULL; this->clients_num = 0;
	this->port = port;

	this->ssl_ctx = NULL;
	this->ssl_cert = NULL;
	this->ssl_key = NULL;

	this->cli_init = cli_init;
	this->cli_receive_callback = cli_receive_callback;
	this->cli_destroy = cli_destroy;

	return this;
}

