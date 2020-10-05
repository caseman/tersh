#include "stdio.h"
#include "err.h"
#include "vterm.h"

void parser_handle(vtparse_t *parser, vtparse_action_t action, unsigned int ch) {
    vterm_t *vt = parser->user_data;
    switch (action) {
        case VTPARSE_ACTION_PRINT:
            if (vt->curs == -1) {
                vec_push(&vt->buf, (wchar_t)ch);
                vt->dirty_count++;
            }
            break;
        default:
            fprintf(stderr, "vterm ignored %s (%c) ", ACTION_NAMES[action], ch);
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

int vterm_init(vterm_t *vt, int left, int top, int width, int height) {
    memset(vt, 0, sizeof(vterm_t));
    vtparse_init(&vt->parser, parser_handle);
    vt->parser.user_data = vt;
    return_err(vec_reserve(&vt->buf, 256));
    vt->left = left;
    vt->top = top;
    vt->width = width;
    vt->height = height;
    vt->curs = -1;
    vt->curs_y = -1;
    vt->flags = TERM_FULL_REDRAW;
    return 0;
}

void vterm_deinit(vterm_t *vt) {
    vec_deinit(&vt->buf);
    vec_deinit(&vt->out);
    vec_deinit(&vt->states);
}

int vterm_write(vterm_t *vt, unsigned char *data, unsigned int data_len) {
    vtparse(&vt->parser, data, data_len);
    return 0;
}

unsigned char *vterm_output(vterm_t *vt) {
    if (vt->out.length) {
        unsigned char *data = vt->out.data;
        vec_init(&vt->out);
        return data;
    }
    return NULL;
}

static inline void print_line(vterm_t *vt, wchar_t *ch, int len, int y) {
    for (int x = 0; x < len; x++) {
        terminal_put(x, y, *(ch++));
    }
}

void vterm_draw(vterm_t *vt) {
    int pos;
    int line_y = vt->top + vt->height - 1;
    int line_len = 0;
    wchar_t *ch;
    terminal_clear_area(vt->left, vt->top, vt->width, vt->height);
    vec_foreach_ptr_rev(&vt->buf, ch, pos) {
        if (line_len < vt->width && *ch != 10) {
            line_len++;
        } else {
            print_line(vt, ch, line_len, line_y);
            line_y--;
            line_len = 0;
            if (line_y < vt->top) break;
        }
    }
    print_line(vt, ch, line_len, line_y);
    vt->dirty_count = 0;
    vt->flags &= ~TERM_FULL_REDRAW;
}
