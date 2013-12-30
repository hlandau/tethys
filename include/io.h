/* ircd-micro, io.h -- async IO loop
   Copyright (C) 2013 Alex Iadicicco

   This file is protected under the terms contained
   in the COPYING file in the project root */

#ifndef __INC_IO_H__
#define __INC_IO_H__

#include "ircd.h"
#include "list.h"

typedef struct u_io u_io;
typedef struct u_io_fd u_io_fd;
typedef struct u_io_timer u_io_timer;

struct u_io {
	int running;
	u_list fds;
	u_list timers;
};

struct u_io_fd {
	u_io *io;
	u_list *n;
	int fd;
	void *priv;
	void (*recv)(u_io_fd*);
	void (*send)(u_io_fd*);
	int (*post)(u_io_fd*);
};


typedef void (u_io_timer_cb_t)(u_io_timer*);

struct u_io_timer {
	u_io *io;
	u_list *n;
	struct timeval time;
	/* u_io_timer* */
	u_io_timer_cb_t *cb;
	void *priv;
};

extern struct timeval NOW;

extern void u_io_init(u_io*);
extern u_io_fd *u_io_add_fd(u_io*, int);
extern u_io_timer *u_io_add_timer(u_io*, ulong sec, ulong usec,
                                  u_io_timer_cb_t *cb, void *priv);
extern void u_io_del_timer(u_io_timer*);

extern void u_io_poll_once(u_io*);
extern void u_io_poll_break(u_io*);
extern void u_io_poll(u_io*);

#endif
