/* Thy vterminal emulator */
#include "vec.h"
#include "BearLibTerminal.h"
#include "vtparse.h"

#define VT_NEEDS_REDRAW  1

#define VTCELL_INLINE_STYLE 0x80
#define VTCELL_DIRTY_FLAG 0x40
#define VTCELL_WRAPPED_FLAG 0x20

#define VTCELL_STYLE_MASK 0x0FFFFFFF

typedef vec_t(wchar_t) vec_wchar_t;

typedef struct {
    color_t fgcolor;
    color_t bkcolor;
} vterm_style_t;

typedef struct {
    union {
        struct {
            unsigned char flags;
            unsigned char fgcolor;
            unsigned char bgcolor;
        };
        unsigned int style;
    };
    unsigned int ch;
} vterm_cell_t;

typedef vec_t(vterm_style_t) vec_vterm_style_t;
typedef vec_t(vterm_cell_t) vec_vterm_cell_t;
typedef vec_t(unsigned char) vec_uchar_t;

typedef struct {
    int flags;
    int left, top, width, height;
    int curs_x, curs_y;
    int lines, scroll_pos;
    vtparse_t parser;
    vec_vterm_style_t styles;
    vec_vterm_cell_t line_buf;
    vec_uchar_t out_buf;
} vterm_t;

int vterm_init(vterm_t *vt, int left, int top, int width, int height);
void vterm_deinit(vterm_t *t);
int vterm_write(vterm_t *t, unsigned char *data, unsigned int data_len);
unsigned char *vterm_read(vterm_t *t);
void vterm_draw(vterm_t *vt);
