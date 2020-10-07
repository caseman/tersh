#include <sys/types.h>

typedef struct {
    pid_t pid;
    int in;
    int out;
    int err;
} process_t;

int process_fork(process_t *child);
int process_spawn(process_t *child, const char *file, char *const argv[]);
