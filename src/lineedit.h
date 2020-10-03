#define LINEEDIT_CANCEL -1
#define LINEEDIT_UNCHANGED 0
#define LINEEDIT_CHANGED 1
#define LINEEDIT_EXIT 2
#define LINEEDIT_CONFIRM 3

typedef struct {
    int len, alloc, curs, curs_vis;
    wchar_t *buf;
} lineedit;

int lineedit_init(lineedit *le);
int lineedit_insert(lineedit *le, wchar_t ch);
int lineedit_handle(lineedit *le, int event);
void lineedit_blink(lineedit *le);
int lineedit_draw(lineedit *le);
