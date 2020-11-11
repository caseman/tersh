/*
 * st: Simple Terminal
 * forked from st: git://git.suckless.org/st
 * License: MIT/X https://git.suckless.org/st/file/LICENSE.html
 */

#include <stdint.h>
#include <sys/types.h>
#include "vec.h"
#include "poller.h"

#ifndef ST_H
#define ST_H

/* Arbitrary sizes */
#define UTF_INVALID   0xFFFD
#define UTF_SIZ       4
#define ESC_BUF_SIZ   (128*UTF_SIZ)
#define ESC_ARG_SIZ   16
#define STR_BUF_SIZ   ESC_BUF_SIZ
#define STR_ARG_SIZ   ESC_ARG_SIZ

/* macros */
#define MIN(a, b)        ((a) < (b) ? (a) : (b))
#define MAX(a, b)        ((a) < (b) ? (b) : (a))
#define LEN(a)            (sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, a, b)    ((a) <= (x) && (x) <= (b))
#define DIVCEIL(n, d)        (((n) + ((d) - 1)) / (d))
#define DEFAULT(a, b)        (a) = (a) ? (a) : (b)
#define LIMIT(x, a, b)        (x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define ATTRCMP(a, b)        ((a).mode != (b).mode || (a).fg != (b).fg || \
                (a).bg != (b).bg)
#define TIMEDIFF(t1, t2)    ((t1.tv_sec-t2.tv_sec)*1000 + \
                (t1.tv_nsec-t2.tv_nsec)/1E6)
#define MODBIT(x, set, bit)    ((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))

#define TRUECOLOR(r,g,b)    (0xff000000 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x)        (1 << 24 & (x))
#define IS_SET(flag)        ((term->mode & (flag)) != 0)

enum glyph_attribute {
    ATTR_NULL       = 0,
    ATTR_BOLD       = 1 << 0,
    ATTR_FAINT      = 1 << 1,
    ATTR_ITALIC     = 1 << 2,
    ATTR_UNDERLINE  = 1 << 3,
    ATTR_BLINK      = 1 << 4,
    ATTR_REVERSE    = 1 << 5,
    ATTR_INVISIBLE  = 1 << 6,
    ATTR_STRUCK     = 1 << 7,
    ATTR_WRAP       = 1 << 8,
    ATTR_WIDE       = 1 << 9,
    ATTR_WDUMMY     = 1 << 10,
    ATTR_BOLD_FAINT = ATTR_BOLD | ATTR_FAINT,
};

enum selection_mode {
    SEL_IDLE = 0,
    SEL_EMPTY = 1,
    SEL_READY = 2
};

enum selection_type {
    SEL_REGULAR = 1,
    SEL_RECTANGULAR = 2
};

enum selection_snap {
    SNAP_WORD = 1,
    SNAP_LINE = 2
};

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;

typedef uint_least32_t Rune;

#define Glyph Glyph_
typedef struct {
    Rune u;           /* character code */
    ushort mode;      /* attribute flags */
    uint32_t fg;      /* foreground  */
    uint32_t bg;      /* background  */
} Glyph;

typedef Glyph *Line;

typedef union {
    int i;
    uint ui;
    float f;
    const void *v;
    const char *s;
} Arg;

enum term_mode {
    MODE_WRAP        = 1 << 0,
    MODE_INSERT      = 1 << 1,
    MODE_ALTSCREEN   = 1 << 2,
    MODE_CRLF        = 1 << 3,
    MODE_ECHO        = 1 << 4,
    MODE_PRINT       = 1 << 5,
    MODE_UTF8        = 1 << 6,
    MODE_VISIBLE     = 1 << 7,
    MODE_FOCUSED     = 1 << 8,
    MODE_APPKEYPAD   = 1 << 9,
    MODE_MOUSEBTN    = 1 << 10,
    MODE_MOUSEMOTION = 1 << 11,
    MODE_REVERSE     = 1 << 12,
    MODE_KBDLOCK     = 1 << 13,
    MODE_HIDE        = 1 << 14,
    MODE_APPCURSOR   = 1 << 15,
    MODE_MOUSESGR    = 1 << 16,
    MODE_8BIT        = 1 << 17,
    MODE_BLINK       = 1 << 18,
    MODE_FBLINK      = 1 << 19,
    MODE_TTYFOCUS    = 1 << 20,
    MODE_MOUSEX10    = 1 << 21,
    MODE_MOUSEMANY   = 1 << 22,
    MODE_BRCKTPASTE  = 1 << 23,
    MODE_NUMLOCK     = 1 << 24,
    MODE_MOUSE       = MODE_MOUSEBTN|MODE_MOUSEMOTION|MODE_MOUSEX10\
                      |MODE_MOUSEMANY,
};

enum cursor_movement {
    CURSOR_SAVE,
    CURSOR_LOAD
};

