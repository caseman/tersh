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

#define FRAME_TIME 30

static void print_w(widget_t *w) {
    printf("widget %d (%d, %d, %d, %d) minw=%d minh=%d\n",
            w->order, w->left, w->top, w->width, w->height, w->min_width, w->min_height);
}

/*
color_t colors[8] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFF3300, 0xFF00FFFF,
                     0xFFBBBBBB, 0xFF444444, 0xFF33FF00};
static void fill_w(widget_t *w) {
    char str[50];
    int l, t, r, b;
    print_w(w);
    l = w->left > 0 ? w->left : 0;
    t = w->top > 0 ? w->top : 0;
    r = w->left + w->width;
    b = w->top + w->height;
    if (b < t || r < l) return;
    terminal_bkcolor(colors[abs(w->order) % 8]);
    printf("clear %d (%d, %d, %d, %d)\n", w->order, l, t, w->width, w->height);
    terminal_clear_area(l, t, r - l, b - t);
    terminal_color(0xFF000000);
    sprintf(str, "%d", w->order);
    terminal_print(w->left + 1, w->top + w->height - 1, str);
    int i;
    widget_t *child;
    vec_foreach(&w->children, child, i) {
        fill_w(child);
    }
}

void widget_test() {
    widget_t *main_w = widget_new((widget_t){
        .anchor = ANCHOR_BOTTOM,
        .min_width = terminal_state(TK_WIDTH),
        .max_width = terminal_state(TK_WIDTH),
        .min_height = terminal_state(TK_HEIGHT),
        .max_height = terminal_state(TK_HEIGHT),
    });
    widget_t *cmd_line_w = widget_addx(main_w, 1, ANCHOR_BOTTOM, -1, 2);
    widget_addx(cmd_line_w, 100, ANCHOR_LEFT, 3, 3);
    widget_addx(cmd_line_w, 102, ANCHOR_RIGHT, 10, 1);
    widget_t *scroll_w = widget_addx(main_w, 2, ANCHOR_RIGHT, 1, -1);
    widget_addx(scroll_w, 201, ANCHOR_TOP, 1, 1);
    widget_addx(scroll_w, 204, ANCHOR_BOTTOM, 1, 1);
    widget_addx(main_w, 3, ANCHOR_BOTTOM, -1, 1);
    widget_t *term_w = widget_addx(main_w, 4, ANCHOR_BOTTOM, -1, -1);
    widget_layout(main_w, 0, 0, terminal_state(TK_WIDTH), terminal_state(TK_HEIGHT));
    widget_t *fake_stuff = NULL;
    int scroll = 0;

    while (1) {
        terminal_bkcolor(0xFF000000);
        terminal_clear();
        fill_w(main_w);
        terminal_refresh();
        while (!terminal_has_input()) {
            terminal_delay(20);
            if (scroll++ % 23 == 0) {
                fake_stuff = widget_addx(term_w, -scroll, ANCHOR_BOTTOM, -1, -2);
            } else if (fake_stuff) {
                fake_stuff->min_height++;
            }
            widget_refresh(term_w);
            fill_w(term_w);
            print_w(term_w);
            terminal_refresh();
        }
        int key = terminal_read();
        if (key == TK_ESCAPE || key == TK_CLOSE) {
            terminal_close();
            exit(0);
        } else if (key == TK_RESIZED) {
            main_w->min_width = main_w->max_width = terminal_state(TK_WIDTH);
            main_w->min_height = main_w->max_height = terminal_state(TK_HEIGHT);
            widget_layout(main_w, 0, 0, terminal_state(TK_WIDTH), terminal_state(TK_HEIGHT));
        }
    }
}
*/

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

int main(int argc, char* argv[]) {
    char *path = strdup(argv[0]);
    char *dirpath = dirname(path);

    terminal_open();
    terminal_setf(
        "window: size=80x40, cellsize=auto, resizeable=true, title='TERSH';"
        "font: /Users/caseyduncan/Library/Fonts/DejaVu Sans Mono for Powerline.ttf, size=13;"
        "input: filter={keyboard}",
        dirpath
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
        .parent = root_w,
        .anchor = ANCHOR_BOTTOM,
        .order = 1,
        .min_height = -1,
        .max_height = -1,
        .min_width = 10,
        .max_width = -1,
    });

    widget_layout(root_w, 0, 0, terminal_state(TK_WIDTH), terminal_state(TK_HEIGHT));
    widget_draw(root_w);
    terminal_refresh();


    long long last_time = time_millis();
    long long now;
    int dt = 0;
    Term *last_term = NULL;
    Term *fg_term;

    while (lineedit_state(line_ed_w) != lineedit_cancelled) {
        if (!terminal_has_input()) {
            // Set an alarm to ensure any blocking i/o eventually
            // gives time back here to the event loop. We use a
            // multiple of the typical "frame" time to gracefully
            // accomodate slow connections
            signal(SIGALRM, SIG_IGN);
            ualarm(FRAME_TIME * 5 * 1000, 0);

            do {
                if (poller_poll(FRAME_TIME - dt) > 0 && !terminal_has_input()) {
                    terminal_delay(FRAME_TIME - dt);
                }
                now = time_millis();
                dt = now - last_time;
            } while (!terminal_has_input() && dt < FRAME_TIME);

            ualarm(0, 0);
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
                    case TK_BACKSPACE:
                        ch = '\b';
                        break;
                    case TK_ESCAPE:
                        ch = '\e';
                        break;
                    default:
                        ch = terminal_state(TK_CHAR);
                }
                ttywrite(fg_term, &ch, 1, 1);
            } else {
                line_ed_w->cls->handle_ev(line_ed_w, key);
            }
        }

        if (lineedit_state(line_ed_w) == lineedit_confirmed) {
            vec_str_t child_argv = NULL_VEC;

            char *cmd = parse_cmd(&le.buf, &child_argv);
            lineedit_clear(line_ed_w);
            if (cmd == NULL) break;
            if (*cmd) {
                Term *term = calloc(1, sizeof(Term));
                tnew(term, term_container->width, terminal_state(TK_HEIGHT));
                ttynew(term, NULL, cmd, "/tmp/term.out", child_argv.data);
                widget_new((widget_t){
                    .cls = &st_widget,
                    .data = term,
                    .parent = term_container,
                    .anchor = ANCHOR_BOTTOM,
                    .order = --term_order,
                    .min_height = 1,
                    .max_height = 1,
                    .min_width = 10,
                    .max_width = -1,
                });
            }
            free(cmd);
        }

        widget_relayout(root_w);
        widget_draw(root_w);
        terminal_refresh();
    }

    terminal_close();
    return 0;
}
