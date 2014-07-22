/*
 *  Copyright (c) 2013-2014 Alper Akcan <alper.akcan@gmail.com>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#ifndef __SOCKET_H___
#define __SOCKET_H___

int socket_destroy (int sd);
int socket_create (const char *addr, int port);
int socket_membership (int sd, const char *address, int on);

#endif
