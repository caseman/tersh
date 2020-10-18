#include <assert.h>
#include "vec.h"

#ifndef WIDGET_H
#define WIDGET_H

#define WIDGET_NEEDS_REDRAW 0x01

#define CHILD_REORDER 0x0400

typedef enum {
    ANCHOR_LEFT = 0,
    ANCHOR_TOP = 1,
    ANCHOR_RIGHT = 2,
    ANCHOR_BOTTOM = 3,
} widget_anchor;

typedef struct widget widget_t;

typedef vec_t(widget_t *) vec_widget_t;

/*
 * Widget class binds methods to widgets
 * All method functions are optional
 */
typedef struct {
    const char *name;
    /*
     * UI event handler method
     * non-zero return value indicates the event was handled
     * zero return value will bubble the event to the widget's parent
     */
    int (*handle_ev)(widget_t *w, int event);
    /*
     * periodic update method
     * dt is the timedelta in millis since the last update
     */
    void (*update)(widget_t *w, unsigned int dt);
    /*
     * layout hook method, allows the widget to adjust its dimensions, etc
     * during widget layout
     */
    void (*layout)(widget_t *w);
    /*
     * draw method, generally only this method updates the view
     */
    void (*draw)(widget_t *w);
    /*
     * delete hook, called before widget is deleted
     */
    void (*del)(widget_t *w);
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

/*
 * widget_new() creates a new widget configured from the attributes in config
 * if a parent is specified, the widget is added to it
 */
widget_t *widget_new(widget_t config);

/*
 * widget_data() is a convenience macro to access the class-specific
 * data from the widget with some sanity checking
 */
#define widget_data(w, w_cls)\
    (assert((w)->cls == (w_cls)), assert((w)->data), (w)->data)

/*
 * widget_del() deletes a widget and its children, removing it from its parent
 */
void widget_del(widget_t *w);

/*
 * widget_layout() calculates the widget bounds for the widget and all of
 * its children within the rectangle specified
 */
void widget_layout(widget_t *w, int left, int top, int right, int bottom);

/*
 * widget_relayout() recalculates the layout of its children within its
 * existing boundary rect
 */
void widget_relayout(widget_t *w);

/*
 * widget_draw() draws a widget and its children
 */
void widget_draw(widget_t *w);

/*
 * widget_update() dispatches updates for the widget and its children
 */
void widget_update(widget_t *w, unsigned int dt);

#endif
