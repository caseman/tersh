#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dlfcn.h>
#include <pthread.h>

#if   defined(__linux)
 #include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
 #include <util.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
 #include <libutil.h>
#endif

#include <mrsh/ast.h>
#include <mrsh/buffer.h>
#include <mrsh/builtin.h>
#include <mrsh/entry.h>
#include <mrsh/parser.h>
#include <mrsh/shell.h>
#include <shell/task.h>
#include <shell/process.h>
#include <shell/job.h>
#include "BearLibTerminal.h"

//#include "tersh.h"
#include "lineedit.h"
#include "vec.h"
#include "poller.h"
#include "widget.h"
#include "st_widget.h"
#include "st.h"
#include "ui.h"

#define FRAME_TIME 30

struct program_ctx {
    Term *term;
    struct mrsh_program *mrsh_prog;
    bool started;
    bool returned;
    int status;
};

struct mrsh_state *mrsh_state;
pthread_mutex_t mrsh_mutex = PTHREAD_MUTEX_INITIALIZER;
widget_t *root_w = NULL;
widget_t *line_ed_w = NULL;
bool running = true;
pid_t main_pid;
int orig_fds[3];
extern char **environ;

/* Ensure exit() calls in child processes do not have side effects */
extern void exit(int status) {
    _exit(status);
}

/* Ensure our direct children have the proper controlling tty */
void atfork_child() {
    if (getppid() == main_pid) {
        struct winsize w = {
            .ws_row = terminal_state(TK_HEIGHT),
            .ws_col = terminal_state(TK_WIDTH),
        };
        if (setsid() < 0) { // create a new process group
            perror("tersh: setsid failed to create a new session");
        }
        // This will fail if already done, so errors are ignored
        ioctl(STDIN_FILENO, TIOCSCTTY, NULL);
        if (ioctl(STDIN_FILENO, TIOCSWINSZ, &w) < 0) {
            perror("tersh: inctl(TIOCSWINSZ) failed to set window size");
        }
    }
}

typedef int (*setpgid_fn)(pid_t, pid_t);

/* Ensure mrsh cannot change our pgid, which breaks atfork_child */
extern int setpgid(pid_t pid, pid_t pgid) {
    if (getpid() == main_pid) {
        /* Block change on main tersh process */
        errno = EPERM;
        return -1;
    }
    setpgid_fn sys_setpgid = dlsym(RTLD_NEXT, "setpgid");
    return sys_setpgid(pid, pgid);
}

long long time_millis(void)
{
    struct timeval tv;
    long long msec;

    gettimeofday(&tv, NULL);

    msec = tv.tv_sec;
    msec *= 1000;
    msec += tv.tv_usec / 1000;

    return msec;
}

static void poll_events(bool delay) {
    static long long last_time = 0;
    long long now;
    static int dt = 0;
    static Term *last_term = NULL;
    Term *fg_term;

    if (last_time == 0) {
        last_time = time_millis();
    }

    if (!terminal_has_input()) {
        do {
            if (poller_poll(FRAME_TIME - dt) > 0 && !terminal_has_input() && delay) {
                terminal_delay(FRAME_TIME - dt);
            }
            now = time_millis();
            dt = now - last_time;
        } while (!terminal_has_input() && dt < FRAME_TIME);

        widget_update(root_w, dt);
        last_time = now;
        dt = 0;
    }

    struct mrsh_job *fg_job = job_get_foreground(mrsh_state);
    // if (fg_job) dprintf(orig_fds[1], "FG %p data=%p\n", fg_job, fg_job->data);
    if (fg_job != NULL && fg_job->data != NULL) {
        fg_term = fg_job->data;
    } else {
        fg_term = NULL;
    }
    if (fg_term != last_term) {
        if (last_term) st_set_focused(last_term, 0);
        if (fg_term) st_set_focused(fg_term, 1);
        last_term = fg_term;
    }

    if (terminal_has_input()) {
        int key = terminal_read();
        if (key == TK_CLOSE) {
            running = false;
            return;
        }
        if (key == TK_RESIZED) {
            root_w->min_width = terminal_state(TK_WIDTH),
                root_w->max_width = terminal_state(TK_WIDTH),
                root_w->min_height = terminal_state(TK_HEIGHT),
                root_w->max_height = terminal_state(TK_HEIGHT),
                widget_layout(root_w, 0, 0, terminal_state(TK_WIDTH), terminal_state(TK_HEIGHT));
            widget_draw(root_w);
            terminal_refresh();
            return;
        }
        if (fg_term) {
            char ch;
            switch (key) {
                case TK_RETURN:
                    ch = '\n';
                    break;
                case TK_TAB:
                    ch = '\t';
                    break;
                case TK_BACKSPACE:
                    ch = 127;
                    break;
                case TK_ESCAPE:
                    ch = '\e';
                    break;
                default:
                    ch = terminal_state(TK_CHAR);
            }
            if (ch) {
                ttywrite(fg_term, &ch, 1, 1);
            }
        } else {
            line_ed_w->cls->handle_ev(line_ed_w, key);
        }
    }
}

