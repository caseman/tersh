#include "widget.h"
#include "vterm.h"

void w_vterm_draw(widget_t *w);
int w_vterm_handle_ev(widget_t *w, int event);

widget_cls widget_vterm = {
    .name = "vterm",
    .handle_ev = w_vterm_handle_ev,
    .draw = w_vterm_draw,
};
