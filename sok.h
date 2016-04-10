#ifndef SOCK_H
#define SOCK_H
#include <stddef.h>

/***************/
/* Client Side */
/***************/

typedef struct SOK_Client SOK_Client;

/**
 *
 */
SOK_Client * SOK_Client_new(char*, int, void(*)(void*,char*,size_t), void*);

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
void SOK_Client_send(void *, char *, size_t);

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

struct SSOK_Client;
typedef struct SSOK_Server SSOK_Server;

/**
 *
 */
SSOK_Server * SSOK_Server_new(int, void*(*)(void*), void(*)(void*,char*, size_t),
		void(*)(void*));

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
void SSOK_Server_run(SSOK_Server *);

/**
 *
 */
void SSOK_Server_broadcast(SSOK_Server*, char*, size_t, struct SSOK_Client*);

/**
*
*/
struct SSOK_Client * SSOK_Client_new(int , void*(*)(void*),
		void(*)(void*,char*,size_t), void(*)(void*));

/**
 *
 */
void SSOK_Client_send(void *, char *, size_t);

/**
 *
 */
void * SSOK_Client_get_server_data(const struct SSOK_Client*);

#endif /* SOCK_H */
