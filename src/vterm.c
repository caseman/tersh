#include "stdio.h"
#include "assert.h"
#include "err.h"
#include "vterm.h"
#include "smalloc.h"

smalloc_pool_t vterm_pool = SMALLOC_POOL(sizeof(vterm_t));

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
            if (x == 0) {
                cell->flags |= VTCELL_STARTS_LINE;
            }

            while (*print_ch) {
                if (++x == w) {
                    // EOL
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

int vterm_init(widget_t *w) {
    vterm_t *vt = smalloc(&vterm_pool);
    if (vt == NULL) return -1;
    w->data = vt;
    vtparse_init(&vt->parser, parser_handle);
    vt->parser.user_data = vt;
    vt->flags = VT_NEEDS_REDRAW;
    return 0;
}

void vterm_del(widget_t *w) {
    vterm_t *vt = widget_data(w, &vterm_widget);
    vec_deinit(&vt->line_buf);
    vec_deinit(&vt->out_buf);
    vec_deinit(&vt->styles);
    smfree(&vterm_pool, vt);
}

int vterm_write(widget_t *w, unsigned char *data, unsigned int data_len) {
    vterm_t *vt = widget_data(w, &vterm_widget);
    vtparse(&vt->parser, data, data_len);
    return 0;
}

int vterm_writew(widget_t *w, int *data, unsigned int data_len) {
    vterm_t *vt = widget_data(w, &vterm_widget);
    vtparsew(&vt->parser, data, data_len);
    return 0;
}

unsigned char *vterm_read(widget_t *w) {
    vterm_t *vt = widget_data(w, &vterm_widget);
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
    vterm_t *vt = widget_data(w, &vterm_widget);
    if (vt->width != w->width && vt->line_buf.length) {
        // Reflow content
        vterm_cell_t *old = vt->line_buf.data;
        vec_init(&vt->line_buf);
        printf("Old: %d,%d | New %d,%d\n", vt->width, vt->height, w->width, w->height);
        int new_size = w->width * w->height;
        int old_size = vt->width * vt->lines;
        vec_reserve(&vt->line_buf, new_size > old_size ? new_size : old_size);
        int x = 0;
        int y = 0;
        vterm_cell_t *cell;
        vterm_cell_t *end = old + vt->width * vt->lines;
        for (cell = old; cell < end; cell++, x++) {
            if (x >= w->width) {
                // skip trailing empty cells when narrowing
                while (cell < end && !cell->ch) {
                    cell++;
                }
                if (cell == end) break;
                // wrap
                x = 0;
                y++;
            }
            if (cell->flags & VTCELL_STARTS_LINE && x > 0) {
                // Skip to start of next line
                vec_push_zeros(&vt->line_buf, w->width - x);
                x = 0;
                y++;
            }
            if(x == 0) printf("line start y=%d %d then %d\n", y, (cell-1)->ch, cell->ch);
            vec_push(&vt->line_buf, *cell);
        }
        if (x > 0 && x < w->width - 1) {
            // Fill out end of last line
            vec_push_zeros(&vt->line_buf, (w->width - 1) - x);
        }
        vt->lines = y + 1;
        free(old);
    }
    vt->width = w->width;
    vt->height = w->height;
    w->flags |= WIDGET_NEEDS_REDRAW;
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
        terminal_clear_area(w->left, w->top + w->height - empty_lines_bottom,
                w->width, empty_lines_bottom);
    }
    // defensive: insure we don't draw beyond the end of the line buffer
    int bottom_y = w->top + w->height - (empty_lines_bottom > 0) * empty_lines_bottom;
    vterm_cell_t *cell = vt->line_buf.data + first_line * vt->width;
    if (full_redraw) {
        // Short circuit checking individual dirty flags and draw everything
        for (int screen_y = w->top + empty_lines_top; screen_y < bottom_y; screen_y++) {
            for (int screen_x = 0; screen_x < vt->width; screen_x++, cell++) {
                if (cell->ch) terminal_put(screen_x, screen_y, cell->ch);
                cell->flags &= ~VTCELL_DIRTY_FLAG;
            }
        }
    } else {
        for (int screen_y = w->top + empty_lines_top; screen_y < bottom_y; screen_y++) {
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
    .init = vterm_init,
    .del = vterm_del,
};

