#ifndef SOCK_H
#define SOCK_H

#ifndef SOK_BUFFER_SIZE
	#define SOK_BUFFER_SIZE 256
#endif

/***************/
/* Client Side */
/***************/

struct SOK_Client;
typedef struct SOK_Client SOK_Client;

/**
 *
 */
SOK_Client * SOK_Client_new(char*, int, void(*)(void*,char*), void*);

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
void SOK_Client_send(void *, char *);

/**
 *
 */
void SOK_Client_destroy(SOK_Client*);

/***************/
/* Server Side */
/***************/

struct SSOK_Client;
struct SSOK_Server;
typedef struct SSOK_Server SSOK_Server;

/**
 *
 */
SSOK_Server * SSOK_Server_new(int, void*(*)(void*), void(*)(void*,char*),
		void(*)(void*));
/**
 *
 */
void SSOK_Server_destroy(SSOK_Server *);

/**
 *
 */
void SSOK_Server_run(SSOK_Server *);

/**
 *
 */
void SSOK_Server_broadcast(SSOK_Server*, char*, struct SSOK_Client*);

/**
*
*/
struct SSOK_Client * SSOK_Client_new(int , void*(*)(void*),
		void(*)(void*,char*), void(*)(void*));

/**
 *
 */
void SSOK_Client_send(void *data, char *buffer);

/**
 *
 */
void * SSOK_Client_get_server_data(const struct SSOK_Client*);

#endif /* SOCK_H */
