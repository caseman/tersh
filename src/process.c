#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include "process.h"

#define R 0
#define W 1

#define READ_BUFFER_SIZE 1024

static int process_fork(process_t *child) {
    int in_pipe[2], out_pipe[2], err_pipe[2];
    if (pipe(in_pipe) != 0) return -1;
    if (pipe(out_pipe) != 0) return -1;
    if (pipe(err_pipe) != 0) return -1;
    int pid = fork();
    if (pid == 0) {
        // Child
        dup2(in_pipe[R], STDIN_FILENO);
        dup2(out_pipe[W], STDOUT_FILENO);
        dup2(err_pipe[W], STDERR_FILENO);
    }
    if (pid <= 0) {
        // Child or fork error
        close(in_pipe[W]);
        close(out_pipe[R]);
        close(err_pipe[R]);
    }
    if (pid > 0) {
        // Parent
        child->pid = pid;
        child->in_fd = in_pipe[W];
        child->out_fd = out_pipe[R];
        child->err_fd = err_pipe[R];

    }
    close(in_pipe[R]);
    close(out_pipe[W]);
    close(err_pipe[W]);
    return pid;
}

process_t *process_spawn(process_mgr_t *mgr, void* ref, const char *file, char *const argv[]) {
    assert(file);
    assert(argv);
    process_t *child_p = calloc(1, sizeof(process_t));
    if (child_p == NULL) return NULL;
    child_p->ref = ref;
    int pid = process_fork(child_p);
    if (pid < 0) {
        free(child_p);
        return NULL;
    }
    if (pid > 0) {
        vec_push(&mgr->processes, child_p);
        return child_p;
    }
    execvp(file, argv);
    exit(errno);
}

void process_del(process_mgr_t *mgr, process_t *p) {
    assert(p->exit_status);
    int i;
    process_t *child;
    close(p->in_fd);
    close(p->out_fd);
    close(p->err_fd);
    vec_deinit(&p->write_buf);
    vec_foreach(&mgr->processes, child, i) {
        if (p == child) {
            vec_del(&mgr->processes, i);
            break;
        }
    }
    free(p);
}

void process_write(process_t *p, unsigned char *data, int data_len) {
    vec_pusharr(&p->write_buf, data, data_len);
}

static void do_write_in(process_mgr_t *mgr, process_t *p) {
    ssize_t retval;
    do {
        retval = write(p->in_fd, p->write_buf.data, p->write_buf.length);
    } while (retval < 0 && errno == EINTR);
    if (retval < 0) {
        mgr->event_cb(p, p->in_fd, PROCESS_WRITE_ERR, errno);
        return;
    }
    if (retval < p->write_buf.length) {
        vec_splice(&p->write_buf, 0, retval);
    } else {
        vec_clear(&p->write_buf);
    }
    mgr->event_cb(p, p->in_fd, PROCESS_WRITE, retval);
}

static void do_read_fd(process_mgr_t *mgr, process_t *p, int fd) {
    ssize_t retval;
    unsigned char buf[READ_BUFFER_SIZE];
    do {
        retval = read(fd, buf, READ_BUFFER_SIZE);
    } while (retval < 0 && errno == EINTR);
    if (retval < 0) {
        mgr->event_cb(p, fd, PROCESS_READ_ERR, errno);
        return;
    }
    if (retval) {
        mgr->data_cb(p, fd, buf, retval);
    }
}

int process_poll(process_mgr_t *mgr, int timeout) {
    int n_fds = mgr->processes.length * 3;
    struct pollfd pollfds[n_fds];
    int i, status, retval, count;
    pid_t pid;
    process_t *p;

    do {
        if (!n_fds) break;
        pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            vec_foreach(&mgr->processes, p, i) {
                assert(p != NULL);
                if (pid == p->pid) {
                    p->exit_status = status;
                    mgr->event_cb(p, -1, PROCESS_EXITED, status);
                    break;
                }
            }
        }
        if (pid < 0) {
            if (errno == ECHILD) {
                // No child processes
                break;
            }
            mgr->event_cb(NULL, -1, PROCESS_WAITPID_ERR, errno);
        }
    } while (pid > 0);

    int f = 0;
    vec_foreach(&mgr->processes, p, i) {
        pollfds[f++] = (struct pollfd){ .fd = p->in_fd, .events = POLLOUT };
        pollfds[f++] = (struct pollfd){ .fd = p->out_fd, .events = POLLIN };
        pollfds[f++] = (struct pollfd){ .fd = p->err_fd, .events = POLLIN };
    }
    do {
        retval = poll(pollfds, n_fds, timeout);
    } while (retval < 0 && errno == EINTR);
    if (retval <= 0) return retval;

    count = retval;
    f = 0;
    vec_foreach(&mgr->processes, p, i) {
        // save all revent flags first so the callbacks can see them all
        p->in_revents = pollfds[f++].revents;
        p->out_revents = pollfds[f++].revents;
        p->err_revents = pollfds[f++].revents;
        if (p->in_revents) {
            mgr->event_cb(p, p->in_fd, PROCESS_POLL_EVENT, p->in_revents);
            if (p->in_revents & POLLOUT && p->write_buf.length) {
                do_write_in(mgr, p);
            }
        }
        if (p->out_revents) {
            mgr->event_cb(p, p->out_fd, PROCESS_POLL_EVENT, p->out_revents);
            if (p->out_revents & POLLIN) {
                do_read_fd(mgr, p, p->out_fd);
            }
        }
        if (p->err_revents) {
            mgr->event_cb(p, p->err_fd, PROCESS_POLL_EVENT, p->err_revents);
            if (p->out_revents & POLLIN) {
                do_read_fd(mgr, p, p->err_fd);
            }
        }
        count -= (p->in_revents != 0 || p->out_revents != 0 || p->err_revents != 0);
        if (count == 0) break;
    }
    return retval;
}
