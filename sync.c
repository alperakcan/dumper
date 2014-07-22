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
#include <stdint.h>

#include <pthread.h>

#if defined(SYNC_USE_FUTEX) && (SYNC_USE_FUTEX == 1)
#error futex

#include <limits.h>
#include <sys/syscall.h>
#include <linux/futex.h>

#endif

#include "sync.h"

#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)

#if defined(SYNC_USE_FUTEX) && (SYNC_USE_FUTEX == 1)

struct mutex {
	uint32_t mutex;
};

struct mutex * mutex_create (void)
{
	struct mutex *mutex;
	mutex = malloc(sizeof(struct mutex));
	if (unlikely(mutex == NULL)) {
		fprintf(stderr, "can not allocate memory for mutex\n");
		return NULL;
	}
	memset(mutex, 0, sizeof(struct mutex));
	mutex->mutex = 0;
	return mutex;
}

int mutex_destroy (struct mutex *mutex)
{
	if (unlikely(mutex == NULL)) {
		fprintf(stderr, "invalid mutex\n");
		return -1;
	}
	mutex->mutex = 0;
	free(mutex);
	return 0;
}

int mutex_lock (struct mutex *mutex)
{
	uint32_t c;
	if (unlikely(mutex == NULL)) {
		fprintf(stderr, "invalid mutex\n");
		return -1;
	}
	if ((c = __sync_val_compare_and_swap(&mutex->mutex, 0, 1)) != 0) {
		do {
			if ((c == 2) || __sync_val_compare_and_swap(&mutex->mutex, 1, 2) != 0) {
				syscall(SYS_futex, &mutex->mutex, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
			}
		} while ((c = __sync_val_compare_and_swap(&mutex->mutex, 0, 2)) != 0);
	}
	return 0;
}

int mutex_unlock (struct mutex *mutex)
{
	if (unlikely(mutex == NULL)) {
		fprintf(stderr, "invalid mutex\n");
		return -1;
	}
	if (__sync_fetch_and_sub(&mutex->mutex, 1) != 1) {
		mutex->mutex = 0;
		syscall(SYS_futex, &mutex->mutex, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
	}
	return 0;
}

struct cond	{
	struct mutex *mutex;
	int seq;
};

struct cond * cond_create (struct mutex *mutex)
{
	struct cond *cond;
	if (unlikely(mutex == NULL)) {
		fprintf(stderr, "invalid mutex\n");
		return NULL;
	}
	cond = malloc(sizeof(struct cond));
	if (unlikely(cond == NULL)) {
		fprintf(stderr, "can not allocate memory for cond\n");
		return NULL;
	}
	memset(cond, 0, sizeof(struct cond));
	cond->mutex = mutex;
	cond->seq = 0;
	return cond;
}

int cond_destroy (struct cond *cond)
{
	if (unlikely(cond == NULL)) {
		fprintf(stderr, "invalid cond\n");
		return -1;
	}
	cond->mutex = NULL;
	cond->seq = 0;
	free(cond);
	return 0;
}

int cond_signal (struct cond *cond)
{
	if (unlikely(cond == NULL)) {
		fprintf(stderr, "invalid cond\n");
		return -1;
	}
	__sync_fetch_and_add(&(cond->seq), 1);
	syscall(SYS_futex, &(cond->seq), FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
	return 0;
}

int cond_broadcast (struct cond *cond)
{
	if (unlikely(cond == NULL)) {
		fprintf(stderr, "invalid cond\n");
		return -1;
	}
	__sync_fetch_and_add(&(cond->seq), 1);
	syscall(SYS_futex, &(cond->seq), FUTEX_REQUEUE_PRIVATE, 1, (void *) INT_MAX, &cond->mutex->mutex, 0);
	return 0;
}

static __inline uint32_t xchg32 (volatile uint32_t *addr, uint32_t newval)
{
	uint32_t result;
	asm volatile(
		"lock; xchgl %0, %1" :
		"+m" (*addr), "=a" (result) :
		"1" (newval) :
		"cc"
	);
	return result;
}

int cond_wait (struct cond *cond)
{
	if (unlikely(cond == NULL)) {
		fprintf(stderr, "invalid cond\n");
		return -1;
	}
	uint32_t oldSeq = cond->seq;
	mutex_unlock(cond->mutex);
	syscall(SYS_futex, &(cond->seq), FUTEX_WAIT_PRIVATE, oldSeq, NULL, NULL, 0);
	while (xchg32(&cond->mutex->mutex, 2))	{
		syscall(SYS_futex, &cond->mutex->mutex, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
	}
	return 0;
}

#else

struct mutex {
	pthread_mutex_t mutex;
};

struct mutex * mutex_create (void)
{
	int rc;
	struct mutex *mutex;
	mutex = malloc(sizeof(struct mutex));
	if (unlikely(mutex == NULL)) {
		fprintf(stderr, "can not allocate memory for mutex\n");
		return NULL;
	}
	memset(mutex, 0, sizeof(struct mutex));
	rc = pthread_mutex_init(&mutex->mutex, NULL);
	if (unlikely(rc != 0)) {
		fprintf(stderr, "can not initialize mutex\n");
		free(mutex);
		return NULL;
	}
	return mutex;
}

int mutex_destroy (struct mutex *mutex)
{
	if (unlikely(mutex == NULL)) {
		fprintf(stderr, "invalid mutex\n");
		return -1;
	}
	pthread_mutex_destroy(&mutex->mutex);
	free(mutex);
	return 0;
}

int mutex_lock (struct mutex *mutex)
{
	int rc;
	if (unlikely(mutex == NULL)) {
		fprintf(stderr, "invalid mutex\n");
		return -1;
	}
	rc = pthread_mutex_lock(&mutex->mutex);
	if (unlikely(rc != 0)) {
		fprintf(stderr, "can not lock mutex\n");
		return -1;
	}
	return 0;
}

int mutex_unlock (struct mutex *mutex)
{
	int rc;
	if (unlikely(mutex == NULL)) {
		fprintf(stderr, "invalid mutex\n");
		return -1;
	}
	rc = pthread_mutex_unlock(&mutex->mutex);
	if (unlikely(rc != 0)) {
		fprintf(stderr, "can not unlock mutex\n");
		return -1;
	}
	return 0;
}

struct cond {
	struct mutex *mutex;
	pthread_cond_t cond;
};

struct cond * cond_create (struct mutex *mutex)
{
	int rc;
	struct cond *cond;
	if (unlikely(mutex == NULL)) {
		fprintf(stderr, "invalid mutex\n");
		return NULL;
	}
	cond = malloc(sizeof(struct cond));
	if (unlikely(cond == NULL)) {
		fprintf(stderr, "can not allocate memory for cond\n");
		return NULL;
	}
	memset(cond, 0, sizeof(struct cond));
	rc = pthread_cond_init(&cond->cond, NULL);
	if (unlikely(rc != 0)) {
		fprintf(stderr, "can not initialize cond\n");
		free(cond);
		return NULL;
	}
	cond->mutex = mutex;
	return cond;
}

int cond_destroy (struct cond *cond)
{
	if (unlikely(cond == NULL)) {
		fprintf(stderr, "invalid cond\n");
		return -1;
	}
	pthread_cond_destroy(&cond->cond);
	free(cond);
	return 0;
}

int cond_signal (struct cond *cond)
{
	int rc;
	if (unlikely(cond == NULL)) {
		fprintf(stderr, "invalid cond\n");
		return -1;
	}
	rc = pthread_cond_signal(&cond->cond);
	if (unlikely(rc != 0)) {
		fprintf(stderr, "can not signal cond\n");
		return -1;
	}
	return 0;
}

int cond_broadcast (struct cond *cond)
{
	int rc;
	if (unlikely(cond == NULL)) {
		fprintf(stderr, "invalid cond\n");
		return -1;
	}
	rc = pthread_cond_broadcast(&cond->cond);
	if (unlikely(rc != 0)) {
		fprintf(stderr, "can not broadcast cond\n");
		return -1;
	}
	return 0;
}

int cond_wait (struct cond *cond)
{
	int rc;
	if (unlikely(cond == NULL)) {
		fprintf(stderr, "invalid cond\n");
		return -1;
	}
	rc = pthread_cond_wait(&cond->cond, &cond->mutex->mutex);
	if (unlikely(rc != 0)) {
		fprintf(stderr, "can not broadcast cond\n");
		return -1;
	}
	return 0;
}

#endif

struct thread {
	pthread_t thread;
};

struct thread * thread_create (void * (*function) (void *arg), void *arg)
{
	int rc;
	struct thread *thread;
	if (unlikely(function == NULL)) {
		fprintf(stderr, "invalid mutex\n");
		return NULL;
	}
	thread = malloc(sizeof(struct thread));
	if (unlikely(thread == NULL)) {
		fprintf(stderr, "can not allocate memory for thread\n");
		return NULL;
	}
	memset(thread, 0, sizeof(struct thread));
	rc = pthread_create(&thread->thread, NULL, function, arg);
	if (rc != 0) {
		fprintf(stderr, "can not create thread\n");
		free(thread);
		return NULL;
	}
	return thread;
}

int thread_join (struct thread *thread)
{
	int rc;
	if (unlikely(thread == NULL)) {
		fprintf(stderr, "invalid thread\n");
		return -1;
	}
	rc = pthread_join(thread->thread, NULL);
	if (rc != 0) {
		fprintf(stderr, "can not join thread\n");
		free(thread);
		return -1;
	}
	free(thread);
	return 0;
}
