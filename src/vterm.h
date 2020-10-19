/* Thy terminal emulator widget */
#include "vec.h"
#include "BearLibTerminal.h"
#include "vtparse.h"
#include "widget.h"
#include "process.h"

#ifndef VTERM_H
#define VTERM_H

#define VT_NEEDS_REDRAW  1

#define VTCELL_INLINE_STYLE 0x80
#define VTCELL_DIRTY_FLAG 0x40
#define VTCELL_STARTS_LINE 0x20

#define VTCELL_STYLE_MASK 0x0FFFFFFF

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

typedef struct {
    int flags;
    int width, height;
    int curs_x, curs_y;
    int lines, scroll_pos;
    vtparse_t parser;
    process_t *process;
    vec_vterm_style_t styles;
    vec_vterm_cell_t line_buf;
    vec_uchar_t out_buf;
} vterm_t;

/*
 * Write bytes to the vterm. encoding of data is assumed to be utf-8
 */
int vterm_write(widget_t *w, unsigned char *data, unsigned int data_len);

/*
 * Write wide chars directly to the vterm.
 */
int vterm_writew(widget_t *w, int *data, unsigned int data_len);

/*
 * Return a pointer to any output bytes written by the vterm and clears
 * the vterm output buffer. returns NULL if nothing has been written. 
 * Caller must free the pointer returned.
 */
unsigned char *vterm_read(widget_t *w);

/*
 * Callback functions to use with process_spawn() when attaching the
 * vterm to a process.
 */
int vterm_process_data_cb(process_t *p, int fd, unsigned char *data, int data_len);
void vterm_process_event_cb(process_t *p, int fd, process_event_t event, intmax_t val);

widget_cls vterm_widget;

#endif
