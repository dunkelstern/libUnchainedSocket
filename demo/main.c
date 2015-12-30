//
//  main.c
//  UnchainedSocket
//
//  Created by Johannes Schriewer on 30/12/15.
//  Copyright © 2015 Johannes Schriewer. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "server.h"

bool receiveCallback(Connection *connection, char *data, size_t size) {
    printf("[%d] %s:%d: %s\n", connection->id, connection->remoteIP, connection->remotePort, data);
    return true; // continue receiving data
}

int main(int argc, char **argv) {
    
    // initialize server
    printf("Initializing server...\n");
    ServerHandle handle = server_init("*", "4567", false, 10);
    if (handle == NULL) {
        printf("Unable to initialize server!\n");
        abort();
    }
    
    // start listening
    printf("Listening on *:4567...\n");
    if (!server_start(handle, &receiveCallback, 10)) {
        printf("Unable to listen to socket!\n");
        abort();
    }
    
    printf("Ready to accept connections...\n");
    // just let the server handle everything
    while (42) {
        sleep(10);
    }
}