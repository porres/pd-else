/* Copyright (c) 2003-2004 krzYszcz and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#ifndef __MOUSE_GUI_H__
#define __MOUSE_GUI_H__


typedef struct _mouse_gui{
    t_pd       g_pd;
    t_symbol  *g_psgui;
    t_symbol  *g_psmouse;
    t_symbol  *g_pspoll;
    t_symbol  *g_psfocus;
    t_symbol  *g_psvised;
    int        g_isup;
}t_mouse_gui;

void mouse_gui_getscreen(void);
void mouse_gui_getscreenfocused(void);
void mouse_gui_getwc(t_pd *master);
void mouse_gui_bindmouse(t_pd *master);
void mouse_gui_unbindmouse(t_pd *master);
void mouse_gui_willpoll(void);
void mouse_gui_startpolling(t_pd *master, int pollmode);
void mouse_gui_stoppolling(t_pd *master);
void mouse_gui_bindfocus(t_pd *master);
void mouse_gui_unbindfocus(t_pd *master);
void mouse_gui_bindvised(t_pd *master);
void mouse_gui_unbindvised(t_pd *master);

#endif
