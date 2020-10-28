#include <string.h>
#include "smalloc.h"
#include "widget.h"
#include "BearLibTerminal.h"

smalloc_pool_t widget_pool = SMALLOC_POOL(sizeof(widget_t));

widget_cls no_widget_cls = { .name = "(no class)" };

widget_t *widget_new(widget_t config) {
    widget_t *w = smalloc(&widget_pool);
    if (w == NULL) return NULL;
    memcpy(w, &config, sizeof(widget_t));
    vec_init(&w->children);
    if (w->parent) {
        w->parent->flags |= CHILD_REORDER;
        vec_push(&w->parent->children, w);
    }
    if (w->cls == NULL) {
        w->cls = &no_widget_cls;
    }
    if (w->cls->init) {
        int err = w->cls->init(w);
        if (err) {
            // TODO log errors
            smfree(&widget_pool, w);
            return NULL;
        }
    }
    return w;
}

static void del_recursive(widget_t *w) {
    int i;
    widget_t *child;
    vec_foreach(&w->children, child, i) {
        del_recursive(child);
    }
    if (w->cls->del) w->cls->del(w);
    vec_deinit(&w->children);
    smfree(&widget_pool, w);
}

void widget_del(widget_t *w) {
    int i;
    widget_t *child;
    if (w->parent) {
        vec_foreach(&w->parent->children, child, i) {
            if (child == w) {
                vec_del(&w->parent->children, i);
                break;
            }
        }
    }
    del_recursive(w);
}

void widget_update(widget_t *w, unsigned int dt) {
    int i;
    widget_t *child;
    vec_foreach(&w->children, child, i) {
        widget_update(child, dt);
    }
    if (w->cls->update) w->cls->update(w, dt);
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

static int cmp_widgets(const void* a, const void* b) {
    widget_t **wa = (widget_t **)a;
    widget_t **wb = (widget_t **)b;
    return ((*wa)->order > (*wb)->order) - ((*wa)->order < (*wb)->order);
}

static void order_children(widget_t *w) {
    if (w->flags & CHILD_REORDER) {
        vec_sort(&w->children, cmp_widgets);
        w->flags &= ~CHILD_REORDER;
    }
}

void widget_layout(widget_t *w, int left, int top, int right, int bottom) {
    int height = bottom - top;
    int max_height = w->max_height < 0 || w->max_height > height ? height : w->max_height;
    int min_height = w->min_height >= 0 ? w->min_height : max_height + w->min_height + 1;
    int width = right - left;
    int max_width = w->max_width < 0 || w->max_width > width ? width : w->max_width;
    int min_width = w->min_width >= 0 ? w->min_width : max_width + w->min_width + 1;

    if (!w->children.length) {
        w->width = max_width > min_width ? max_width : min_width;
        w->height = max_height > min_height ? max_height : min_height;
        /*
        if (w->order < 0)
            printf("layout %d maxw=%d minw=%d w=%d maxh=%d minh=%d h=%d\n",
                w->order,
                w->max_width, w->min_width, w->width,
                w->max_height, w->min_height, w->height);
        */
        if (w->cls->layout) w->cls->layout(w);
        place_widget(w, left, top, right, bottom);
        return;
    }

    order_children(w);

    // Start with minimal container size
    switch (w->anchor) {
        case ANCHOR_LEFT:
        case ANCHOR_RIGHT:
            w->width = min_width;
            w->height = height;
            break;
        case ANCHOR_TOP:
        case ANCHOR_BOTTOM:
            w->width = width;
            w->height = min_height;
            break;
    }

    int i;
    int overflow = 0;
    // layout children, expanding until they fit or we exceed a max dimension
    do {
        // Allow the layout hook to adjust dimensions
        if (w->cls->layout) w->cls->layout(w);

        int inner_l = left, inner_t = top, inner_r = right, inner_b = bottom;
        switch (w->anchor) {
            case ANCHOR_LEFT:
                inner_r = left + w->width;
                break;
            case ANCHOR_RIGHT:
                inner_l = right - w->width;
                break;
            case ANCHOR_TOP:
                inner_b = top + w->height;
                break;
            case ANCHOR_BOTTOM:
                inner_t = bottom - w->height;
                break;
        }

        widget_t *child;
        vec_foreach(&w->children, child, i) {
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
                w->width += overflow * (overflow > 0);
                if (w->width >= max_width) {
                    w->width = max_width;
                    overflow = 0;
                }
                break;
            case ANCHOR_BOTTOM:
            case ANCHOR_TOP:
                overflow = inner_t - inner_b;
                w->height += overflow * (overflow > 0);
                if (w->height >= max_height) {
                    w->height = max_height;
                    overflow = 0;
                }
                break;
        }
    } while (overflow > 0);

    place_widget(w, left, top, right, bottom);
}

void widget_relayout(widget_t *w) {
    widget_layout(w, w->left, w->top, w->left + w->width, w->top + w->height);
}

void widget_draw(widget_t *w) {
    if (w->order % 2) {
        terminal_bkcolor(0xff222222);
    } else {
        terminal_bkcolor(0xff444444);
    }
    //terminal_crop(w->left, w->top, w->width, w->height);
    /* Note this leaves it up to the widget drawing routines to clear
     * the widget rect if it is needed */
    if (w->cls->draw) w->cls->draw(w);
    w->flags &= ~WIDGET_NEEDS_REDRAW;
    if (w->children.length) {
        int i;
        widget_t *child;
        order_children(w);
        vec_foreach(&w->children, child, i) {
            widget_draw(child);
        }
    }
}
