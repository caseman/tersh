#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include "poller.h"

struct pollee {
    int fd;
    void *data;
    poller_cb cb;
};

vec_t(struct pollee) pollees = NULL_VEC;

int poller_add(int fd, void* data, poller_cb cb) {
    int err;
    assert(cb);
    err = vec_push(&pollees, ((struct pollee){ fd, data, cb }));
    return err;
}

void *poller_getfg() {
    for (int i = pollees.length - 1; i >= 0; i--) {
        if (pollees.data[i].fd >= 0) {
            return pollees.data[i].data;
        }
    }
    return NULL;
}

int poller_poll(int timeout) {
    int i, retval, count;
    struct pollfd pollfds[pollees.length];
    struct pollee *p;

    // Prune closed fds
    for (i = pollees.length - 1; i >= 0; i--) {
        if (pollees.data[i].fd < 0) {
            vec_del(&pollees, i);
        }
    }

    for (i = 0; i < pollees.length; i++) {
        pollfds[i].fd = pollees.data[i].fd;
        pollfds[i].events = POLLIN | POLLOUT;
        pollfds[i].revents = 0;
    }

    retval = poll(pollfds, pollees.length, timeout);
    if (retval <= 0) return retval;

    count = retval;
    for (i = 0, p = pollees.data; i < pollees.length; i++, p++) {
        short revents = pollfds[i].revents;
        if (revents) {
            /*
            printf("fd=%d events:", p->fd);
            if (revents & POLLIN) printf(" POLLIN");
            if (revents & POLLOUT) printf(" POLLOUT");
            if (revents & POLLNVAL) printf(" POLLNVAL");
            if (revents & POLLHUP) printf(" POLLHUP");
            if (revents & POLLERR) printf(" POLLERR");
            printf("\n");
            */
            if (revents & POLLNVAL && p->fd > 0) {
                // fd was closed somewhere, mark it closed
                p->fd = -p->fd;
            }
            p->cb(p->fd, p->data, revents);
            if (--count == 0) break;
        }
    }

    return retval;
}
