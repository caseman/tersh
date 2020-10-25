#include <sys/types.h>
#include "poll.h"
#include "vec.h"

#ifndef POLLER_H
#define POLLER_H

typedef enum {
    POLLER_CHILD_EXIT = 1,
    POLLER_EVENTS = 2,
} poller_event_t;

/*
 * Poller callback function that will be called when the child process
 * exits, or when the fd receives a poll event. Note that callbacks can
 * block, but should not retry on EINTR. Instead they should return
 * to give cycles to the event loop. They will be called again by the poller
 * when their fd is ready
 */
typedef void (*poller_cb)(int fd, void *data, poller_event_t event, int val);

int poller_add(int fd, pid_t pid, void* data, poller_cb cb);
int poller_poll(int timeout);

#endif
