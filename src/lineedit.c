#include "BearLibTerminal.h"
#include "lineedit.h"

int lineedit_insert(widget_t *w, wchar_t ch) {
    lineedit_t *le = widget_data(w, &lineedit_widget);
    int err;
    if (le->curs >= le->buf.length) {
        err = vec_push(&le->buf, ch);
        le->curs = le->buf.length;
    } else {
        err = vec_insert(&le->buf, le->curs, ch);
        if (!err) le->curs++;
    }
    return err;
}

void lineedit_clear(widget_t *w) {
    lineedit_t *le = widget_data(w, &lineedit_widget);
    le->buf.length = 0;
    le->curs = 0;
    le->curs_vis = 1;
    le->state = lineedit_unchanged;
}

void lineedit_was_changed(widget_t *w) {
    lineedit_t *le = widget_data(w, &lineedit_widget);
    le->state = lineedit_changed;
    le->curs_vis = 1;
    w->flags |= WIDGET_NEEDS_REDRAW;
}

int lineedit_handle_ev(widget_t *w, int event) {
    lineedit_t *le = widget_data(w, &lineedit_widget);
    le->state = lineedit_unchanged;
    if (terminal_state(TK_CONTROL)) {
        switch (event) {
            case TK_D:
                le->state = lineedit_cancelled;
                return 1;
            case TK_E:
                le->curs = le->buf.length;
                lineedit_was_changed(w);
                return 1;
            case TK_A:
                le->curs = 0;
                lineedit_was_changed(w);
                return 1;
            case TK_C:
                lineedit_clear(w);
                lineedit_was_changed(w);
                return 1;
            case TK_K:
                le->buf.length = le->curs;
                lineedit_was_changed(w);
                return 1;
            case TK_H:
                event = TK_BACKSPACE;
                break;
        }
    }
    switch (event) {
        case TK_RETURN:
            le->state = lineedit_confirmed;
            return 1;
        case TK_ESCAPE:
            le->state = lineedit_cancelled;
            return 1;
        case TK_BACKSPACE:
            if (le->buf.length == 0) return 1;
            if (le->curs >= le->buf.length) {
                vec_pop(&le->buf);
                le->curs = le->buf.length;
            } else if (le->curs > 0) {
                vec_del(&le->buf, le->curs - 1);
                le->curs--;
            }
            lineedit_was_changed(w);
            return 1;
        case TK_DELETE:
            if (le->curs < le->buf.length) {
                vec_del(&le->buf, le->curs);
            }
            lineedit_was_changed(w);
            return 1;
        case TK_LEFT:
            if (le->curs > 0) {
                le->curs--;
            }
            lineedit_was_changed(w);
            return 1;
        case TK_RIGHT:
            if (le->curs < le->buf.length) {
                le->curs++;
            }
            lineedit_was_changed(w);
            return 1;
        case TK_HOME:
            le->curs = 0;
            lineedit_was_changed(w);
            return 1;
        case TK_END:
            le->curs = le->buf.length;
            lineedit_was_changed(w);
            return 1;
        default:
            if (terminal_check(TK_WCHAR)) {
                lineedit_insert(w, terminal_state(TK_WCHAR));
                lineedit_was_changed(w);
                return 1;
            }
    }
    return 0;
}

void lineedit_update(widget_t *w, unsigned int dt) {
    lineedit_t *le = widget_data(w, &lineedit_widget);
    le->elapsed += dt;
    if (le->blink_time && le->elapsed >= le->blink_time) {
        le->elapsed -= le->blink_time;
        le->curs_vis = !le->curs_vis;
        w->flags |= WIDGET_NEEDS_REDRAW;
    }
}

void lineedit_layout(widget_t *w) {
    lineedit_t *le = widget_data(w, &lineedit_widget);
    w->height = 1 + (le->buf.length + 2) / (w->width + 1);
}

void lineedit_draw(widget_t *w) {
    lineedit_t *le = widget_data(w, &lineedit_widget);
    terminal_color(0xffffffff);
    terminal_bkcolor(0xff000000);
    terminal_clear_area(w->left, w->top, w->width, w->height);
    int y = w->top;
    int x = w->left + 2;
    terminal_print(0, y, "[color=yellow]>[/color]");
    int i;
    wchar_t ch;
    vec_foreach(&le->buf, ch, i) {
        terminal_put(x++, y, ch);
        if (x > w->left + w->width) {
            x = 0;
            y++;
        }
    }
    // Draw cursor
    terminal_layer(1);
    terminal_clear_area(w->left, w->top, w->width, w->height);
    if (le->curs_vis) {
        int cellw = terminal_state(TK_CELL_WIDTH);
        x = w->left + (le->curs + 2) % (w->width + 1);
        y = w->top + (le->curs + 2) / (w->width + 1);
        terminal_put_ext(x, y, -cellw / 2 + 1, -2, 0x007C, NULL);
    }
    terminal_layer(0);
}

lineedit_state_e lineedit_state(widget_t *w) {
    lineedit_t *le = widget_data(w, &lineedit_widget);
    lineedit_state_e state = le->state;
    return state;
}

widget_cls lineedit_widget = {
    .name = "lineedit",
    .handle_ev = lineedit_handle_ev,
    .draw = lineedit_draw,
    .layout = lineedit_layout,
    .update = lineedit_update,
};
