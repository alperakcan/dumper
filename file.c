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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file.h"

int file_create (const char *path)
{
	int fd;
	fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0666);
	return fd;
}

int file_destroy (int fd)
{
	close(fd);
	return 0;
}

int file_write (int file, const void *buffer, size_t n)
{
	return write(file, buffer, n);
}
