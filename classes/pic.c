// moonlib

#include "m_pd.h"
#include "m_imp.h"
#include "g_canvas.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

// ------------------------ pic ----------------------------- //

static t_class *pic_class;

typedef struct _pic{
    t_object    x_obj;
    t_glist    *x_glist;
    int         x_width;
    int         x_height;
    t_symbol   *x_filename;
    t_symbol   *x_arg;
    int         x_def_img;
} t_pic;

// widget helper functions

const char *pic_get_filename(t_pic *x,char *file){
    static char fname[MAXPDSTRING];
    char *bufptr;
    int fd = open_via_path(canvas_getdir(glist_getcanvas(x->x_glist))->s_name, file, "",
                           fname, &bufptr, MAXPDSTRING, 1);
    if(fd > 0){
        fname[strlen(fname)] = '/';
        close(fd);
        return fname;
    }
    else
        return 0;
}

void pic_drawme(t_pic *x, t_glist *glist, int displace){
    if(displace){
        sys_vgui(".x%lx.c coords %xS \
                 %d %d\n", glist_getcanvas(glist), x,
                 text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist));
    }
    else{
        const char *fname = pic_get_filename(x, x->x_filename->s_name);
        if(x->x_filename == &s_){ // if blank, use default
            x->x_filename = gensym("else_pic_def_img");
            x->x_def_img = 1;
        }
        else if(!fname){
            pd_error(x, "[pic]: error opening file '%s'", x->x_filename->s_name);
            x->x_filename = gensym("else_pic_def_img");
            x->x_def_img = 1;
        }
        if(x->x_def_img){
            sys_vgui(".x%lx.c create image %d %d -tags %xS\n", glist_getcanvas(glist),
                     text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist), x);
            sys_vgui(".x%lx.c itemconfigure %xS -image %s\n",
                     glist_getcanvas(glist), x, x->x_filename->s_name);
        }
        else{
            sys_vgui("image create photo img%x\n", x); // if no local image ?
            sys_vgui("::else::pic::configure .x%lx img%x {%s}\n", x, x, fname);
            sys_vgui(".x%lx.c create image %d %d -image img%x -tags %xS\n",
                     glist_getcanvas(glist), text_xpix(&x->x_obj, glist),
                     text_ypix(&x->x_obj, glist), x, x);
        }
        // TODO callback from gui sys_vgui("pic_size logo");
    }
}

void pic_erase(t_pic *x,t_glist *glist){
    sys_vgui(".x%lx.c delete %xS\n", glist_getcanvas(glist), x);
}

// ------------------------ pic widgetbehaviour-----------------------------

static void pic_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    int width, height;
    t_pic *x = (t_pic *)z;
    width = x->x_width;
    height = x->x_height;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = text_xpix(&x->x_obj, glist) + width;
    *yp2 = text_ypix(&x->x_obj, glist) + height;
}

static void pic_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_pic *x = (t_pic *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    sys_vgui(".x%lx.c coords %xSEL %d %d %d %d\n", glist_getcanvas(glist), x,
             text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist),
             text_xpix(&x->x_obj, glist) + x->x_width, text_ypix(&x->x_obj, glist) + x->x_height);
    pic_drawme(x, glist, 1);
    canvas_fixlinesfor(glist,(t_text *) x);
}

static void pic_select(t_gobj *z, t_glist *glist, int state){
    t_pic *x = (t_pic *)z;
    if(state){
        sys_vgui(".x%lx.c create rectangle \
%d %d %d %d -tags %xSEL -outline blue\n",
            glist_getcanvas(glist), text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist),
            text_xpix(&x->x_obj, glist) + x->x_width, text_ypix(&x->x_obj, glist) + x->x_height, x);
    }
    else
        sys_vgui(".x%lx.c delete %xSEL\n", glist_getcanvas(glist), x);
}

static void pic_delete(t_gobj *z, t_glist *glist){
    t_text *x = (t_text *)z;
    canvas_deletelinesfor(glist, x);
}

static void pic_vis(t_gobj *z, t_glist *glist, int vis){
    t_pic *s = (t_pic *)z;
    if(vis)
        pic_drawme(s, glist, 0);
    else
        pic_erase(s, glist);
}

static void pic_save(t_gobj *z, t_binbuf *b){
    t_pic *x = (t_pic *)z;
    if(x->x_filename == gensym("else_pic_def_img")){
        if(x->x_arg != &s_)
            x->x_filename = x->x_arg;
        else
            x->x_filename = &s_;
    }
    binbuf_addv(b, "ssiiss", gensym("#X"), gensym("obj"), x->x_obj.te_xpix, x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)), x->x_filename);
    binbuf_addv(b, ";");
}

t_widgetbehavior   pic_widgetbehavior;

void pic_open(t_gobj *z,t_symbol *file){
    t_pic *x = (t_pic *)z;
    int default_pic = x->x_def_img;
    const char *fname = pic_get_filename(x, file->s_name);
    if(fname){
        x->x_filename = file;
        x->x_def_img = 0;
        if(glist_isvisible(x->x_glist)){
            sys_vgui("image create photo img%x\n",x); // if no local image ?
            sys_vgui("img%x blank\n", x);
            sys_vgui("::else::pic::configure .x%lx img%x {%s}\n", x, x, fname);
            if(default_pic)
                sys_vgui(".x%lx.c itemconfigure %xS -image img%x\n", glist_getcanvas(x->x_glist), x, x);
        }
    }
    else
        pd_error(x, "[pic]: error opening file '%s'", file->s_name);
}

static void pic_setwidget(void){
    pic_widgetbehavior.w_getrectfn =  pic_getrect;
    pic_widgetbehavior.w_displacefn = pic_displace;
    pic_widgetbehavior.w_selectfn =   pic_select;
    pic_widgetbehavior.w_deletefn =   pic_delete;
    pic_widgetbehavior.w_visfn =      pic_vis;
    pic_widgetbehavior.w_clickfn = NULL;
}

static void *pic_new(t_symbol *name){
    t_pic *x = (t_pic *)pd_new(pic_class);
    x->x_glist = (t_glist *)canvas_getcurrent();
    x->x_width = x->x_height = 15;
    x->x_def_img = 0;
    x->x_arg = x->x_filename = name;
    return(x);
}

void pic_setup(void){
    pic_class = class_new(gensym("pic"), (t_newmethod)pic_new, 0, sizeof(t_pic), 0, A_DEFSYM, 0);
    class_addmethod(pic_class, (t_method)pic_open, gensym("open"), A_SYMBOL, 0);
    pic_setwidget();
    class_setwidget(pic_class, &pic_widgetbehavior);
    class_setsavefn(pic_class, &pic_save);
    sys_vgui("eval [read [open {%s/%s.tcl}]]\n", pic_class->c_externdir->s_name,
             pic_class->c_name->s_name);
}
