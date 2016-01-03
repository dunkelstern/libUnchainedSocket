//
//  server.c
//  UnchainedSocket
//
//  Created by Johannes Schriewer on 30/12/15.
//  Copyright © 2015 Johannes Schriewer. All rights reserved.
//


#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <pthread.h>

// for sendfile
#if defined(__APPLE__) && defined(__MACH__)
#include <sys/uio.h>
#else
#include <sys/sendfile.h>
#endif

#include "debug.h"
#include "server.h"
#include "queue.h"

// server handle definition
struct _ServerHandle {
    // user settings
    ReceiveCallback onReceive;  // receive callback function
	void *userData;				// user data given to the data callback verbatim
    int timeout;                // socket read timeout

    // socket specific
    int socket;                 // socket fd
    pthread_t socketListener;   // listener thread
    
    // connections
    pthread_mutex_t connectionMutex;
    Connection **connections;
    int numConnections;
    int allocatedConnections;
    int connectionID;
    
    // worker queue
    work_queue queue;
};

// listener thread
void *listener(void *data);

// Internal action functions
static void accept_connection(ServerHandle handle);
static void close_idle_connections(ServerHandle handle);
static void close_connection(ServerHandle handle, Connection *connection);
static void read_data(ServerHandle handle, fd_set readable);

// read task
struct readTaskData {
    ServerHandle handle;
    Connection *connection;
};
void read_task(void *data);
void clean_task(void *data);

// Internal helper
struct selectMask {
    fd_set readSet;
    int maxFD;
};

static struct selectMask build_select_mask(ServerHandle handle);

/*
 * MARK: - API
 */

ServerHandle server_init(const char *listenIP, const char *port, bool v4Only, int timeout) {

    // zero initialize needed structs
    ServerHandle handle = calloc(sizeof(struct _ServerHandle), 1);
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));

    // initialize handle
    handle->timeout = timeout;
    handle->allocatedConnections = 1000;
    handle->numConnections = 0;
    handle->connections = calloc(1000, sizeof(Connection *));
    pthread_mutex_init(&handle->connectionMutex, NULL);

    // address to listen on
    const char *address = listenIP;

    // if only IPv4 is requested filter for that
    hints.ai_family = (v4Only) ? PF_INET : PF_UNSPEC;

    // if listenIP starts with a asterisk we use a wildcard listener
	if (listenIP[0] == '*') {
		hints.ai_flags = AI_PASSIVE;
        address = NULL;
	}
    
    // we want a TCP socket
	hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // fetch all address info for the defined filter
	struct addrinfo *info = NULL;
	int result = getaddrinfo(address, port, &hints, &info);
	if (result != 0) {
		free(handle);
        DebugLog("getaddrinfo call failed: %s\n", gai_strerror(result));
		return NULL;
	}

    // select the best info struct (usually use IPv6)
	struct addrinfo *cInfo = info;
	while (42) {
#if defined(DEBUG)
        DebugLog("addrinfo:\n");
        DebugLog(" - family: %d (IPv4: %d, IPv6: %d)\n", cInfo->ai_family, AF_INET, AF_INET6);
        DebugLog(" - socktype: %d (stream: %d, dgram: %d)\n", cInfo->ai_socktype, SOCK_STREAM, SOCK_DGRAM);
        DebugLog(" - protocol: %d\n", cInfo->ai_protocol);
        char buf[45];
        if (cInfo->ai_family == AF_INET) {
            struct sockaddr_in *s = (struct sockaddr_in *)cInfo->ai_addr;
            inet_ntop(cInfo->ai_family, (const void *)&s->sin_addr, buf, 45);
        } else {
            struct sockaddr_in6 *s = (struct sockaddr_in6 *)cInfo->ai_addr;
            inet_ntop(cInfo->ai_family, (const void *)&s->sin6_addr, buf, 45);
        }
        DebugLog(" - address: %s\n", buf);
#endif
        if (((cInfo->ai_next == NULL) || (cInfo->ai_family == AF_INET6)) || (v4Only)) {
			break;
		}

		cInfo = cInfo->ai_next;
	}
    
    // create a socket
	handle->socket = socket(cInfo->ai_family, cInfo->ai_socktype, cInfo->ai_protocol);
	if (handle->socket < 0) {
		freeaddrinfo(info);
		free(handle);
        DebugLog("socket call failed: %s\n", strerror(errno));
		return NULL;
	}

    // allow socket address reuse to avoid being blocked for two minutes after restart
    int yes = 1;
    setsockopt(handle->socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    freeaddrinfo(info);

    // bind to the selected address
    if (bind(handle->socket, cInfo->ai_addr, cInfo->ai_addrlen)) {
        freeaddrinfo(info);
        free(handle);
        DebugLog("bind call failed: %s\n", strerror(errno));
        return NULL;
    }
    
    // all ready
	return handle;
}

