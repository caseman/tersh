#include "widget_vterm.h"

int w_vterm_handle_ev(widget_t *w, int event) {
    return 0;
}

void w_vterm_draw(widget_t *w) {
    vterm_t *vt = w->data;
    int full_redraw = w->flags & WIDGET_NEEDS_REDRAW 
                      || vt->flags & VT_NEEDS_REDRAW;
    int line_pos = vt->scroll_pos == 0 ? vt->lines - vt->height : vt->scroll_pos - 1;
    int empty_lines_top = (line_pos < 0) * -line_pos;
    if (full_redraw) {
        terminal_clear_area(w->left, w->top, w->width, w->height);
    } else if (empty_lines_top) {
        terminal_clear_area(w->left, w->top, w->width, empty_lines_top);
    }
    int first_line = (line_pos > 0) * line_pos;
    int empty_lines_bottom = vt->height - empty_lines_top - vt->lines - first_line;
    if (!full_redraw && empty_lines_bottom > 0) {
        terminal_clear_area(vt->left, vt->top + vt->height - empty_lines_bottom,
                vt->width, empty_lines_bottom);
    }
    // defensive: insure we don't draw beyond the end of the line buffer
    int bottom_y = vt->top + vt->height - (empty_lines_bottom > 0) * empty_lines_bottom;
    vterm_cell_t *cell = vt->line_buf.data + first_line * vt->width;
    if (full_redraw) {
        // Short circuit checking individual dirty flags and draw everything
        for (int screen_y = vt->top + empty_lines_top; screen_y < bottom_y; screen_y++) {
            for (int screen_x = 0; screen_x < vt->width; screen_x++, cell++) {
                if (cell->ch) terminal_put(screen_x, screen_y, cell->ch);
                cell->flags &= ~VTCELL_DIRTY_FLAG;
            }
        }
    } else {
        for (int screen_y = vt->top + empty_lines_top; screen_y < bottom_y; screen_y++) {
            if (!(cell->flags & VTCELL_DIRTY_FLAG)) {
                // Line is not dirty, skip it
                // printf("skipped line @%d ch=%c flags=%x first=%d\n", screen_y, cell->ch, cell->flags, first_line);
                cell += vt->width;
                continue;
            }
            for (int screen_x = 0; screen_x < vt->width; screen_x++, cell++) {
                if (cell->flags & VTCELL_DIRTY_FLAG) {
                    terminal_put(screen_x, screen_y, cell->ch);
                    cell->flags &= ~VTCELL_DIRTY_FLAG;
                }
            }
        }
    }
    vt->flags &= ~VT_NEEDS_REDRAW;
}