void poll_jobs() {
    if (pthread_mutex_trylock(&mrsh_mutex)) return;
    struct mrsh_job **jobs = mrsh_poll_jobs(mrsh_state);
    if (jobs != NULL) {
        for (int i = 0; jobs[i] != NULL; i++) {
            dprintf(orig_fds[1], "POLLED JOB %p\n", jobs[i]);
            Term *term = jobs[i]->data;
            if (jobs[i]->last_status >= 0) {
                st_set_child_status(term, jobs[i]->last_status);
            } else if (jobs[i]->last_status == TASK_STATUS_STOPPED) {
                term->childstopped = 1;
            } else if (jobs[i]->last_status == TASK_STATUS_WAIT) {
                term->childstopped = 0;
            }
        }
        free(jobs);
    }
    pthread_mutex_unlock(&mrsh_mutex);
}

static void refresh() {
    widget_relayout(root_w);
    terminal_clear();
    widget_draw(root_w);
    terminal_refresh();
}

static int init_program_pty(struct program_ctx *prog) {
    int m, s;
    mrsh_program_print(prog->mrsh_prog);

    // seems to work fine on linux, openbsd and freebsd
    if (openpty(&m, &s, NULL, NULL, NULL) < 0) {
        st_perror(prog->term, "tersh: openpty failed");
        return -1;
    }
    assert(m > 2 && s > 2); // Guard against std fds being accidentally closed
    int flags = fcntl(m, F_GETFL, 0);
    if (fcntl(m, F_SETFL, flags | O_CLOEXEC) < 0) {
        st_perror(prog->term, "tersh: fcntl failed to set pty flags");
    }
    if (dup2(s, 0) < 0 ||
        dup2(s, 1) < 0 ||
        dup2(s, 2) < 0) {
        // This may not be visible
        perror("tersh: dup2 failed, unable to use terminal");
        close(s);
        close(m);
        return -1;
    }
    close(s);
    prog->term->cmdfd = m;
    poller_add(m, prog->term, st_on_poll);
    return 0;
}

static void update_program(struct program_ctx *prog) {
    if (!prog->started) return;
    if (pthread_mutex_trylock(&mrsh_mutex)) return;
    if (!prog->returned) {
        pthread_mutex_unlock(&mrsh_mutex);
        return;
    }
    poll_events(0); // Ensure all data is read
    if (prog->status >= 0) {
        st_set_child_status(prog->term, prog->status);
    }
    for (int i = 0; i <= 2; i++) {
        close(i);
        if (dup2(orig_fds[i], i) < 0) {
            perror("tersh: dup2 failed restoring fd");
        }
    }
    prog->started = false;
    pthread_mutex_unlock(&mrsh_mutex);
}

static void *run_program_thread(void *data) {
    struct program_ctx *prog = data;
    int err = pthread_mutex_lock(&mrsh_mutex);
    if (err) {
        fprintf(stderr, "tersh: failed to get program lock: %s\n", strerror(err));
        prog->returned = true;
        pthread_exit(NULL);
    }
    mrsh_state->job_data = prog->term;
    prog->status = mrsh_run_program(mrsh_state, prog->mrsh_prog);
    prog->returned = true;
    pthread_mutex_unlock(&mrsh_mutex);
    pthread_exit(NULL);
}

void handle_sigalrm() {
    // TODO check current thread
    if (getpid() != main_pid) return;
    poll_events(false);
    refresh();
}

