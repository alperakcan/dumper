/*
 *  Copyright (c) 2013-2014 Alper Akcan <alper.akcan@gmail.com>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#ifndef __SYNC_H___
#define __SYNC_H___

struct mutex;

struct mutex * mutex_create (void);
int mutex_destroy (struct mutex *mutex);
int mutex_lock (struct mutex *mutex);
int mutex_unlock (struct mutex *mutex);

struct cond;

struct cond * cond_create (struct mutex *mutex);
int cond_destroy (struct cond *cond);
int cond_signal (struct cond *cond);
int cond_broadcast (struct cond *cond);
int cond_wait (struct cond *cond);

struct thread;

struct thread * thread_create (void * (*function) (void *arg), void *arg);
int thread_join (struct thread *thread);

#endif
