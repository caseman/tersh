/* 
 * st: Simple Terminal
 * forked from st: git://git.suckless.org/st
 * License: MIT/X https://git.suckless.org/st/file/LICENSE.html
 */
#include "st.h"
#include "widget.h"

widget_cls st_widget;

void xbell(void);
void xclipcopy(void);
void xfinishdraw(void);
void xloadcols(void);
int xsetcolorname(int, const char *);
void xseticontitle(char *);
void xsettitle(char *);
int xsetcursor(int);
void xsetpointermotion(int);
void xsetsel(char *);
int xstartdraw(void);
void xximspot(int, int);
