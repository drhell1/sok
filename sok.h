/**
 * @file sok.h
 * @author José Monteiro
 * @author João Silva
 * @date 12 Apr 2016
 *
 * @brief C posix/ssl socket abstraction layer
 *
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
 * @brief SOK_Client constructor.
 *
 * Creates an instance of SOK_Client, which is used to connect
 * to a SOK server.
 *
 * @param addr String containing the server's ip address or hostname.
 * @param port Server's port to connect to.
 * @param cli_request_callback Function pointer to be called whenever the
 * server sends a request to the client.
 * @param data Pointer set by the programmer to be passed as an argument
 * to the received callback.
 * @param async Defines if the requests are to block the program or run in a
 * separate thread.
 * @return Pointer to the new SOK_Client.
 */
SOK_Client * SOK_Client_new(char *addr, int port, sok_request_cb
		cli_request_callback, void *data, int async);

/**
 * @brief Defines if the server uses ssl.
 * @param this Pointer to SOK_Client instance.
 */
void SOK_Client_use_ssl(SOK_Client *this);

/**
 * @brief Starts connection to the server.
 * @param this Pointer to SOK_Client instance.
 * @return 1 if the connection was successful, 0 if otherwise.
 */
int SOK_Client_connect(SOK_Client *this);

/**
 * @brief Overrides the pointer to be passed as an argument to the
 * received callback.
 * @param this Pointer to SOK_Client instance.
 */
void SOK_Client_set_send_data(SOK_Client *this, void *data);

/**
 * @brief Sends request to the server.
 *
 * Locks the program until the server sends a response.
 * @param this Pointer to SOK_Client instance.
 * @param buffer String containing the request's message.
 * @param len Size of the @p buffer.
 * @param result_len Reference to a size_t, to be modified to contain the size
 * of the request response from the server.
 * @return A string containing the response to the request from the server.
 */
char * SOK_Client_request(SOK_Client *this, char *buffer, size_t len, size_t
		*result_len);

/**
 * @brief Sends a message to the server.
 * @param this Pointer to SOK_Client instance.
 * @param buffer String containing the message to be sent to the server.
 * @param len Size of the @p buffer
 */
void SOK_Client_send(SOK_Client *this, char *buffer, size_t len);

/**
 * @brief SOK_Client destructor.
 *
 * Disconnects and frees SOK_Client's requests and allocated memory.
 * @param this Pointer to SOK_Client instance.
 */
void SOK_Client_destroy(SOK_Client *this);

/**
 * @brief Waits for the client's connection to the server to be completed.
 * @param this Pointer to SOK_Client instance.
 */
void SOK_Client_wait(SOK_Client *this);

/***************/
/* Server Side */
/***************/

typedef struct SSOK_Client SSOK_Client;
typedef struct SSOK_Server SSOK_Server;

/**
 * @brief SSOK_Server constructor.
 *
 * Creates an instance of SSOK_Server, which is used to host a SOK server.
 *
 * @param port Port server will be listening to.
 * @param cli_init Function pointer to a constructor that is called whenever a
 * new client connects.
 * @param cli_request_callback Function pointer to be called whenever a client
 * sends a request.
 * @param cli_destroy Function pointer to the client's destructor, this
 * function must free all the memory allocated in @p cli_init.
 */
SSOK_Server * SSOK_Server_new(int port, sok_cli_init_cb cli_init,
		sok_request_cb cli_request_callback, sok_cli_destroy_cb cli_destroy);

/**
 * @brief SSOK_Server destructor.
 *
 * Shutsdown server and frees all clients by calling respective destructors.
 * @param this Pointer to SSOK_Server instance.
 */
void SSOK_Server_destroy(SSOK_Server *this);

/**
 * @brief Sets the server's certificate and key paths.
 *
 * @param this Pointer to SSOK_Server instance.
 * @param cert Path to the ssl certificate file. If the pointer is NULL, the
 * server won't use ssl.
 * @param key Path to the ssl key file.
 */
void SSOK_Server_set_ssl_certificate(SSOK_Server *this, char *cert, char *key);

/**
 * @brief Starts server accept loop.
 *
 * @param this Pointer to SSOK_Server instance.
 * @param async If @p async is 0, the function will wait for the server to be
 * terminated before returning to the caller.
 */
void SSOK_Server_run(SSOK_Server *this, int async);

/**
 * @brief Sends message to all clients.
 * @param this Pointer to SSOK_Server instance.
 * @param message String containing the message to be sent to the clients.
 * @param len Size of the @p message.
 * @param except If @p except is a valid client, the message will not be sent
 * to that client.
 */
void SSOK_Server_broadcast(SSOK_Server *this, char *message, size_t len,
		SSOK_Client *except);

/**
 * @brief Sends a message to the server.
 * @param this Pointer to SSOK_Client instance.
 * @param buffer String containing the message to be sent to the server.
 * @param len Size of the @p buffer
 */
void SSOK_Client_send(SSOK_Client *, char *, size_t);

/**
 * @brief Gets the pointer to the instance allocated by the user defined
 * constructor.
 * @param this Pointer to SSOK_Client instance.
 * @return Pointer to the instance allocated by the user defined constructor.
 */
void * SSOK_Client_get_server_data(const SSOK_Client *this);

#endif /* SOCK_H */
