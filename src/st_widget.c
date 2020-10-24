/* 
 * Simple Terminal widget
 */

#include "BearLibTerminal.h"
#include "widget.h"
#include "st.h"
#include "win.h"

void xbell(void) {}
void xclipcopy(void) {}

void xdrawcursor(int cx, int cy, Glyph g, int ox, int oy, Glyph og) {
}

void xdrawline(Line line, int x1, int y1, int x2) {
    Glyph last = (Glyph){};
    Glyph this;

    for (int x = x1; x < x2; x++) {
        this = line[x];
        if (this.mode == ATTR_WDUMMY) continue;
        // TODO handle selection
        if (this.bg != last.bg) {
            terminal_bkcolor(this.bg);
        }
        if (this.fg != last.fg) {
            terminal_color(this.fg);
        }
        // TODO support styles
        terminal_put(x, y1, this.u);
    }
}

void xfinishdraw(void) {}
void xloadcols(void) {}
int xsetcolorname(int x, const char *name) {
    return 0;
}
void xseticontitle(char *p) {}
void xsettitle(char *p) {}
int xsetcursor(int cursor) {
    return 0;
}
void xsetmode(int set, unsigned int flags) {}
void xsetpointermotion(int set) {}
void xsetsel(char *str) {}
int xstartdraw(void) {
    return 1;
}
void xximspot(int x, int y) {}