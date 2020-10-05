/* Thy vterminal emulator */
#include "vec.h"
#include "BearLibTerminal.h"
#include "vtparse.h"

#define TERM_FULL_REDRAW  1
// #define TERM_VT100_FLAG  1

typedef vec_t(wchar_t) vec_wchar_t;

typedef struct {
    int buf_pos;
    color_t fgcolor;
    color_t bkcolor;
} vterm_state_t;

typedef vec_t(vterm_state_t) vec_vterm_state_t;
typedef vec_t(unsigned char) vec_uchar_t;

typedef struct {
    int flags, dirty_count;
    int left, top, width, height;
    int curs, curs_x, curs_y;
    vtparse_t parser;
    vec_wchar_t buf;
    vec_uchar_t out;
    vec_vterm_state_t states;
} vterm_t;

int vterm_init(vterm_t *vt, int left, int top, int width, int height);
void vterm_deinit(vterm_t *t);
int vterm_write(vterm_t *t, unsigned char *data, unsigned int data_len);
unsigned char *vterm_output(vterm_t *t);
void vterm_draw(vterm_t *vt);
