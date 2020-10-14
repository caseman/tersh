#include "vec.h"

#define WIDGET_NEEDS_REDRAW 0x01
#define WIDGET_DELETED 0x2

#define CHILD_REORDER 0x0400

typedef enum {
    ANCHOR_LEFT = 0,
    ANCHOR_TOP = 1,
    ANCHOR_RIGHT = 2,
    ANCHOR_BOTTOM = 3,
} widget_anchor;

typedef struct widget widget_t;

typedef vec_t(widget_t *) vec_widget_t;

typedef int (*widget_event_f)(widget_t *w, int event);
typedef void (*widget_draw_f)(widget_t *w);

typedef struct {
    const char *name;
    int (*handle_ev)(widget_t *w, int event);
    void (*draw)(widget_t *w);
} widget_cls;

struct widget {
    widget_t *parent;
    vec_widget_t children;
    short order, flags;
    widget_anchor anchor;
    int min_width, max_width;
    int min_height, max_height;
    int left, top, width, height;
    widget_cls *cls;
    void *data;
};

void widget_init(widget_t *w);
widget_t *widget_new(widget_t config);
widget_t *widget_add(widget_t *parent);
widget_t *widget_addx(widget_t *parent, short order, widget_anchor anchor, int width, int height);
void widget_del(widget_t *w);
void widget_layout(widget_t *w, int left, int top, int right, int bottom);
void widget_refresh(widget_t *w);
