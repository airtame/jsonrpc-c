/*
 * ev.c
 *
 *  Created on: Jan 14, 2015
 *      Author: elisescu
 */


#include "ev.h"
#include <stdlib.h>

typedef struct list {
    struct list *next;
    ev_io *io;
} list_t;

#define LIST(l) ((list_t*)(l))
#define LIST_INSERT(list, eviodata) { \
    do { \
        list_t *el = LIST(list)->next == NULL ? LIST(list) : (list_t *)malloc(sizeof(list_t)); \
        el->io = eviodata; \
        el->next = LIST(list); \
        list = el; \
    } while (0);\
}

static list_t first_el = {
        .next = NULL,
        .io = NULL
};

struct ev_loop EV_DEFAULT_S = {
        .running = 0,
        .ev_ios = (void *) &first_el
};

struct ev_loop *EV_DEFAULT = &EV_DEFAULT_S;

// PUBLIC API functions
int ev_io_init(ev_io *io, io_callbacks io_cb, int fd, int flag) {
    ev_vb("Init ev_io with fd = %d", fd);
    if (fd > FD_SETSIZE || fd < 1) {
        ev_error("invalid file descriptor");
        return EV_ERROR;
    }
    io->cb = io_cb;
    io->fd = fd;
    return EV_OK;
}

void ev_io_start(struct ev_loop *loop, ev_io *io) {
    ev_vb("Starting ev_io with fd = %d", io->fd);
    LIST_INSERT(loop->ev_ios, io);
}

void ev_io_stop(struct ev_loop *loop, ev_io *io) {
    ev_vb("Stopping ev_io with fd = %d", io->fd);
    //loop->ev_ios[io->fd] = NULL;
}

void ev_run(struct ev_loop *loop, int flags) {
    ev_vb("Starting the loop");
    fd_set active_fd_set, read_fd_set;
    int i;

    pthread_mutex_init(&loop->mutex, NULL);
    FD_ZERO (&active_fd_set);
    loop->running = 1;

    while (1) {
        pthread_mutex_lock(&loop->mutex);
        int r = loop->running;
        pthread_mutex_unlock(&loop->mutex);
        if (!r) {
            ev_vb("Stopping the loop...");
            break;
        }

    }
}

void ev_break(struct ev_loop *loop, int how) {
    ev_vb("Stopping the loop");
    pthread_mutex_lock(&loop->mutex);
    loop->running = 0;
    pthread_mutex_unlock(&loop->mutex);
}