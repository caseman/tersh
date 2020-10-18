#include "vec.h"
#include "widget.h"

typedef enum {
    lineedit_unchanged = 0,
    lineedit_changed = 1,
    lineedit_confirmed = 3,
    lineedit_cancelled = 4,
} lineedit_state_e;

typedef struct {
    int curs, curs_vis;
    unsigned int blink_time, elapsed;
    lineedit_state_e state;
    vec_wchar_t buf;
} lineedit_t;

widget_cls lineedit_widget;

int lineedit_insert(widget_t *w, wchar_t ch);
void lineedit_clear(widget_t *w);
lineedit_state_e lineedit_state(widget_t *w);
