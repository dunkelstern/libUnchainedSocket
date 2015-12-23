#ifndef __server_h
#define __server_h

#include <stdint.h>
#include <stdbool.h>

/** Connection identifier */
typedef struct _Connection {
	int id;         /**< Connection ID */
	int fd;         /**< Socket handle */
	
	char *remoteIP; /**< Remote IP address */
	int remotePort; /**< Remote port */
} Connection;

/** Opaque server handle */
typedef struct _ServerHandle *ServerHandle;

/** Data Receive callback, return false if you want the server to terminate the connection */
typedef bool (*ReceiveCallback)(Connection *connection, char *data, size_t size);

/** Initialize server
 *
 * @param listenIP: Textual form of IP interface to listen on (use "*" for wildcard)
 * @param port: port number or name
 * @param v4Only: set to true to listen only on IPv4 sockets
 */
ServerHandle server_init(char *listenIP, char *port, bool v4Only);

/** Start a server
 *
 * @param handle: Server handle
 */

bool server_start(ServerHandle handle, ReceiveCallback *onReceive);

/** Stop a server
 *
 * @param handle: Server to stop
 */
void server_stop(ServerHandle handle);

#endif /* __server_h */
