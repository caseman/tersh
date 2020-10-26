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
 *
 * val is either the exit status value as returned by waitpid() for the
 * child process, or the pollfd.events value returned by poll for the
 * associated file.
 */
typedef void (*poller_cb)(int fd, void *data, poller_event_t event, int val);

/*
 * Add a new file and child pid to poll, associated with
 * the data pointer. Events on the file and process are sent to cb.
 * This becomes the new forground process and fd.
 *
 * When the poller receives a HUP event for the file, and the child process
 * exits, it is automatically removed from the poller.
 *
 * Returns 0 on success and -1 on error, setting errno.
 */
int poller_add(int fd, pid_t pid, void *data, poller_cb cb);

/*
 * Returns the data pointer for the current foreground process, or
 * NULL if there is no currently running fg process.
 */
void *poller_getfg();

/*
 * Poll all active files and child processes in the poller and dispatch
 * callbacks for any events available.
 *
 * The poller will wait until the timeout for an event to occur, but may
 * return early if events are available sooner.
 */
int poller_poll(int timeout);

#endif
