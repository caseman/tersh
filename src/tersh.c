#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include "BearLibTerminal.h"
//#include "tersh.h"
#include "lineedit.h"
#include "vterm.h"
#include "vec.h"
#include "process.h"
#include "widget.h"

#define curs_blink 500

static void print_w(widget_t *w) {
    printf("widget %d (%d, %d, %d, %d) minw=%d minh=%d\n",
            w->order, w->left, w->top, w->width, w->height, w->min_width, w->min_height);
}


color_t colors[8] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFF7700, 0xFF00FFFF,
                     0xFFBBBBBB, 0xFF444444, 0xFF77FF00};
static void fill_w(widget_t *w) {
    char str[50];
    int l, t, r, b;
    if (w->flags & WIDGET_DELETED) return;
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

    widget_t *main_w = widget_new();
    main_w->min_width = main_w->max_width = terminal_state(TK_WIDTH);
    main_w->min_height = main_w->max_height = terminal_state(TK_HEIGHT);
    main_w->anchor = ANCHOR_BOTTOM;
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
            terminal_delay(10);
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
    vterm_t vt;
    vterm_init(&vt, 0, 0, terminal_state(TK_WIDTH), terminal_state(TK_HEIGHT)-1);

    lineedit led;
    lineedit_init(&led);
    int curs_time = 0;

    lineedit_draw(&led);
    terminal_refresh();

    vec_char_t cmd_line;
    vec_init(&cmd_line);
    vec_reserve(&cmd_line, 128);
    vec_str_t child_argv;
    vec_init(&child_argv);

    process_t child;

    while (1) {
        while (!terminal_has_input()) {
            terminal_delay(1);
            if (++curs_time > curs_blink) {
                lineedit_blink(&led);
                terminal_refresh();
                curs_time = 0;
            }
        }

        if (terminal_has_input()) {
            int key = terminal_read();
            if (key == TK_ESCAPE || key == TK_CLOSE) {
                break;
            }
            int led_state = lineedit_handle(&led, key);
            if (led_state == LINEEDIT_CHANGED) {
                led.curs_vis = 1;
                lineedit_draw(&led);
                terminal_refresh();
                curs_time = 0;
            } else if (led_state == LINEEDIT_CONFIRM) {
                cmd_line.length = 0;
                child_argv.length = 0;
                const char *cmd = cmd_line.data;
                vec_push(&child_argv, cmd_line.data);
                int i = 0;
                for(; i < led.len; i++) {
                    if (led.buf[i] == ' ') {
                        vec_push(&cmd_line, 0);
                        break;
                    }
                    vec_push(&cmd_line, led.buf[i]);
                }
                while (i < led.len) {
                    while (led.buf[i] == ' ' && i < led.len) i++;
                    if (i == led.len) break;
                    vec_push(&child_argv, cmd_line.data + cmd_line.length);
                    for(; i < led.len; i++) {
                        if (led.buf[i] == ' ') {
                            vec_push(&cmd_line, 0);
                            break;
                        }
                        vec_push(&cmd_line, led.buf[i]);
                    }
                }
                vec_push(&cmd_line, 0);
                vec_push(&child_argv, 0);

                int err = process_spawn(&child, cmd, child_argv.data);
                if (err < 0) {
                    char *err_str = strerror(errno);
                    vterm_write(&vt, "ERROR: ", 7);
                    vterm_write(&vt, err_str, strlen(err_str));
                    vterm_write(&vt, "\n", 1);
                    continue;
                }
                ssize_t bytes;
                unsigned char buf[256];
                while ((bytes = read(child.out, buf, 256))) {
                    if (bytes < 0) break;
                    vterm_write(&vt, buf, bytes);
                }
                int status;
                waitpid(child.pid, &status, 0);
                if (WIFEXITED(status)) {
                    printf("%s exited %d\n", cmd, WEXITSTATUS(status));
                }
                vterm_draw(&vt);
                lineedit_clear(&led);
                lineedit_draw(&led);
                terminal_refresh();
            }
        }
    }
    terminal_close();
    return 0;
}
