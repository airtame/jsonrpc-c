/*
 * ev.h
 *
 *  Created on: Jan 14, 2015
 *      Author: elisescu
 */

#ifndef EV_H_
#define EV_H_

typedef struct ev_io {
    int nothing_now;
} ev_io;

struct ev_loop {
    int nothing_now;
};

void ev_io_stop(struct ev_loop *loop, ev_io w);

#endif /* EV_H_ */