int main(int argc, char* argv[]) {
    pthread_t program_thread;
    main_pid = getpid();
    pthread_atfork(NULL, NULL, atfork_child);
    for (int i = 0; i <= 2; i++) {
        orig_fds[i] = dup(i);
    }
    char *path = strdup(argv[0]);
    char *dirpath = dirname(path);

    /* init mrsh */
    mrsh_state = mrsh_state_create();
    mrsh_state->options |= MRSH_OPT_MONITOR;
    mrsh_state->interactive = true;
    state_get_priv(mrsh_state)->job_control = true;
    if (!mrsh_populate_env(mrsh_state, environ)) {
        return 1;
    }
    /*
    signal(SIGTTIN, SIG_IGN);
    if (!mrsh_set_job_control(mrsh_state, true)) {
        return 1;
    }
    */

    struct mrsh_buffer parser_buffer = {0};
    struct mrsh_parser *parser = mrsh_parser_with_buffer(&parser_buffer);
    struct program_ctx program = {};

    /* init BearLibTerminal */
    terminal_open();
    terminal_set(
        "window: size=80x40, cellsize=auto, resizeable=true, title='TERSH';"
        "font: /Users/caseyduncan/Library/Fonts/DejaVu Sans Mono for Powerline.ttf, size=13;"
        "input: filter={keyboard}"
    );

    int cellh = terminal_state(TK_CELL_HEIGHT);
    terminal_setf(
        "0xE000: %s/../Resources/spinner.png, size=64x64, resize=%dx%d, resize-filter=bicubic;",
        dirpath, cellh, cellh
    );

    int term_order = 0;

    root_w = widget_new((widget_t){
        .anchor = ANCHOR_BOTTOM,
        .min_width = terminal_state(TK_WIDTH),
        .max_width = terminal_state(TK_WIDTH),
        .min_height = terminal_state(TK_HEIGHT),
        .max_height = terminal_state(TK_HEIGHT),
    });

    lineedit_t le = (lineedit_t){
        .blink_time = 700,
    };

    line_ed_w = widget_new((widget_t){
        .cls = &lineedit_widget,
        .parent = root_w,
        .anchor = ANCHOR_BOTTOM,
        .order = 0,
        .min_height = 1,
        .max_height = -1,
        .min_width = 10,
        .max_width = -1,
        .data = &le,
    });

    widget_t *term_container = widget_new((widget_t){
        .cls = &container_widget,
        .parent = root_w,
        .anchor = ANCHOR_BOTTOM,
        .order = 1,
        .min_height = -1,
        .max_height = -1,
        .min_width = 10,
        .max_width = -1,
    });
    container_set_bkcolor(term_container, 0xff000000);

    widget_layout(root_w, 0, 0, terminal_state(TK_WIDTH), terminal_state(TK_HEIGHT));
    widget_draw(root_w);
    terminal_refresh();

    while (running && lineedit_state(line_ed_w) != lineedit_cancelled) {
        poll_events(true);
        update_program(&program);
        poll_jobs();
        if (!program.started && lineedit_state(line_ed_w) == lineedit_confirmed) {
            if (le.buf.length) {
                Term *term = calloc(1, sizeof(Term));
                tnew(term, term_container->width, terminal_state(TK_HEIGHT));
                term->mode = MODE_UTF8 | MODE_WRAP | MODE_CRLF;
                char cmd[le.buf.length + 1];
                for (int i = 0; i < le.buf.length; i++) {
                    cmd[i] = le.buf.data[i];
                }
                cmd[le.buf.length] = 0;
                mrsh_buffer_append(&parser_buffer, cmd, le.buf.length);
                mrsh_parser_reset(parser);
                job_widget_new(term_container, --term_order, term, le.buf.data, le.buf.length);
                struct mrsh_program *prog = mrsh_parse_line(parser);
                if (prog != NULL) {
                    program = (struct program_ctx){
                        .term = term,
                        .mrsh_prog = prog,
                    };
                    int err = init_program_pty(&program);
                    if (!err) {
                        err = pthread_create(&program_thread, NULL, run_program_thread, &program);
                    }
                    if (err == 0) {
                        program.started = true;
                    } else {
                        if (err > 0) {
                            fprintf(stderr, "failed to create program thread: %s\n",
                                    strerror(err));
                        }
                        st_set_child_status(term, 1 << 8);
                    }
                } else {
                    struct mrsh_position err_pos;
                    const char *parser_err_msg = mrsh_parser_error(parser, &err_pos);
                    if (parser_err_msg != NULL) {
                        char err_msg[256];
                        int err_len = snprintf(err_msg, sizeof(err_msg),
                                "error in command:%d:%d\n\e[0;31m",
                                err_pos.line, err_pos.column);
                        st_print(term, err_msg, err_len);
                        st_print(term, parser_err_msg, -1);
                    } else {
                        st_print(term, "\e[0;31munknown error in command", -1);
                    }
                    st_set_child_status(term, 1 << 8);
                }
            }
            lineedit_clear(line_ed_w);
        }
        refresh();
    }

    mrsh_state_destroy(mrsh_state);
    terminal_close();
    return 0;
}
