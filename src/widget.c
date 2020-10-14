#include <string.h>
#include "widget.h"

#define WIDGET_POOL_SIZE 128

typedef struct w_pool widget_pool;

struct w_pool {
    widget_t *new, *free;
    widget_pool *next_pool;
    widget_t widgets[WIDGET_POOL_SIZE];
};

widget_pool *_widget_p = NULL;

void widget_init(widget_t *w) {
    memset(w, 0, sizeof(widget_t));
}

static widget_pool *alloc_pool() {
    widget_pool *p = calloc(1, sizeof(widget_pool));
    if (p == NULL) return NULL;
    p->new = p->widgets;
    if (_widget_p != NULL) {
        p->free = _widget_p->free;
        _widget_p->free = NULL;
    }
    p->next_pool = _widget_p;
    _widget_p = p;
    return p;
}

widget_t *widget_new(widget_t config) {
    widget_t *w;
    if (_widget_p == NULL || 
        _widget_p->new >= _widget_p->widgets + WIDGET_POOL_SIZE) {
        if (alloc_pool() == NULL) return NULL;
    }
    if (_widget_p->free) {
        w = _widget_p->free;
        _widget_p->free = w->parent;
        widget_init(w);
    } else {
        w = _widget_p->new++;
    }
    memcpy(w, &config, sizeof(widget_t));
    return w;
}

void widget_del(widget_t *w) {
    int i;
    widget_t *child;
    vec_foreach(&w->children, child, i) {
        widget_del(child);
    }
    w->flags |= WIDGET_DELETED;
    vec_deinit(&w->children);
    w->parent = _widget_p->free;
    _widget_p->free = w;
}

widget_t *widget_add(widget_t *parent) {
    widget_t *w = widget_new();
    if (w == NULL) return NULL;
    w->parent = parent;
    parent->flags |= CHILD_REORDER;
    vec_push(&parent->children, w);
    return w;
}

widget_t *widget_addx(widget_t *parent, short order, widget_anchor anchor, int width, int height) {
    widget_t *w = widget_add(parent);
    if (w == NULL) return NULL;
    w->order = order;
    w->anchor = anchor;
    if (width >= 0) {
        w->min_width = w->max_width = width;
    } else {
        w->max_width = -1;
        w->min_width = -width;
    }
    if (height >= 0) {
        w->min_height = w->max_height = height;
    } else {
        w->max_height = -1;
        w->min_height = -height;
    }
    return w;
}

static int cmp_widgets(const void* a, const void* b) {
    widget_t **wa = (widget_t **)a;
    widget_t **wb = (widget_t **)b;
    return ((*wa)->order > (*wb)->order) - ((*wa)->order < (*wb)->order);
}

static void place_widget(widget_t *w, int left, int top, int right, int bottom) {
    switch (w->anchor) {
        case ANCHOR_LEFT:
            w->left = left;
            w->top = top;
            break;
        case ANCHOR_RIGHT:
            w->left = right - w->width;
            w->top = top;
            break;
        case ANCHOR_TOP:
            w->left = left;
            w->top = top;
            break;
        case ANCHOR_BOTTOM:
            w->left = left;
            w->top = bottom - w->height;
            break;
    }
    w->flags |= WIDGET_NEEDS_REDRAW;
}

void widget_layout(widget_t *w, int left, int top, int right, int bottom) {
    int height = bottom - top;
    int max_height = w->max_height < 0 || w->max_height > height ? height : w->max_height;
    int width = right - left;
    int max_width = w->max_width < 0 || w->max_width > width ? width : w->max_width;

    if (!w->children.length) {
        w->width = max_width > w->min_width ? max_width : w->min_width;
        w->height = max_height > w->min_height ? max_height : w->min_height;
        place_widget(w, left, top, right, bottom);
        return;
    }

    if (w->flags & CHILD_REORDER) {
        vec_sort(&w->children, cmp_widgets);
        w->flags &= ~CHILD_REORDER;
    }

    // Start with minimal container size
    switch (w->anchor) {
        case ANCHOR_LEFT:
        case ANCHOR_RIGHT:
            width = w->min_width;
        case ANCHOR_TOP:
        case ANCHOR_BOTTOM:
            height = w->min_height;
    }

    int i;
    int v_width = width, v_height = height;
    int overflow = 0;
    // layout children, expanding until they fit or we exceed a max dimension
    do {
        int inner_l = left, inner_t = top, inner_r = right, inner_b = bottom;
        switch (w->anchor) {
            case ANCHOR_LEFT:
                inner_r = left + width;
                break;
            case ANCHOR_RIGHT:
                inner_l = right - width;
                break;
            case ANCHOR_TOP:
                inner_b = top + height;
                break;
            case ANCHOR_BOTTOM:
                inner_t = bottom - height;
                break;
        }

        widget_t *child;
        vec_foreach(&w->children, child, i) {
            if (child->flags & WIDGET_DELETED) continue;
            widget_layout(child, inner_l, inner_t, inner_r, inner_b);
            switch (child->anchor) {
                case ANCHOR_LEFT:
                    inner_l += child->width;
                    break;
                case ANCHOR_RIGHT:
                    inner_r -= child->width;
                    break;
                case ANCHOR_TOP:
                    inner_t += child->height;
                    break;
                case ANCHOR_BOTTOM:
                    inner_b -= child->height;
                    break;
            }
        }

        switch (w->anchor) {
            case ANCHOR_LEFT:
            case ANCHOR_RIGHT:
                overflow = inner_l - inner_r;
                v_width += overflow * (overflow > 0);
                if (v_width >= max_width) {
                    width = max_width;
                    overflow = 0;
                }
                break;
            case ANCHOR_BOTTOM:
            case ANCHOR_TOP:
                overflow = inner_t - inner_b;
                v_height += overflow * (overflow > 0);
                if (v_height >= max_height) {
                    height = max_height;
                    overflow = 0;
                }
                break;
        }
    } while (overflow > 0);

    w->width = width;
    w->height = height;
    place_widget(w, left, top, right, bottom);
}

void widget_refresh(widget_t *w) {
    widget_layout(w, w->left, w->top, w->left + w->width, w->top + w->height);
}
