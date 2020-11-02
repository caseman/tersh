/*
 * Simple Terminal widget
 */

#include "BearLibTerminal.h"
#include "st_widget.h"

void xbell(void) {}
void xclipcopy(void) {}

void draw_cursor(Term *term, int cx, int cy, Glyph g, int ox, int oy, Glyph og) {
    int u, offx, offy;

    /* remove old cursor */
    if (term->cursorshape > 2) {
        terminal_layer(1);
        terminal_put(ox, oy, 0);
    } else {
        terminal_put(ox, oy, og.u);
    }

    if (IS_SET(MODE_HIDE) || IS_SET(MODE_BLINK) || !IS_SET(MODE_FOCUSED)) {
        terminal_layer(0);
        return;
    }

    // TODO handle selection and colors

    if (term->cursorshape <= 2) {
        /* block cursor */
        u = g.u;
        if (u < 0x20) {
            u = 0x20;
        }
        terminal_color(0xff000000);
        terminal_bkcolor(0xffffffff);
        terminal_put(cx, cy, u);
        return;
    }

    switch (term->cursorshape) {
        case 3:
        case 4:
            /* underline cursor */
            u = '_';
            offx = offy = 0;
            break;
        case 5:
        case 6:
            /* bar cursor */
            u = '|';
            offx = 1 - terminal_state(TK_CELL_WIDTH) / 2;
            offy = -2;
            break;
        default:
            u = customcursor;
            offx = offy = 0;
    }

    terminal_color(0xffffffff);
    terminal_put_ext(cx, cy, offx, offy, u, NULL);
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
void xsetpointermotion(int set) {}
void xsetsel(char *str) {}
int xstartdraw(void) {
    return 1;
}
void xximspot(int x, int y) {}

void st_update(widget_t *w, unsigned int dt) {
    Term *term = widget_data(w, &st_widget);
    if (!IS_SET(MODE_FOCUSED) || !blinktimeout) return;
    term->blinkelapsed += dt;
    if (term->blinkelapsed >= blinktimeout) {
        term->blinkelapsed -= blinktimeout;
        term->mode ^= MODE_BLINK;
        w->flags |= WIDGET_NEEDS_REDRAW;
    }
}

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
    .update = st_update,
    // .init = st_init,
    // .del = st_del,
};