bool server_start(ServerHandle handle, ReceiveCallback onReceive, void *userData, int workerCount) {
	if (handle->onReceive) {
		// already running
		return false;
	}
    handle->onReceive = onReceive;
    handle->queue = queue_create(workerCount);
	handle->userData = userData;
    queue_resume(handle->queue);

    // start listening
	int result = listen(handle->socket, 20);
	if (result < 0) {
        DebugLog("listen call failed: %s\n", strerror(errno));
		return false;
	}

    // create a thread for the accept loop
    DebugLog("Starting ACCEPT thread\n");
	pthread_create(&handle->socketListener, NULL, listener, handle);

	return true;
}

void server_stop(ServerHandle handle) {
    // close the socket
    DebugLog("Closing socket\n");
	close(handle->socket);
    
    // wait for the accept thread to finish
    DebugLog("Joining ACCEPT thread\n");
	pthread_join(handle->socketListener, NULL);
    
    // destroy the handle
    queue_free(handle->queue);

    for(int i = 0; i < handle->numConnections; i++) {
        close_connection(handle, handle->connections[i]);
    }
    free(handle->connections);

    pthread_mutex_destroy(&handle->connectionMutex);
    handle->onReceive = NULL;
	free(handle);
}

void server_send_data(Connection *connection, const char *data, size_t len) {
    connection->sending = true;
    
    size_t bytesWritten = 0;
    while (bytesWritten < len) {
        // Try to send the data
        ssize_t result = write(connection->fd, data + bytesWritten, len - bytesWritten);
        if (result < 0) {
            // error occured, check if it was recoverable
            if (errno == EINTR) {
                continue;
            }
            
            if (errno == EAGAIN) {
                // TODO: select on fd until writable instead of spinning
                continue;
            }
            
            // not recoverable
            DebugLog("[SEND:%d] Could not send data: %s\n", connection->id, strerror(errno));
            break;
        }
        
        bytesWritten += result;
    }
    connection->sending = false;
}

void server_send_file(Connection *connection, const char *filename) {
    // open filne
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        DebugLog("[SEND:%d] Could not open file: %s\n", connection->id, strerror(errno));
        return;
    }

    // fetch file size
    struct stat st;
    memset(&st, 0, sizeof(struct stat));
    fstat(fd, &st);
    size_t fileSize = st.st_size;

    
    // send file by using OS specific sendfile implementation
    connection->sending = true;
    off_t size = 0;
    off_t bytesWritten = 0;
#if defined(__APPLE__) && defined(__MACH__)

    // OSX way of sending a file
    while (bytesWritten < fileSize) {
        if (sendfile(fd, connection->fd, bytesWritten, &size, NULL, 0)) {
            
            // buffer full?
            if (errno == EAGAIN) {
                bytesWritten += size;
                size = 0;
                // TODO: select on the fd until writable again instead of spinning
                continue;
            }
            
            // unrecoverable error
            DebugLog("[SEND:%d] Could not send file: %s\n", connection->id, strerror(errno));
            break;
        }
    }
