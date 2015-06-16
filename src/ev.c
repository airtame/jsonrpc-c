/*
 * ev.c
 *
 *  Created on: Jan 14, 2015
 *      Author: elisescu
 */


#include "ev.h"
#include <stdlib.h>
#ifndef _WIN32
#include <sys/select.h>
#else
#include <winsock2.h>
#endif
#include <errno.h>

// PRIVATE DECLARATIONS
enum {
    IO_PENDING_ADD,
    IO_PENDING_REMOVE
};

typedef struct list {
    struct list *next;
    ev_io *io;
} list_t;

#define LIST(l) ((list_t*)(l))
#define LIST_NEW_EL(new_el, el_data) ((new_el) = malloc(sizeof(list_t)), (new_el)->io = (el_data), new_el)
#define LIST_ITERATE(list) (list_t *el = LIST(list); el != NULL; el = el->next)
#define LIST_ITERATOR() (el)

#define SELECT_TIMEOUT_SEC (1)

static list_t first_el = {
        .next = NULL,
        .io = NULL
};

static struct ev_loop EV_DEFAULT_S = {
        .running = 0,
        .ev_ios = NULL,
        .no_fds = 0
};

void loop_add_io(struct ev_loop *loop, ev_io *io);
void loop_remove_io(struct ev_loop *loop, ev_io *io);
void loop_display_ios(struct ev_loop *loop);


// PUBLIC API functions or declarations
struct ev_loop *EV_DEFAULT = &EV_DEFAULT_S;

int ev_io_init(ev_io *io, io_callbacks io_cb, int fd, int flag) {
    ev_vb("Init ev_io with fd = %d", fd);
    io->cb = io_cb;
    io->fd = fd;
    io->flags = flag;
    return EV_OK;
}
void ev_io_start(struct ev_loop *loop, ev_io *io) {
    ev_vb("Starting ev_io with fd = %d", io->fd);

    pthread_mutex_lock(&loop->mutex);
    loop_add_io(loop, io);
    pthread_mutex_unlock(&loop->mutex);

#ifdef DEBUG
    ev_vb("Currently having %d io events in the loop: ", loop->no_fds);
    loop_display_ios(loop);
#endif
}

void ev_io_stop(struct ev_loop *loop, ev_io *io) {
    ev_vb("Stopping ev_io with fd = %d", io->fd);
    pthread_mutex_lock(&loop->mutex);
    loop_remove_io(loop, io);
    pthread_mutex_unlock(&loop->mutex);
#ifdef DEBUG
    ev_vb("Currently having %d io events in the loop: ", loop->no_fds);
    loop_display_ios(loop);
#endif
}

void ev_run(struct ev_loop *loop, int flags) {
    ev_vb("Starting the loop");
    ev_io** cbs;
    struct timeval tv;

    pthread_mutex_init(&loop->mutex, NULL);
    loop->running = 1;

    while (1) {
        pthread_mutex_lock(&loop->mutex);
        int r = loop->running;
        pthread_mutex_unlock(&loop->mutex);
        if (!r) {
            ev_vb("Stopping the loop...");
            break;
        }

        FD_ZERO(&loop->readfds);
        FD_ZERO(&loop->writefds);

        pthread_mutex_lock(&loop->mutex);
        for (list_t *el = ((list_t*)(loop->ev_ios)); el != NULL; el = el->next) {
            if (el->io->flags | EV_READ)  FD_SET(el->io->fd, &loop->readfds);
            if (el->io->flags | EV_WRITE)  FD_SET(el->io->fd, &loop->writefds);
        }
        pthread_mutex_unlock(&loop->mutex);

        // wait for one or more socket being ready
        tv.tv_sec = SELECT_TIMEOUT_SEC;
        tv.tv_usec = 0;
        int rc= select(FD_SETSIZE, &loop->readfds, NULL, NULL, &tv);
        if (rc == 0) continue;
        if (rc < 0) {
            if (errno == EINTR) continue;
            ev_error("Error when doing select on our sockets");
            perror("select");
            return;
        }

        ev_vb("Got event on %d sockets out of %d monitoring", rc, loop->no_fds);

        // loop over our sockets
        cbs = (ev_io**) malloc(rc*sizeof(ev_io *));
        int no = 0;
        pthread_mutex_lock(&loop->mutex);
        for (list_t *el = ((list_t*)(loop->ev_ios)); el != NULL; el = el->next) {
            if (FD_ISSET(el->io->fd, &loop->readfds) && el->io->flags | EV_READ) {
                cbs[no]=el->io;
                no++;
            }
        }
        pthread_mutex_unlock(&loop->mutex);

        for (int i = 0; i < no; i++) {
            ev_vb("Calling the callback for fd %d ", cbs[i]->fd);
            cbs[i]->cb(loop, cbs[i], 0);
        }
        free(cbs);
    }
}

void ev_break(struct ev_loop *loop, int how) {
    ev_vb("Stopping the loop");
    pthread_mutex_lock(&loop->mutex);
    loop->running = 0;
    pthread_mutex_unlock(&loop->mutex);
}

// PRIVATE FUNCTIONS
void loop_add_io(struct ev_loop *loop, ev_io *io) {
    // first element
    if (loop->no_fds == 0) {
        list_t *first_el = LIST_NEW_EL(first_el, io);
        first_el->next = NULL;
        loop->ev_ios = (void*) first_el;
        loop->no_fds = 1;
    } else {
        list_t *new_el = LIST_NEW_EL(new_el, io);
        new_el->next = LIST(loop->ev_ios);
        loop->ev_ios = (void *) new_el;
        loop->no_fds++;
    }
}

void loop_display_ios(struct ev_loop *loop) {
    for (list_t *el = LIST(loop->ev_ios); el != NULL; el = el->next) {
        ev_info("element: %d", el->io->fd);
    }
}

void loop_remove_io(struct ev_loop *loop, ev_io *io) {
    for (list_t *el = LIST(loop->ev_ios); el->next != NULL; el = el->next) {
        // found it in the middle
        if (el->next != NULL && el->next->io == io) {
            ev_vb("Found element after first pos. Removing it...");
            list_t *ol  = el->next;
            el->next = el->next->next;
            free(ol);
            loop->no_fds--;
            break;
        } else if (el->io == io) { // found it on the first pos
            ev_vb("Found element on the first pos. Removing it...");
            list_t *ol = LIST(loop->ev_ios);
            loop->ev_ios = (void *) el->next;
            free(ol);
            loop->no_fds--;
            break;
        }
    }
}
