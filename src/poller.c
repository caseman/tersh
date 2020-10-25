#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include "poller.h"

struct pollee {
    int fd;
    pid_t pid;
    void *data;
    poller_cb cb;
    int exit_status;
};

vec_t(struct pollfd) pollfds = NULL_VEC;
vec_t(struct pollee) pollees = NULL_VEC;

int poller_add(int fd, pid_t pid, void* data, poller_cb cb) {
    int err;
    assert(cb);
    err = vec_push(&pollfds, ((struct pollfd){ .fd = fd, .events = POLLIN | POLLOUT}));
    err |= vec_push(&pollees, ((struct pollee){ fd, pid, data, cb }));
    return err;
}

int poller_poll(int timeout) {
    int i, status, retval, count;
    pid_t pid;
    struct pollee *p;

    do {
        if (!pollees.length) break;
        pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            p = pollees.data;
            for (i = 0; i < pollees.length; i++, p++) {
                if (pid == p->pid) {
                    p->exit_status = status;
                    p->cb(p->fd, p->data, POLLER_CHILD_EXIT, status);
                    break;
                }
            }
        }
        if (pid < 0) {
            if (errno == ECHILD) {
                // No child processes
                break;
            }
            return pid;
        }
    } while (pid > 0);

    // Prune closed fds with dead pids
    for (i = pollees.length - 1; i >= 0; i--) {
        if (pollees.data[i].fd < 0 && pollees.data[i].exit_status) {
            vec_del(&pollees, i);
            vec_del(&pollfds, i);
        }
    }

    retval = poll(pollfds.data, pollfds.length, timeout);
    if (retval <= 0) return retval;

    count = retval;
    p = pollees.data;
    for (i = 0; i < pollees.length; i++, p++) {
        short events = pollfds.data[i].events;
        if (events) {
            if (events & POLLNVAL && p->fd > 0) {
                // fd was closed somewhere, mark it closed
                p->fd = -p->fd;
            }
            p->cb(p->fd, p->data, POLLER_EVENTS, events);
            pollfds.data[i].events = 0;
            if (--count == 0) break;
        }
    }

    return retval;
}