#else

    // Linux way of sending a file
    while (bytesWritten < fileSize) {
        ssize_t result = sendfile(connection->fd, fd, &bytesWritten, fileSize - bytesWritten);
        
        if ((result < 0) && (errno != EAGAIN)) {
            // unrecoverable error
            DebugLog("[SEND:%d] Could not send file: %s\n", connection->id, strerror(errno));
            break;
        }

        // TODO: select on the fd until writable again instead of spinning
    }
#endif
    connection->sending = false;

    // finished, close file
    close(fd);
}


/*
 * MARK: - Accept thread
 */

void *listener(void *data) {
	ServerHandle handle = (ServerHandle)data;

    // prepare timeout
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    
    DebugLog("[Listenter thread] Hello\n");
    
    // select loop
	while (42) {
        // reset timeout as linux rewrites it with the unused time
        tv.tv_sec = handle->timeout;

        // select on all open connections until someone has something to read
        struct selectMask msk = build_select_mask(handle);
        int result = select(msk.maxFD + 1, &msk.readSet, NULL, NULL, &tv);
        
        if (result == 0) {
            // timeout, kick all open connections that are not sending currently
            close_idle_connections(handle);
        } else if ((result < 0) && (errno != EINTR) && (errno != EBADF)) {
            DebugLog("[Listenter thread] Error in select: %s\n", strerror(errno));
            pthread_exit(NULL);
        }
        
        // Check if we can accept a connection
        if (FD_ISSET(handle->socket, &msk.readSet)) {
            accept_connection(handle);
        }
        
        // check all open connections and start read threads
        if (result > 0) {
            read_data(handle, msk.readSet);
        }
    }
}

static void accept_connection(ServerHandle handle) {
    // zero out the remote address struct
    struct sockaddr_storage remoteAddr;
    memset(&remoteAddr, 0, sizeof(struct sockaddr_storage));
    
    // wait for a client to connect
    DebugLog("[ACCEPT] Waiting for connection\n");
    socklen_t len = sizeof(struct sockaddr_storage);
    int fd;
    do {
        fd = accept(handle->socket, (struct sockaddr *)&remoteAddr, &len);
    
        // if some error happened determine if it is recoverable
        if ((fd < 0) && (errno != EINTR)) {
            // unknown error die now
            DebugLog("[ACCEPT] Error while accept: %s\n", strerror(errno));
            return;
        }
    } while (fd < 0);
    
    // make socket non blocking
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    // create a new connection struct
    Connection *conn = calloc(sizeof(Connection), 1);
	conn->remoteIP = calloc(45, sizeof(char));
    conn->fd = fd;
    conn->id = handle->connectionID++;
    
    // fill out the remote address and port
    if (remoteAddr.ss_family == AF_INET) {
        struct sockaddr_in *addr = (struct sockaddr_in *)&remoteAddr;
        inet_ntop(remoteAddr.ss_family, (const void *)&addr->sin_addr, conn->remoteIP, 45);
        conn->remotePort = addr->sin_port;
        
        DebugLog("[ACCEPT] Remote %s:%d\n", conn->remoteIP, conn->remotePort);
    } else if (remoteAddr.ss_family == AF_INET6) {
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&remoteAddr;
        inet_ntop(remoteAddr.ss_family, (const void *)&addr->sin6_addr, conn->remoteIP, 45);
        conn->remotePort = addr->sin6_port;
        
        DebugLog("[ACCEPT] Remote [%s]:%d\n", conn->remoteIP, conn->remotePort);
    }
    
    // add connection to list
    pthread_mutex_lock(&handle->connectionMutex);
    if (handle->allocatedConnections == handle->numConnections) {
        handle->allocatedConnections *= 2;
        handle->connections = realloc(handle->connections, handle->allocatedConnections * sizeof(Connection *));
    }
    handle->connections[handle->numConnections] = conn;
    handle->numConnections++;
    pthread_mutex_unlock(&handle->connectionMutex);
}

