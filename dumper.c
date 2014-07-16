
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>

#include <pthread.h>

#include <poll.h>

#include "sync.h"

#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)

#define PACKET_MAX_SIZE	65536

struct packet {
	int size;
	char buffer[PACKET_MAX_SIZE];
};

struct buffer {
	int in;
	int out;
	unsigned int pcount;
	unsigned int acount;
	struct {
		pthread_t thread;
		int fd;
		int started;
		int running;
		int stopped;
	} reader,
	  writer;
	mutex mutex;
	condvar cond;
	struct packet packets[0];
};

static void * buffer_reader (void *arg)
{
	int rc;
	int in;
	int *running;
	unsigned int pcnt;
	unsigned int roff;
	struct pollfd pfd;
	struct packet *packet;
	struct buffer *buffer = arg;
	struct packet *packets = buffer->packets;
	condvar *cond = &buffer->cond;
	mutex *mutex = &buffer->mutex;
	mutex_lock(mutex);
	buffer->reader.started = 1;
	buffer->reader.running = 1;
	buffer->reader.stopped = 0;
	cond_signal(cond);
	mutex_unlock(mutex);
	roff = 0;
	in = buffer->in;
	pcnt = buffer->pcount;
	running = &buffer->reader.running;
	pfd.fd = buffer->in;
	pfd.events = POLLIN;
	while (1) {
		rc = poll(&pfd, 1, 1000);
		if (unlikely(rc < 0)) {
			fprintf(stderr, "poll failed\n");
			return NULL;
		}
		if (unlikely(rc == 0)) {
			fprintf(stderr, "poll timeout\n");
			continue;
		}
		mutex_lock(mutex);
		if (unlikely(buffer->acount >= pcnt)) {
			fprintf(stderr, "buffer under run\n");
			mutex_unlock(mutex);
			continue;
		}
		mutex_unlock(mutex);
		packet = &packets[roff];
		packet->size = read(in, packet->buffer, sizeof(packet->buffer));
		if (unlikely(packet->size < 0)) {
			fprintf(stderr, "read failed");
			continue;
		}
		if (++roff >= pcnt) {
			roff = 0;
		}
		mutex_lock(mutex);
		if (unlikely(*running == 0)) {
			mutex_unlock(mutex);
			break;
		}
		buffer->acount += 1;
		cond_signal(cond);
		mutex_unlock(mutex);
	}
	mutex_lock(mutex);
	buffer->reader.started = 1;
	buffer->reader.running = 0;
	buffer->reader.stopped = 1;
	cond_signal(cond);
	mutex_unlock(mutex);
	return NULL;
}

static void * buffer_writer (void *arg)
{
	int rc;
	int out;
	int *running;
	unsigned int pcnt;
	unsigned int woff;
	struct packet *packet;
	struct buffer *buffer = arg;
	struct packet *packets = buffer->packets;
	condvar *cond = &buffer->cond;
	mutex *mutex = &buffer->mutex;
	mutex_lock(mutex);
	buffer->writer.started = 1;
	buffer->writer.running = 1;
	buffer->writer.stopped = 0;
	cond_signal(cond);
	mutex_unlock(mutex);
	woff = 0;
	out = buffer->out;
	pcnt = buffer->pcount;
	running = &buffer->writer.running;
	while (1) {
		mutex_lock(mutex);
		while (unlikely(buffer->acount == 0) && likely(*running != 0)) {
			cond_wait(cond);
		}
		if (unlikely(*running == 0)) {
			mutex_unlock(mutex);
			break;
		}
		mutex_unlock(mutex);
		packet = &packets[woff];
		rc = write(out, packet->buffer, packet->size);
		if (unlikely(rc != packet->size)) {
			fprintf(stderr, "write failed (rc: %d, size: %d, out: %d)\n", rc, packet->size, buffer->out);
		}
		mutex_lock(mutex);
		buffer->acount -= 1;
		mutex_unlock(mutex);
		if (++woff >= pcnt) {
			woff = 0;
		}
	}
	mutex_lock(mutex);
	buffer->writer.started = 1;
	buffer->writer.running = 0;
	buffer->writer.stopped = 1;
	cond_signal(cond);
	mutex_unlock(mutex);
	return NULL;
}

