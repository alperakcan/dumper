/*
 *  Copyright (c) 2013-2014 Alper Akcan <alper.akcan@gmail.com>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "socket.h"

int socket_destroy (int sd)
{
	close(sd);
	return 0;
}

int socket_create (const char *addr, int port)
{
	int in;
	int on;
	int rc;
	struct sockaddr_in iaddr;
	in = socket(AF_INET, SOCK_DGRAM, 0);
	if (in < 0) {
		fprintf(stderr, "socket failed\n");
		return -1;
	}
	on = 1;
	rc = setsockopt(in, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
	if (rc < 0) {
		fprintf(stderr, "setsockopt for reuseaddr failed\n");
	}
	memset(&iaddr, 0, sizeof(iaddr));
	iaddr.sin_family = AF_INET;
	inet_aton(addr, &iaddr.sin_addr);
	iaddr.sin_port = htons(port);
	rc = bind(in, (const struct sockaddr *) &iaddr, sizeof(iaddr));
	if (rc < 0) {
		fprintf(stderr, "bind to %s:%d failed\n", addr, port);
	}
	return in;
}

int socket_membership (int sd, const char *address, int on)
{
	struct hostent *h;
	struct ip_mreq mreq;
	struct in_addr mcastip;
	h = gethostbyname(address);
	if (h == NULL) {
		return -1;
	}
	memcpy(&mcastip, h->h_addr_list[0], h->h_length);
	mreq.imr_multiaddr.s_addr = mcastip.s_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (on) {
		return setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &mreq, sizeof(mreq));
	} else {
		return setsockopt(sd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &mreq, sizeof(mreq));
	}
}