enum cursor_state {
    CURSOR_DEFAULT  = 0,
    CURSOR_WRAPNEXT = 1,
    CURSOR_ORIGIN   = 2
};

enum charset {
    CS_GRAPHIC0,
    CS_GRAPHIC1,
    CS_UK,
    CS_USA,
    CS_MULTI,
    CS_GER,
    CS_FIN
};

enum escape_state {
    ESC_START      = 1,
    ESC_CSI        = 2,
    ESC_STR        = 4,  /* DCS, OSC, PM, APC */
    ESC_ALTCHARSET = 8,
    ESC_STR_END    = 16, /* a final string was encountered */
    ESC_TEST       = 32, /* Enter in test mode */
    ESC_UTF8       = 64,
};

typedef struct {
    Glyph attr; /* current char attributes */
    int x;
    int y;
    char state;
} TCursor;

typedef struct {
    int mode;
    int type;
    int snap;
    /*
     * Selection variables:
     * nb – normalized coordinates of the beginning of the selection
     * ne – normalized coordinates of the end of the selection
     * ob – original coordinates of the beginning of the selection
     * oe – original coordinates of the end of the selection
     */
    struct {
        int x, y;
    } nb, ne, ob, oe;

    int alt;
} Selection;

/* CSI Escape sequence structs */
/* ESC '[' [[ [<priv>] <arg> [;]] <mode> [<mode>]] */
typedef struct {
    char buf[ESC_BUF_SIZ]; /* raw string */
    size_t len;            /* raw string length */
    char priv;
    int arg[ESC_ARG_SIZ];
    int narg;              /* nb of args */
    char mode[2];
} CSIEscape;

/* STR Escape sequence structs */
/* ESC type [[ [<priv>] <arg> [;]] <mode>] ESC '\' */
typedef struct {
    char type;             /* ESC type ... */
    char *buf;             /* allocated raw string */
    size_t siz;            /* allocation size */
    size_t len;            /* raw string length */
    char *args[STR_ARG_SIZ];
    int narg;              /* nb of args */
} STREscape;

/* Internal representation of the screen */
typedef struct {
    int row;      /* nb row */
    int col;      /* nb col */
    int nlines;   /* number of lines used */
    int lastx, lasty;
    Line *line;   /* screen */
    Line *alt;    /* alternate screen */
    int *dirty;   /* dirtyness of lines */
    TCursor c;    /* cursor */
    int ocx;      /* old cursor col */
    int ocy;      /* old cursor row */
    int top;      /* top    scroll limit */
    int bot;      /* bottom scroll limit */
    int mode;     /* terminal mode flags */
    int esc;      /* escape state flags */
    char trantbl[4]; /* charset table translation */
    int charset;  /* current charset */
    int icharset; /* selected charset for sequence */
    int *tabs;
    Rune lastc;   /* last printed char outside of sequence, 0 if control */
    Selection sel;
    CSIEscape csiescseq;
    STREscape strescseq;
    int iofd;
    int cmdfd;
    pid_t pid;
    int childstopped;
    int childexited;
    int childexitst;
    vec_char_t wbuf; /* write buffer */
    int wbuf_offs; /* offset into write buffer */
    int cursorshape;
    int blinkelapsed;
} Term;

void die(const char *, ...);
void redraw(Term *term);
void tdraw(Term *term);

void printscreen(Term *term, const Arg *);
void printsel(Term *term, const Arg *);
void sendbreak(Term *term, const Arg *);
void toggleprinter(Term *term, const Arg *);

int tattrset(Term *term, int);
void tnew(Term *term, int, int);
void tresize(Term *term, int, int);
void tsetdirtattr(Term *term, int);
void ttyhangup(Term *term);
int ttynew(Term *term, char *, char *, char *, char **);
int st_fork_pty(Term *term);
void st_set_child_status(Term *term, int status);
size_t term_read(Term *term);
void ttyresize(Term *term, int, int);
void ttywrite(Term *term, const char *, size_t, int);
void st_print(Term *term, const char *s, int len);
void st_perror(Term *term, char *s);
void st_set_focused(Term *term, int);
int st_set_cursor(Term *term, int cursor);

void st_on_poll(int fd, void *data, int events);

void resettitle(Term *term);

void selclear(Term *term);
void selinit(Term *term);
void selstart(Term *term, int, int, int);
void selextend(Term *term, int, int, int, int);
int selected(Term *term, int, int);
char *getsel(Term *term);

size_t utf8encode(Rune, char *);

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
char *xstrdup(char *);

/* config.h globals */
extern char *utmp;
extern char *scroll;
extern char *stty_args;
extern char *vtiden;
extern wchar_t *worddelimiters;
extern int allowaltscreen;
extern int allowwindowops;
extern char *termname;
extern unsigned int tabspaces;
extern unsigned int defaultfg;
extern unsigned int defaultbg;
extern unsigned int blinktimeout;
extern int customcursor;

#endif
