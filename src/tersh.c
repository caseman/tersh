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

extern char **environ;

char *parse_cmd(vec_wchar_t *input, vec_str_t *argv) {
    argv->length = 0;
    char *cmd = calloc(input->length + 1, 1);
    if (cmd == NULL) return NULL;
    char *c = cmd;
    int i = 0;
    while (i < input->length) {
        while (i < input->length && input->data[i] == ' ') i++;
        if (i == input->length) break;
        vec_push(argv, c);
        for(; i < input->length; i++) {
            if (input->data[i] == ' ') {
                *(c++) = 0;
                break;
            }
            *(c++) = input->data[i];
        }
    }
    vec_push(argv, 0);
    return cmd;
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

int alarm_time = 0;

void handle_sigalrm() {
    alarm_time = time_millis();
}

int run_program(Term *term, struct mrsh_state *state, struct mrsh_program *prog) {
    pid_t pid;
    int ret;
    mrsh_program_print(prog);
    switch (pid = st_fork_pty(term)) {
    case -1:
        return -1;
    case 0:
        // child
        ret = mrsh_run_program(state, prog);
        mrsh_destroy_terminated_jobs(state);
        exit(ret >= 0 ? ret : 127);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    char *path = strdup(argv[0]);
    char *dirpath = dirname(path);

    terminal_open();
    terminal_set(
        "window: size=80x40, cellsize=auto, resizeable=true, title='TERSH';"
        "font: /Users/caseyduncan/Library/Fonts/DejaVu Sans Mono for Powerline.ttf, size=13;"
        "input: filter={keyboard}"
    );
    atexit(terminal_close); // ensure this is called for child processes

    int cellh = terminal_state(TK_CELL_HEIGHT);
    terminal_setf(
        "0xE000: %s/../Resources/spinner.png, size=64x64, resize=%dx%d, resize-filter=bicubic;",
        dirpath, cellh, cellh
    );

    int term_order = 0;

    widget_t *root_w = widget_new((widget_t){
        .anchor = ANCHOR_BOTTOM,
        .min_width = terminal_state(TK_WIDTH),
        .max_width = terminal_state(TK_WIDTH),
        .min_height = terminal_state(TK_HEIGHT),
        .max_height = terminal_state(TK_HEIGHT),
    });

    lineedit_t le = (lineedit_t){
        .blink_time = 700,
    };

    widget_t *line_ed_w = widget_new((widget_t){
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

    struct mrsh_state *state = mrsh_state_create();
    if (!mrsh_populate_env(state, environ)) {
        return 1;
    }

    struct mrsh_buffer parser_buffer = {0};
    struct mrsh_parser *parser = mrsh_parser_with_buffer(&parser_buffer);

    long long last_time = time_millis();
    long long now;
    int dt = 0;
    Term *last_term = NULL;

    while (lineedit_state(line_ed_w) != lineedit_cancelled) {
        // Set an alarm to ensure any blocking i/o eventually
        // gives time back here to the event loop. We use a
        // multiple of the typical "frame" time to gracefully
        // accomodate slow connections
        signal(SIGALRM, handle_sigalrm);
        ualarm(0, 0);
        ualarm(FRAME_TIME * 5 * 1000, 0);

        if (!terminal_has_input()) {
            do {
                if (poller_poll(FRAME_TIME - dt) > 0 && !terminal_has_input()) {
                    ualarm(0, 0);
                    terminal_delay(FRAME_TIME - dt);
                    ualarm(FRAME_TIME * 5 * 1000, 0);
                }
                now = time_millis();
                dt = now - last_time;
            } while (!terminal_has_input() && dt < FRAME_TIME);

            widget_update(root_w, dt);
            last_time = now;
            dt = 0;
        }

        Term *fg_term = poller_getfg();
        if (fg_term != last_term) {
            if (last_term) st_set_focused(last_term, 0);
            if (fg_term) st_set_focused(fg_term, 1);
            last_term = fg_term;
        }

        if (terminal_has_input()) {
            int key = terminal_read();
            if (key == TK_CLOSE) {
                break;
            }
            if (key == TK_RESIZED) {
                root_w->min_width = terminal_state(TK_WIDTH),
                root_w->max_width = terminal_state(TK_WIDTH),
                root_w->min_height = terminal_state(TK_HEIGHT),
                root_w->max_height = terminal_state(TK_HEIGHT),
                widget_layout(root_w, 0, 0, terminal_state(TK_WIDTH), terminal_state(TK_HEIGHT));
                widget_draw(root_w);
                terminal_refresh();
                continue;
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

        if (lineedit_state(line_ed_w) == lineedit_confirmed) {
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
                    int r = run_program(term, state, prog);
                    if (r == -1) {
                        term->childexited = 1;
                        term->childexitst = 1;
                    }
                } else {
                    char err_msg[256];
                    int err_len;
                    struct mrsh_position err_pos;
                    const char *parser_err_msg = mrsh_parser_error(parser, &err_pos);
                    if (parser_err_msg != NULL) {
                        err_len = snprintf(err_msg, sizeof(err_msg),
                                "%s:%d:%d -~- %s\n",
                                cmd, err_pos.line, err_pos.column, parser_err_msg);
                    } else {
                        err_len = snprintf(err_msg, sizeof(err_msg),
                                "%s -~- unknown error\n", cmd);
                    }
                    st_print(term, err_msg, err_len);
                }
            }
            lineedit_clear(line_ed_w);
        }

        widget_relayout(root_w);
        terminal_clear();
        widget_draw(root_w);
        terminal_refresh();
    }

    return 0;
}
