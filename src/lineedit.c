#include "BearLibTerminal.h"
#include "lineedit.h"

#define ALLOC_INCR 256

int lineedit_init(lineedit *le) {
    le->alloc = ALLOC_INCR;
    le->len = 0;
    le->buf = malloc(sizeof(wchar_t) * le->alloc);
    if (le->buf == NULL) {
        return -1;
    }
    le->curs = 0;
    le->curs_vis = 0;
    *(le->buf) = 0;
    return 0;
}

int lineedit_insert(lineedit *le, wchar_t ch) {
    if (le->len + 1 == le->alloc) {
        wchar_t *rebuf = realloc(le->buf, sizeof(wchar_t) * (le->alloc + ALLOC_INCR));
        if (rebuf == NULL) {
            return -1;
        }
        le->buf = rebuf;
        le->alloc += ALLOC_INCR;
    }
    le->len++;
    for (wchar_t *c = le->buf+le->len; c > le->buf+le->curs; c--) {
        *c = *(c - 1);
    }
    *(le->buf+le->curs) = ch;
    *(le->buf+le->len+1) = 0;
    le->curs++;
    return 0;
}

int lineedit_handle(lineedit *le, int event) {
    switch (event) {
        case TK_RETURN:
            return LINEEDIT_CONFIRM;
        case TK_ESCAPE:
            return LINEEDIT_CANCEL;
        case TK_BACKSPACE:
            if (le->curs > 0) {
                le->curs--;
                for (wchar_t *c = le->buf+le->curs; c < le->buf+le->len+1; c++) {
                    *c = *(c + 1);
                }
                le->len--;
            }
            return LINEEDIT_CHANGED;
        case TK_LEFT:
            if (le->curs > 0) {
                le->curs--;
            }
            return LINEEDIT_CHANGED;
        case TK_RIGHT:
            if (le->curs < le->len) {
                le->curs++;
            }
            return LINEEDIT_CHANGED;
    }
    if (terminal_check(TK_WCHAR)) {
        lineedit_insert(le, terminal_state(TK_WCHAR));
        return LINEEDIT_CHANGED;
    }
    return LINEEDIT_UNCHANGED;
}

static void draw_curs(lineedit *le, int term_w, int term_h, int le_h) {
    terminal_layer(1);
    terminal_clear_area(0, term_h - le_h, term_w, le_h);
    if (le->curs_vis) {
        int cellw = terminal_state(TK_CELL_WIDTH);
        int x = (le->curs+2) % term_w;
        int y = term_h - le_h + (le->curs+2) / term_w;
        terminal_put_ext(x, y, -cellw / 2 + 1, -2, 0x007C, NULL);
    }
    terminal_layer(0);
}

void lineedit_blink(lineedit *le) {
    int term_w = terminal_state(TK_WIDTH);
    int term_h = terminal_state(TK_HEIGHT);
    int le_h = (le->len+2) / term_w + 1;
    le->curs_vis = !le->curs_vis;
    draw_curs(le, term_w, term_h, le_h);
}

int lineedit_draw(lineedit *le) {
    int term_w = terminal_state(TK_WIDTH);
    int term_h = terminal_state(TK_HEIGHT);
    int le_h = (le->len+2) / term_w + 1;
    int y = term_h - le_h;
    terminal_clear_area(0, y, term_w, le_h);
    terminal_print(0, y, "[color=yellow]>[/color]");
    wchar_t *c = le->buf;
    int x = 2;
    while (*(c)) {
        for (; *(c) && x < term_w; x++) {
            terminal_put(x, y, *(c++));
        }
        x = 0;
        y++;
    }
    draw_curs(le, term_w, term_h, le_h);
    return le_h;
}