static struct buffer * buffer_create (unsigned int size, int in, int out)
{
	struct buffer *buffer;
	buffer = malloc(sizeof(struct buffer) + size);
	if (buffer == NULL) {
		fprintf(stderr, "malloc failed\n");
		return NULL;
	}
	buffer->in = in;
	buffer->out = out;
	buffer->pcount = size / sizeof(struct packet);
	buffer->acount = 0;
	fprintf(stdout, "buffer packets count: %d\n", buffer->pcount);
	mutex_init(&buffer->mutex);
	cond_init(&buffer->cond, &buffer->mutex);
	pthread_create(&buffer->writer.thread, NULL, buffer_writer, buffer);
	pthread_create(&buffer->reader.thread, NULL, buffer_reader, buffer);
	return buffer;
}

static int buffer_destroy (struct buffer *buffer)
{
	if (buffer == NULL) {
		return 0;
	}
	mutex_lock(&buffer->mutex);
	buffer->reader.running = 0;
	cond_signal(&buffer->cond);
	while (buffer->reader.stopped == 0) {
		cond_wait(&buffer->cond);
	}
	buffer->writer.running = 0;
	cond_signal(&buffer->cond);
	while (buffer->writer.stopped == 0) {
		cond_wait(&buffer->cond);
	}
	mutex_unlock(&buffer->mutex);
	pthread_join(buffer->reader.thread, NULL);
	pthread_join(buffer->writer.thread, NULL);
	cond_destroy(&buffer->cond);
	mutex_destroy(&buffer->mutex);
	free(buffer);
	return 0;
}

static int socket_destroy (int sd)
{
	close(sd);
	return 0;
}

static int socket_create (char *addr, int port)
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

static int socket_membership (int sd, const char *address, int on)
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

static int file_create (char *path)
{
	int fd;
	fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0666);
	return fd;
}

static int file_destroy (int fd)
{
	close(fd);
	return 0;
}

static int print_help (char *prog)
{
	printf("%s usage:\n", prog);
	printf("  -i : ip address\n");
	printf("  -p : source port\n");
	printf("  -s : buffer size in bytes\n");
	printf("  -f : destination file\n");
	printf("  -t : run time in seconds, -1 for infinite\n");
	return 0;
}

int main (int argc, char *argv[])
{
	int c;
	char *addr;
	char *file;
	int port;
	int size;
	int time;
	int in;
	int out;
	struct buffer *buffer;
	addr = NULL;
	file = NULL;
	port = 0;
	size = 20 * 1024 * 1024;
	time = -1;
	while ((c = getopt(argc, argv, "i:p:s:f:t:h")) != -1) {
		switch (c) {
			case 'i':
				addr = optarg;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 's':
				size = atoi(optarg);
				break;
			case 'f':
				file = optarg;
				break;
			case 't':
				time = atoi(optarg);
				break;
			case 'h':
			default:
				print_help(argv[0]);
				exit(0);
		}
	}
	if (addr == NULL) {
		fprintf(stderr, "source address is null\n");
		exit(-1);
	}
	if (file == NULL) {
		fprintf(stderr, "destination file is null\n");
		exit(-1);
	}
	fprintf(stdout, "creating socket for %s:%d\n", addr, port);
	in = socket_create(addr, port);
	if (in < 0) {
		fprintf(stderr, "socket create failed\n");
		exit(-1);
	}
	socket_membership(in, addr, 1);
	fprintf(stdout, "creating file for %s\n", file);
	out = file_create(file);
	if (out < 0) {
		fprintf(stderr, "file create failed\n");
		socket_destroy(in);
		exit(-1);
	}
	fprintf(stdout, "creating buffer for %d bytes\n", size);
	buffer = buffer_create(size, in, out);
	if (buffer == NULL) {
		fprintf(stderr, "buffer create failed\n");
		socket_destroy(in);
		file_destroy(out);
		exit(-1);
	}
	if (time == -1) {
		while (1) {
			sleep(1);
		}
	} else {
		sleep(time);
	}
	fprintf(stdout, "destroying buffer\n");
	buffer_destroy(buffer);
	fprintf(stdout, "destroying socket\n");
	socket_destroy(in);
	fprintf(stdout, "destroying file\n");
	file_destroy(out);
	return 0;
}
