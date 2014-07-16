#ifndef __SYNC__
#define __SYNC__

#if defined(ENABLE_FUTEX) && (ENABLE_FUTEX == 1)

#include <unistd.h>
#include <limits.h>
#include <sys/syscall.h>
#include <linux/futex.h>

typedef uint32_t mutex;
typedef struct condvar condvar;

struct condvar	{
	mutex *m;
	int seq;
};

void mutex_init (mutex *m)
{
	*m = 0;
}

void mutex_destroy (mutex *m)
{
	*m = 0;
}

void mutex_lock (mutex *m)
{
	uint32_t c;
	if ((c = __sync_val_compare_and_swap(m, 0, 1)) != 0) {
		do {
			if ((c == 2) || __sync_val_compare_and_swap(m, 1, 2) != 0) {
				syscall(SYS_futex, m, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
			}
		} while ((c = __sync_val_compare_and_swap(m, 0, 2)) != 0);
	}
}

void mutex_unlock (mutex *m)
{
	if (__sync_fetch_and_sub(m, 1) != 1) {
		*m = 0;
		syscall(SYS_futex, m, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
	}
}

void cond_init (condvar *c, mutex *m)
{
	c->m = m;
	c->seq = 0;
}

void cond_destroy (condvar *c)
{
	c->m = NULL;
	c->seq = 0;
}

void cond_signal (condvar *c)
{
	__sync_fetch_and_add(&(c->seq), 1);
	syscall(SYS_futex, &(c->seq), FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
}

void cond_broadcast (condvar *c)
{
	__sync_fetch_and_add(&(c->seq), 1);
	syscall(SYS_futex, &(c->seq), FUTEX_REQUEUE_PRIVATE, 1, (void *) INT_MAX, c->m, 0);
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

void cond_wait (condvar *c)
{
	uint32_t oldSeq = c->seq;
	mutex_unlock(c->m);
	syscall(SYS_futex, &(c->seq), FUTEX_WAIT_PRIVATE, oldSeq, NULL, NULL, 0);
	while (xchg32(c->m, 2))	{
		syscall(SYS_futex, c->m, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
	}
}

#else

#include <pthread.h>

typedef pthread_mutex_t mutex;
typedef struct condvar condvar;

struct condvar	{
	mutex *m;
	pthread_cond_t c;
};

void mutex_init (mutex *m)
{
	pthread_mutex_init(m, NULL);
}

void mutex_destroy (mutex *m)
{
	pthread_mutex_destroy(m);
}

void mutex_lock (mutex *m)
{
	pthread_mutex_lock(m);
}

void mutex_unlock (mutex *m)
{
	pthread_mutex_unlock(m);
}

void cond_init (condvar *c, mutex *m)
{
	c->m = m;
	pthread_cond_init(&c->c, NULL);
}

void cond_destroy (condvar *c)
{
	c->m = NULL;
	pthread_cond_destroy(&c->c);
}

void cond_signal (condvar *c)
{
	pthread_cond_signal(&c->c);
}

void cond_broadcast (condvar *c)
{
	pthread_cond_broadcast(&c->c);
}

void cond_wait (condvar *c)
{
	pthread_cond_wait(&c->c, c->m);
}

#endif

#endif
