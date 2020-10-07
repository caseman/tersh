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

#define curs_blink 500

int main(int argc, char* argv[]) {
    char *path = strdup(argv[0]);
    char *dirpath = dirname(path);

    terminal_open();
    terminal_setf(
        "window: size=80x40, cellsize=auto, resizeable=true, fullscreen=true, title='TERSH';"
        "font: /Users/caseyduncan/Library/Fonts/DejaVu Sans Mono for Powerline.ttf, size=13;"
        "input: filter={keyboard}",
        dirpath
    );

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
