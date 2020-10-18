#include "stdio.h"
#include "assert.h"
#include "err.h"
#include "vterm.h"

static int add_lines(vterm_t *vt, int count) {
    if (count <= 0) return 0;
    vt->lines += count;
    vt->flags |= VT_NEEDS_REDRAW;
    return vec_push_zeros(&vt->line_buf, count * vt->width);
}

void parser_handle(vtparse_t *parser, vtparse_action_t action, unsigned int ch) {
    vterm_t *vt = parser->user_data;
    switch (action) {
        case VTPARSE_ACTION_PRINT:
        {
            int x = vt->curs_x;
            int y = vt->curs_y;
            y += vt->scroll_pos > 0 ? vt->scroll_pos - 1 : 0;
            int w = vt->width;
            int line_count = 1 + (x + parser->print_buf_len) / w;
            int err = add_lines(vt, y + line_count - vt->lines);
            if (err != 0) return; // TODO error logging
            unsigned int *print_ch = parser->print_buf;
            vterm_cell_t *cell = vt->line_buf.data + y * w;
            // Mark starting line dirty
            cell->flags |= VTCELL_DIRTY_FLAG;
            cell += x;

            while (*print_ch) {
                if (++x == w) {
                    // EOL, mark cell as wrapped and start next line
                    cell->flags |= VTCELL_WRAPPED_FLAG;
                    x = 0;
                    y++;
                }
                cell->flags |= VTCELL_DIRTY_FLAG;
                cell->ch = *(print_ch++);
                cell++;
            }
            vt->curs_x = x;
            vt->curs_y = y;
            break;
        }
        case VTPARSE_ACTION_EXECUTE:
        {
            switch (ch) {
                case '\n':
                    vt->curs_x = 0;
                    vt->curs_y++;
                    add_lines(vt, vt->curs_y - vt->lines);
                    return;
            }
        }
        default:
        {
            fprintf(stderr, "vterm ignored %s (%x) ", ACTION_NAMES[action], ch);
            for (int i = 0; i < parser->num_params; i++) {
                if ((i+1) < parser->num_params) {
                    fprintf(stderr, "%d, ", parser->params[i]);
                } else {
                    fprintf(stderr, "%d", parser->params[i]);
                }
            }
            fprintf(stderr, "\n");
        }
    }
}

int vterm_init(vterm_t *vt, int left, int top, int width, int height) {
    memset(vt, 0, sizeof(vterm_t));
    vtparse_init(&vt->parser, parser_handle);
    vt->parser.user_data = vt;
    return_err(vec_reserve(&vt->line_buf, (width+1)*height));
    vt->left = left;
    vt->top = top;
    vt->width = width;
    vt->height = height;
    vt->flags = VT_NEEDS_REDRAW;
    return 0;
}

void vterm_deinit(vterm_t *vt) {
    vec_deinit(&vt->line_buf);
    vec_deinit(&vt->out_buf);
    vec_deinit(&vt->styles);
}

int vterm_write(vterm_t *vt, unsigned char *data, unsigned int data_len) {
    vtparse(&vt->parser, data, data_len);
    return 0;
}

unsigned char *vterm_read(vterm_t *vt) {
    if (vt->out_buf.length) {
        unsigned char *data = vt->out_buf.data;
        vec_init(&vt->out_buf);
        return data;
    }
    return NULL;
}

int vterm_handle_ev(widget_t *w, int event) {
    assert(w->cls == &vterm_widget);
    return 0;
}

void vterm_layout(widget_t *w) {
    assert(w->cls == &vterm_widget);
    // TODO reflow
}

void vterm_draw(widget_t *w) {
    assert(w->cls == &vterm_widget);
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

widget_cls vterm_widget = {
    .name = "vterm",
    .handle_ev = vterm_handle_ev,
    .draw = vterm_draw,
    .layout = vterm_layout,
};

