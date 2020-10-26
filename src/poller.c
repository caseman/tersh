#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include "poller.h"

struct pollee {
    int fd;
    pid_t pid;
    void *data;
    poller_cb cb;
    int exit_status;
};

vec_t(struct pollee) pollees = NULL_VEC;

int poller_add(int fd, pid_t pid, void* data, poller_cb cb) {
    int err;
    assert(cb);
    err = vec_push(&pollees, ((struct pollee){ fd, pid, data, cb }));
    return err;
}

void *poller_getfg() {
    for (int i = pollees.length - 1; i >= 0; i--) {
        if (!pollees.data[i].exit_status) {
            return pollees.data[i].data;
        }
    }
    return NULL;
}

int poller_poll(int timeout) {
    int i, status, retval, count;
    pid_t pid;
    struct pollfd pollfds[pollees.length];
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
        }
    }

    for (i = 0; i < pollees.length; i++, p++) {
        pollfds[i].fd = pollees.data[i].fd;
        pollfds[i].events = POLLIN | POLLOUT;
        pollfds[i].revents = 0;
    }

    retval = poll(pollfds, pollees.length, timeout);
    if (retval <= 0) return retval;

    count = retval;
    p = pollees.data;
    for (i = 0; i < pollees.length; i++, p++) {
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
            p->cb(p->fd, p->data, POLLER_EVENTS, revents);
            if (--count == 0) break;
        }
    }

    return retval;
}
