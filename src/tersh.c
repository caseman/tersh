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

void parse_cmd(vec_wchar_t *input, vec_char_t *cmd_line, vec_str_t *argv) {
    cmd_line->length = 0;
    argv->length = 0;
    if (!input->length) return;
    vec_reserve(cmd_line, input->length);
    vec_push(argv, cmd_line->data);
    int i = 0;
    while (input->data[i] == ' ' && i < input->length) i++;
    for(; i < input->length; i++) {
        if (input->data[i] == ' ') {
            vec_push(cmd_line, 0);
            break;
        }
        vec_push(cmd_line, input->data[i]);
    }
    while (i < input->length) {
        while (input->data[i] == ' ' && i < input->length) i++;
        if (i == input->length) break;
        vec_push(argv, cmd_line->data + cmd_line->length);
        for(; i < input->length; i++) {
            if (input->data[i] == ' ') {
                vec_push(cmd_line, 0);
                break;
            }
            vec_push(cmd_line, input->data[i]);
        }
    }
    vec_push(cmd_line, 0);
    vec_push(argv, 0);
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

    process_mgr_t process_mgr = (process_mgr_t){
        .data_cb = vterm_process_data_cb,
        .event_cb = vterm_process_event_cb,
    };

    widget_t *root_w = widget_new((widget_t){
        .anchor = ANCHOR_BOTTOM,
        .min_width = terminal_state(TK_WIDTH),
        .max_width = terminal_state(TK_WIDTH),
        .min_height = terminal_state(TK_HEIGHT),
        .max_height = terminal_state(TK_HEIGHT),
    });

    lineedit_t le = (lineedit_t){
        .blink_time = 500,
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

    widget_t *vterm_container = widget_new((widget_t){
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

    vec_char_t cmd_line;
    vec_init(&cmd_line);
    vec_str_t child_argv;
    vec_init(&child_argv);
    process_t *child;

    while (1) {
        if (!terminal_has_input()) {
            process_poll(&process_mgr, 5);
            widget_update(root_w, 5);
        }

        if (terminal_has_input()) {
            int key = terminal_read();
            if (key == TK_ESCAPE || key == TK_CLOSE) {
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
            line_ed_w->cls->handle_ev(line_ed_w, key);
        }

        if (lineedit_state(line_ed_w) == lineedit_confirmed) {
            parse_cmd(&le.buf, &cmd_line, &child_argv);
            lineedit_clear(line_ed_w);
            if (cmd_line.length) {
                widget_t *vterm_w = widget_new((widget_t){
                    .cls = &vterm_widget,
                    .parent = vterm_container,
                    .anchor = ANCHOR_BOTTOM,
                    .order = -process_mgr.processes.length,
                    .min_height = 1,
                    .max_height = 1,
                    .min_width = 10,
                    .max_width = -1,
                });
                child = process_spawn(&process_mgr, vterm_w, cmd_line.data, child_argv.data);
                if (child == NULL) {
                    char *err_str = strerror(errno);
                    vterm_write(vterm_w, "ERROR: ", 7);
                    vterm_write(vterm_w, err_str, strlen(err_str));
                    vterm_write(vterm_w, "\n", 1);
                    continue;
                }
            }
        }

        widget_relayout(root_w);
        widget_draw(root_w);
        terminal_refresh();
    }

    terminal_close();
    return 0;
}
