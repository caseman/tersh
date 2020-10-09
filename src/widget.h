#include "vec.h"

#define WIDGET_DELETED 0x4

#define CHILD_REORDER 0x0400

typedef enum {
    ANCHOR_LEFT = 0,
    ANCHOR_TOP = 1,
    ANCHOR_RIGHT = 2,
    ANCHOR_BOTTOM = 3,
} widget_anchor;

typedef struct widget widget_t;

typedef vec_t(widget_t *) vec_widget_t;

typedef int (*widget_event_cb)(widget_t *w, int event);

struct widget {
    widget_t *parent;
    vec_widget_t children;
    short order;
    short flags, del_children;
    widget_anchor anchor;
    int min_width, max_width;
    int min_height, max_height;
    int left, top, width, height;
    widget_event_cb event_cb;
    void *data;
};

void widget_init(widget_t *w);
widget_t *widget_new();
widget_t *widget_add(widget_t *parent);
widget_t *widget_addx(widget_t *parent, short order, widget_anchor anchor, int width, int height);
void widget_del(widget_t *w);
void widget_layout(widget_t *w, int left, int top, int right, int bottom);
void widget_refresh(widget_t *w);