static void close_idle_connections(ServerHandle handle) {
    pthread_mutex_lock(&handle->connectionMutex);
    
    // remove from open connection list
    int removedConnections = 0;
    for(int i = 0; i < handle->numConnections; i++) {
        Connection *connection = handle->connections[i];
        if (connection->sending == false) {
            DebugLog("[IDLE] closing idle connection %d\n", connection->id);

            // close this connection
            close(connection->fd);
           
		   	// free remote ip pointer
			free(connection->remoteIP);

            // remove from list
            if (i == handle->numConnections - 1) {
                // last one, just decrement connection count
                removedConnections++;
                break;
            } else {
                // move all connections above this one one down
                memmove(handle->connections[i], handle->connections[i+1], sizeof(Connection *) * (handle->numConnections - i - 1 - removedConnections));
                removedConnections++;
            }
        }
    }
    
    // correct connection count
    handle->numConnections -= removedConnections;
    
    pthread_mutex_unlock(&handle->connectionMutex);
}

static void close_connection(ServerHandle handle, Connection *connection) {
    pthread_mutex_lock(&handle->connectionMutex);

    // close file descriptor
    DebugLog("[CLOSE] closing connection %d\n", connection->id);
    close(connection->fd);
    
    // remove from open connection list
    for(int i = 0; i < handle->numConnections; i++) {
        if (handle->connections[i] == connection) {
            if (connection->remoteIP) {
                free(connection->remoteIP);
                connection->remoteIP = NULL;
            }

            if (i == handle->numConnections - 1) {
                // last one, just decrement connection count
                handle->numConnections--;
                break;
            } else {
                // move all connections above this one one down
                memmove(handle->connections[i], handle->connections[i+1], sizeof(Connection *) * (handle->numConnections - i - 1));
            }
        }
    }
    
    pthread_mutex_unlock(&handle->connectionMutex);
}

static void read_data(ServerHandle handle, fd_set readable) {
    // add all open connections
    for(int i = 0; i < handle->numConnections; i++) {
        Connection *connection = handle->connections[i];
        if (FD_ISSET(connection->fd, &readable)) {
            struct readTaskData *data = malloc(sizeof(struct readTaskData));
            data->handle = handle;
            data->connection = connection;
            queue_add_task(handle->queue, read_task, clean_task, data);
        }
    }
}

static struct selectMask build_select_mask(ServerHandle handle) {
    struct selectMask msk;
    FD_ZERO(&msk.readSet);
    
    // add socket itselt for selecting on 'accept' calls
    msk.maxFD = handle->socket;
    FD_SET(handle->socket, &msk.readSet);
    
    // add all open connections
    pthread_mutex_lock(&handle->connectionMutex);
    for(int i = 0; i < handle->numConnections; i++) {
        Connection *connection = handle->connections[i];
        if (connection->fd > msk.maxFD) {
            msk.maxFD = connection->fd;
        }
        FD_SET(connection->fd, &msk.readSet);
    }
    pthread_mutex_unlock(&handle->connectionMutex);
    
    // return the set
    return msk;
}

/*
 * MARK: - Task workers
 */

void read_task(void *data) {
    struct readTaskData *info = (struct readTaskData *)data;
    char buffer[4096];
    
    ssize_t bytesRead;
    do {
        bytesRead = read(info->connection->fd, buffer, 4096);
        if ((bytesRead < 0) && (errno != EINTR)) {
            // unrecoverable error
            if (errno != EAGAIN) {
                DebugLog("[READ] error: %s\n", strerror(errno));
                close_connection(info->handle, info->connection);
            }
            return;
        } else if (bytesRead == 0) {
            // end of file, aka connection closed
            DebugLog("[READ] EOF, closing connection\n");
            close_connection(info->handle, info->connection);
            return;
        }
    } while (bytesRead < 0);
    
    buffer[bytesRead] = 0;
    DebugLog("[READ] read %d bytes\n", (int)bytesRead);
    bool keepConnection = info->handle->onReceive(info->connection, info->handle->userData, buffer, bytesRead);
    if (!keepConnection) {
        DebugLog("[READ] closing connection upon request\n");
        close_connection(info->handle, info->connection);
    }
}

void clean_task(void *data) {
    struct readTaskData *info = (struct readTaskData *)data;
    free(info);
}
