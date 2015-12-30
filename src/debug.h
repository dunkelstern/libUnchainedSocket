//
//  debug.h
//  UnchainedSocket
//
//  Created by Johannes Schriewer on 30/12/15.
//  Copyright © 2015 Johannes Schriewer. All rights reserved.
//

#ifndef __debug_h
#define __debug_h

#include <stdio.h>

#if defined(DEBUG)
# define DebugLog(_fmt, ...) fprintf(stderr, _fmt, ## __VA_ARGS__)
#else
# define DebugLog(_fmt, ...) /* do nothing */
#endif

#endif /* debug_h */
