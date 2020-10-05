#include <unistd.h>
#include <fcntl.h>
#include "process.h"

#define R 0
#define W 1

int process_fork(process *child) {
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
        child->in = in_pipe[W];
        child->out = out_pipe[R];
        child->err = err_pipe[R];

    }
    close(in_pipe[R]);
    close(out_pipe[W]);
    close(err_pipe[W]);
    return pid;
}
