/*
 * ev.h
 *
 *  Created on: Jan 14, 2015
 *      Author: elisescu
 */

#ifndef EV_H_
#define EV_H_

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/socket.h>
#endif
#include <stdio.h>
#include <pthread.h>

//#define DEBUG
#define ev_error(...)    fprintf(stderr, "\n"__VA_ARGS__); fflush(stderr)
#define ev_info(...)     fprintf(stdout, "\n"__VA_ARGS__); fflush(stdout)
#ifdef DEBUG
#define ev_vb(...)       fprintf(stdout, "\n"__VA_ARGS__); fflush(stdout)
#else
#define ev_vb(...)
#endif

enum {
    EV_OK = 0,
    EV_ERROR = 1
};

enum {
    EVBREAK_ALL = 0
};

enum {
    EV_READ = 1,
    EV_WRITE = 2,
    EV_READ_WRITE = EV_READ | EV_WRITE
};

extern struct ev_loop *EV_DEFAULT;

struct ev_io;
typedef void (*io_callbacks)(struct ev_loop *loop, struct ev_io *w, int revents);

typedef struct ev_io {
    int flags;
    void *data;
    int fd;
    io_callbacks cb;
    int pending_op;
} ev_io;

struct ev_loop {
    int running;
    pthread_mutex_t mutex;
    void *ev_ios;
    int no_fds;
    fd_set readfds;
    fd_set writefds;
};

int ev_io_init(ev_io *io, io_callbacks io_cb, int fd, int flag);
void ev_io_start(struct ev_loop *loop, ev_io *io);
void ev_io_stop(struct ev_loop *loop, ev_io *w);
void ev_run(struct ev_loop *loop, int flags);
void ev_break(struct ev_loop *loop, int how);

#endif /* EV_H_ */
