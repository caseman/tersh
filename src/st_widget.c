/*
 * Simple Terminal widget
 */

#include "BearLibTerminal.h"
#include "st_widget.h"

void xbell(void) {}
void xclipcopy(void) {}

void draw_cursor(Term *term, int cx, int cy, Glyph g, int ox, int oy, Glyph og) {
    terminal_layer(1);
    /* remove old cursor */
    terminal_put(ox, oy, 0);

    if (IS_SET(MODE_HIDE) || !IS_SET(MODE_FOCUSED)) {
        terminal_layer(0);
        return;
    }

    // TODO handle selection and colors
    terminal_put(cx, cy, '|');
    terminal_layer(0);
}

void draw_line(Line line, int x1, int y1, int x2) {
    Glyph last = (Glyph){};
    Glyph *this = line;

    for (int x = x1; x < x2; x++, this++) {
        if (this->mode == ATTR_WDUMMY) continue;
        // TODO handle selection
        /*
        if (this.bg != last.bg) {
            terminal_bkcolor(this.bg);
        }
        if (this.fg != last.fg) {
            terminal_color(this.fg);
        }
        */
        // TODO support styles
        terminal_put(x, y1, this->u);
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
void xsetpointermotion(int set) {}
void xsetsel(char *str) {}
int xstartdraw(void) {
    return 1;
}
void xximspot(int x, int y) {}

void st_layout(widget_t *w) {
    Term *term = widget_data(w, &st_widget);
    if (w->width < 1) {
        w->width = 1;
    }
    if (term->col != w->width) {
        printf("resize!\n");
        tresize(term, w->width, term->row);
    }
    w->min_height = 1;
    w->max_height = term->nlines ? term->nlines : 1;
}

void
st_draw(widget_t *w)
{
    Term *term = widget_data(w, &st_widget);
    int cx = term->c.x, ocx = term->ocx, ocy = term->ocy;

    if (!xstartdraw())
        return;

    /* adjust cursor position */
    LIMIT(term->ocx, 0, term->col-1);
    LIMIT(term->ocy, 0, term->row-1);
    if (term->line[term->ocy][term->ocx].mode & ATTR_WDUMMY)
        term->ocx--;
    if (term->line[term->c.y][cx].mode & ATTR_WDUMMY)
        cx--;

    int moved = term->lastx != w->left || term->lasty != w->top;
    int y = w->top + w->height - 1;
    for (int row = MIN(term->nlines-1, term->row-1); row >= 0 && y >= w->top; row--, y--) {
        if (!moved && !term->dirty[row]) continue;
        term->dirty[row] = 0;
        draw_line(term->line[row], w->left, y, w->left + term->col);
    }

    draw_cursor(term, w->left + cx, w->top + term->c.y, term->line[term->c.y][cx],
        term->lastx + term->ocx, term->lasty + term->ocy, term->line[term->ocy][term->ocx]);
    term->ocx = cx;
    term->ocy = term->c.y;
    term->lastx = w->left;
    term->lasty = w->top;
    xfinishdraw();
    if (ocx != term->ocx || ocy != term->ocy)
        xximspot(term->ocx, term->ocy);
}

widget_cls st_widget = {
    .name = "st",
    // .handle_ev = st_handle_ev,
    .draw = st_draw,
    .layout = st_layout,
    // .init = st_init,
    // .del = st_del,
};
