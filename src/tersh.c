#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <libgen.h>
#include "BearLibTerminal.h"
#include "tersh.h"
#include "lineedit.h"

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
    terminal_refresh();

    lineedit led;
    lineedit_init(&led);
    int curs_time = 0;

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
            }
        }
    }
    terminal_close();
    return 0;
}
