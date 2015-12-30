# Simple Socket library

This library abstracts all complexity of the POSIX socket interface into some very simple interface that is easy to use and relatively foolproof.

## Installation

To compile you'll need to have `clang` installed.
Just enter `make` in the toplevel directory. It will build a static library (`libUnchainedSocket.a`) and a small demo program.

If you want to install to your system (as needed for the swift build system to pick up the library file) just enter `make install`.
It will install to `/usr/local/{lib,include}` by default but you may customize by setting `DESTDIR` (like `make install DESTDIR=./install`).

## Usage

C interface: 

~~~c
#include <unchainedSocket/server.h>

// data receive callback
bool receiveCallback(Connection *connection, char *data, size_t size) {
	// log the data
    printf("[%d] %s:%d: %s\n", connection->id, connection->remoteIP, connection->remotePort, data);
    
    // return `true` continue receiving data
    return true;
}

// initialize server for all network interfaces on port 4567
// with IPv6 enabled and 10 seconds idle timeout
ServerHandle handle = server_init("*", "4567", false, 10);
if (handle == NULL) {
    printf("Unable to initialize server!\n");
    abort();
}
    
// start listening with 10 parallel worker threads
if (!server_start(handle, &receiveCallback, 10)) {
    printf("Unable to listen to socket!\n");
    abort();
}
    
// Now go into main loop of your program or just suspend the thread somehow
~~~

Swift should work analogous but does currently not work correctly.

## Copyright

Copyright (c) 2015 Johannes Schriewer, licensed under 3 Clause BSD License, see LICENSE.txt for full license text.