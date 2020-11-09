#include <wchar.h>
#include "BearLibTerminal.h"
#include "st.h"
#include "ui.h"

/*
 * label widget
 */
int label_set_text(widget_t *w, wchar_t *s, size_t len) {
    w->data = realloc(w->data, (len + 1) * sizeof(wchar_t));
    if (!w->data) return -1;
    wcsncpy(w->data, s, len);
    ((wchar_t *)w->data)[len] = 0;
    dimensions_t dims = terminal_wmeasure(w->data);
    w->max_width = dims.width;
    return 0;
}

void label_layout(widget_t *w) {
    if (w->max_height > 0 && w->width < w->max_width) {
        w->height = w->max_width / w->width + 1;
        w->height = w->height < w->max_height ? w->height : w->max_height;
    }
}

void label_draw(widget_t *w) {
    if (!w->data) return;
    terminal_color(0xffffffff);
    terminal_wprint_ext(w->left, w->top, w->width, w->height, TK_ALIGN_DEFAULT, w->data);
}

void label_del(widget_t *w) {
    if (w->data) {
        free(w->data);
    }
}

widget_cls label_widget = {
    .name = "label",
    // .handle_ev = label_handle_ev,
    .draw = label_draw,
    .layout = label_layout,
    .del = label_del,
};

/*
 * container widget
 */
void container_set_bkcolor(widget_t *w, int bkcolor) {
    w->data_int = bkcolor;
}

void container_draw(widget_t *w) {
    if (!w->data_int) return;
    terminal_bkcolor(w->data_int);
    terminal_clear_area(w->left, w->top, w->width, w->height);
    terminal_layer(1);
    terminal_color(0xff555555);
    for (int x = w->left; x < w->left + w->width; x++) {
        terminal_put_ext(x, w->top + w->height - 1, 0, 1, 0x2581, NULL);
    }
    terminal_layer(0);
}

widget_cls container_widget = {
    .name = "container",
    .draw = container_draw,
};

/*
 * job spinner widget
 */

void job_spinner_set(widget_t *w, int status) {
    w->data_int = status;
}

const wchar_t *spinner_frames = L"\uE000\uE001\uE002\uE003\uE004\uE005\uE006\uE007";
const int spinner_frame_time = 100;

void job_spinner_update(widget_t *w, unsigned int dt) {
    if (w->data_int >= 0) {
        w->data_int += dt;
    }
}

void job_spinner_draw(widget_t *w) {
    static int frames = 0;
    if (!frames) {
        frames = wcslen(spinner_frames);
    }
    wchar_t u = 0;
    int offy = -1;
    if (w->data_int >= 0) {
        int frame = (w->data_int / spinner_frame_time) % frames;
        terminal_color(0xffffffff);
        u = spinner_frames[frame];
        offy = 0;
    } else {
        switch (w->data_int) {
            case JOB_EXIT_ZERO:
                terminal_color(0xff00aa00);
                u = 0x2714;
                break;
            case JOB_EXIT_NONZERO:
                terminal_color(0xffcc0000);
                u = 0x2762;
                break;
            case JOB_EXIT_SIGNAL:
                terminal_color(0xffffff00);
                u = 0x21AF;
                break;
            case JOB_STOPPED:
                terminal_color(0xffffff00);
                u = 0x25C9;
                break;
            case JOB_CMD_ERROR:
                terminal_color(0xffaa0000);
                u = 0x2716;
                break;
        }
    }
    terminal_put_ext(w->left, w->top, 4, offy, u, NULL);
}

widget_cls job_spinner_widget = {
    .name = "job spinner",
    .update = job_spinner_update,
    .draw = job_spinner_draw,
};

/*
 * job widget
 */

widget_t *job_widget_new(widget_t *parent, int order, Term *term, wchar_t *cmd, size_t cmd_len) {
    widget_t *job = widget_new((widget_t){
        .cls = &job_widget,
        .parent = parent,
        .order = order,
        .data = term,
        .anchor = ANCHOR_BOTTOM,
        .max_width = -1,
        .max_height = -1,
    });
    if (job == NULL) return NULL;
    widget_new((widget_t){
        .cls = &st_widget,
        .parent = job,
        .data = term,
        .anchor = ANCHOR_BOTTOM,
        .min_width = 10,
        .max_width = -1,
    });
    widget_t *status = widget_new((widget_t){
        .cls = &container_widget,
        .parent = job,
        .anchor = ANCHOR_BOTTOM,
        .min_width = 10,
        .max_width = -1,
        .min_height = 1,
        .max_height = 1,
    });
    if (status == NULL) return NULL;
    container_set_bkcolor(status, 0xff333333);
    widget_new((widget_t){
        .cls = &job_spinner_widget,
        .parent = status,
        .anchor = ANCHOR_LEFT,
        .min_width = 2,
        .min_height = 1,
    });
    widget_t *cmd_label = widget_new((widget_t){
        .cls = &label_widget,
        .parent = status,
        .anchor = ANCHOR_LEFT,
        .min_width = 8,
        .min_height = 1,
    });
    if (cmd_label == NULL) return NULL;
    label_set_text(cmd_label, cmd, cmd_len);
    return job;
}

void job_update(widget_t *w, unsigned int dt) {
    Term *term = w->data;
    assert(term);
    assert(w->children.length >= 2);
    widget_t *status = w->children.data[1];
    widget_t *spinner = status->children.data[0];
    // widget_t *cmd = status->children.data[1];
    if (term->childexited) {
        if (term->childexitst == 0) {
            job_spinner_set(spinner, JOB_EXIT_ZERO);
        } else if (term->childexitst < 128) {
            job_spinner_set(spinner, JOB_EXIT_NONZERO);
        } else {
            job_spinner_set(spinner, JOB_EXIT_SIGNAL);
        }
    } else if (term->childstopped) {
        job_spinner_set(spinner, JOB_STOPPED);
    } else if (spinner->data_int < JOB_RUNNING) {
        job_spinner_set(spinner, JOB_RUNNING);
    }
}

widget_cls job_widget = {
    .name = "job",
    // .handle_ev = st_handle_ev,
    .update = job_update,
};
