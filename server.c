#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <pthread.h>

#include "server.h"

struct _ServerHandle {
	int socket;
	pthread_t socketListener;
	ReceiveCallback *onReceive;
};

void *listener(void *data);

ServerHandle server_init(char *listenIP, char *port, bool v4Only) {
	ServerHandle handle = calloc(sizeof(struct _ServerHandle), 1);

	char *address = NULL;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));

	if (v4Only) {
		hints.ai_family = AF_INET;
		address = listenIP;
	} else {
		hints.ai_family = AF_INET6;
		address = listenIP;
	}

	if (listenIP[0] == '*') {
		hints.ai_family = AF_UNSPEC;
		hints.ai_flags  = AI_PASSIVE;
	}
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *info = NULL;
	int result = getaddrinfo(address, port, &hints, &info);
	if (result != 0) {
		free(handle);
		return NULL;
	}

	struct addrinfo *cInfo = info;
	while (42) {
		if ((cInfo->ai_next == NULL) || (cInfo->ai_family == AF_INET6)) {
			break;
		}

		cInfo = cInfo->ai_next;
	}

	handle->socket = socket(cInfo->ai_family, cInfo->ai_socktype, cInfo->ai_protocol);
	if (handle->socket < 0) {
		freeaddrinfo(info);
		free(handle);
		return NULL;
	}

	int yes = 1;
	setsockopt(handle->socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	freeaddrinfo(info);

	return handle;
}

bool server_start(ServerHandle handle, ReceiveCallback *onReceive) {
	if (handle->onReceive) {
		// already running
		return false;
	}

	int result = listen(handle->socket, 20);
	if (result < 0) {
		return false;
	}

	pthread_create(&handle->socketListener, NULL, listener, handle);

	return true;
}

void server_stop(ServerHandle handle) {
	close(handle->socket);
	pthread_join(handle->socketListener, NULL);
	handle->onReceive = NULL;
	free(handle);
}


void *listener(void *data) {
	ServerHandle handle = (ServerHandle)data;

	while (42) {
		struct sockaddr_storage remoteAddr;
		memset(&remoteAddr, 0, sizeof(struct sockaddr_storage));
		
		socklen_t len = sizeof(struct sockaddr_storage);
		int fd = accept(handle->socket, (struct sockaddr *)&remoteAddr, &len);
		
		if (fd < 0) {
			if ((errno == EAGAIN) || (errno == EINTR) || (errno == ECONNABORTED)) {
				continue;
			}

			// unknown error die now
			pthread_exit(NULL);
		}

		Connection *conn = calloc(sizeof(Connection), 1);
		conn->fd = fd;

		
		if (remoteAddr.ss_family == AF_INET) {
			struct sockaddr_in *addr = (struct sockaddr_in *)&remoteAddr;
			inet_ntop(remoteAddr.ss_family, (const void *)&addr->sin_addr, conn->remoteIP, 40);
			conn->remotePort = addr->sin_port;
		} else if (remoteAddr.ss_family == AF_INET6) {
			struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&remoteAddr;
			inet_ntop(remoteAddr.ss_family, (const void *)&addr->sin6_addr, conn->remoteIP, 40);
			conn->remotePort = addr->sin6_port;
		}

		// TODO: Fire off receive task	
	}
}
