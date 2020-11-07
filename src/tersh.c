#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/time.h>
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

int main(int argc, char* argv[]) {
    char *path = strdup(argv[0]);
    char *dirpath = dirname(path);

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


    long long last_time = time_millis();
    long long now;
    int dt = 0;
    Term *last_term = NULL;

    char *cmd_argv[4] = {"zsh", "-c", NULL, NULL};

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
                char cmd[le.buf.length + 1];
                for (int i = 0; i < le.buf.length; i++) {
                    cmd[i] = le.buf.data[i];
                }
                cmd[le.buf.length] = 0;
                cmd_argv[2] = cmd;
                Term *term = calloc(1, sizeof(Term));
                tnew(term, term_container->width, terminal_state(TK_HEIGHT));
                ttynew(term, NULL, "tersh", "/tmp/term.out", cmd_argv);
                job_widget_new(term_container, --term_order, term, le.buf.data, le.buf.length);
            }
            lineedit_clear(line_ed_w);
        }

        widget_relayout(root_w);
        terminal_clear();
        widget_draw(root_w);
        terminal_refresh();
    }

    terminal_close();
    return 0;
}
