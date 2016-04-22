/**
 *@file sok.h
 *@author José Monteiro
 *@author João Silva
 *@date 12 Apr 2016
 *
 * @brief Sok - Sockets Wrapper
 *
 * Sok is a wrapper to server and client socket communication, including ssl
 * protocol
 */

#ifndef SOCK_H
#define SOCK_H
#include <stddef.h>

/***************/
/* Client Side */
/***************/

typedef struct SOK_Client SOK_Client;
typedef void(*sok_message_cb)(void*,char*,size_t);
typedef char*(*sok_request_cb)(void*,char*,size_t,size_t*);
typedef void*(*sok_cli_init_cb)(void*);
typedef void(*sok_cli_destroy_cb)(void*);
typedef void*(*sok_thread_cb)(void*);

/**
 *
 */
SOK_Client * SOK_Client_new(char*, int, sok_request_cb, void*, int);

/**
 *
 */
void SOK_Client_use_ssl(SOK_Client*);

/**
 *
 */
int SOK_Client_connect(SOK_Client*);

/**
 *
 */
void SOK_Client_set_send_data(SOK_Client*, void *);

/**
 *
 */
char * SOK_Client_request(SOK_Client*, char *, size_t, size_t*);

/**
 *
 */
void SOK_Client_send(SOK_Client *, char *, size_t);

/**
 *
 */
void SOK_Client_destroy(SOK_Client*);

/**
 *
 */
void SOK_Client_wait(SOK_Client *);

/***************/
/* Server Side */
/***************/

typedef struct SSOK_Client SSOK_Client;
typedef struct SSOK_Server SSOK_Server;

/**
 *
 */
SSOK_Server * SSOK_Server_new(int, sok_cli_init_cb, sok_request_cb,
		sok_cli_destroy_cb);

/**
 *
 */
void SSOK_Server_destroy(SSOK_Server *);

/**
 *
 */
void SSOK_Server_set_ssl_certificate(SSOK_Server *, char *, char *);

/**
 *
 */
void SSOK_Server_run(SSOK_Server *, int);

/**
 *
 */
void SSOK_Server_broadcast(SSOK_Server*, char*, size_t, SSOK_Client*);

/**
*
*/
SSOK_Client * SSOK_Client_new(int , sok_cli_init_cb, sok_request_cb,
		sok_cli_destroy_cb);

/**
 *
 */
char * SSOK_Client_request(void *, char *, size_t, size_t*);


/**
 *
 */
void SSOK_Client_send(SSOK_Client *, char *, size_t);

/**
 *
 */
void * SSOK_Client_get_server_data(const SSOK_Client*);

#endif /* SOCK_H */
