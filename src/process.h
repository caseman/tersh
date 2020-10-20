#include <sys/types.h>
#include "poll.h"
#include "vec.h"

#ifndef PROCESS_H
#define PROCESS_H

typedef enum {
    PROCESS_EXITED = 1,
    PROCESS_POLL_EVENT = 2,
    PROCESS_WRITE = 3,
    PROCESS_WRITE_ERR = 4,
    PROCESS_READ_ERR = 5,
    PROCESS_WAITPID_ERR = 6,
    PROCESS_CLOSED_FD = 7,
} process_event_t;

typedef struct process process_t;

typedef int (*process_data_cb)(process_t *p, int fd, unsigned char *data, int data_len);
typedef void (*process_event_cb)(process_t *p, int fd, process_event_t event, intmax_t val);

struct process {
    pid_t pid;
    int in_fd, out_fd, err_fd;
    short in_revents, out_revents, err_revents;
    int exit_status;
    vec_uchar_t write_buf;
    void *ref;
};

typedef vec_t(process_t *) vec_process_t;
typedef vec_t(struct pollfd) vec_pollfd_t;

typedef struct {
    vec_process_t processes;
    process_data_cb data_cb;
    process_event_cb event_cb;
} process_mgr_t;

process_t *process_spawn(process_mgr_t *mgr, void* ref, const char *file, char *const argv[]);
void process_del(process_mgr_t *mgr, process_t *p);
void process_write(process_t *p, unsigned char *data, int data_len);
int process_poll(process_mgr_t *mgr, int timeout);

#endif
