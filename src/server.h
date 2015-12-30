//
//  server.h
//  UnchainedSocket
//
//  Created by Johannes Schriewer on 30/12/15.
//  Copyright © 2015 Johannes Schriewer. All rights reserved.
//

#ifndef __server_h
#define __server_h

#include <stdint.h>
#include <stdbool.h>

/** Connection identifier */
typedef struct _Connection {
	int id;         /**< Connection ID */

	char remoteIP[45]; /**< Remote IP address */
	int remotePort; /**< Remote port */
    
    // internal
    int fd;         /**< Socket handle */
    bool sending;   /**< currently sending data */
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
 * @param timeout: socket idle timeout in seconds (close socket when not receiving data for this amount of time)
 */
ServerHandle server_init(char *listenIP, char *port, bool v4Only, int timeout);

/** Start a server
 *
 * @param handle: Server handle
 */

bool server_start(ServerHandle handle, ReceiveCallback onReceive, int workerCount);

/** Stop a server
 *
 * @param handle: Server to stop
 */
void server_stop(ServerHandle handle);

/** Send data back to the connected client
 *
 * @param connection: the connection to send the data to
 * @param data: data to send
 * @param len: length of the data to send
 */
void server_send_data(Connection *connection, char *data, size_t len);

/** Send a file back to the connected client
 *
 * @param connection: the connection to send the data to
 * @param filename: path to the file to send
 */
void server_send_file(Connection *connection, char *filename);


#endif /* __server_h */
