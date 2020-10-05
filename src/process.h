#include <sys/types.h>

typedef struct {
    pid_t pid;
    int in;
    int out;
    int err;
} process;

int process_fork(process *child);
