// from moonlib

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

//////////////////////////// widget helper functions /////////////////////////////

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

void pic_draw(t_pic *x, t_glist *glist, int displace){
    if(displace){ // move to displace???
        sys_vgui(".x%lx.c coords %xS \
                 %d %d\n", glist_getcanvas(glist), x,
                 text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist));
    }
    else{ // VIS (move to VIS???)
        if(x->x_def_img){ // USING DEFAULT PIC
            sys_vgui(".x%lx.c create image %d %d -tags %xS\n", glist_getcanvas(glist),
                     text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist), x);
            sys_vgui(".x%lx.c itemconfigure %xS -image %s\n",
                     glist_getcanvas(glist), x, x->x_filename->s_name);
        }
        else{ // using file pic
            sys_vgui("image create photo img%x\n", x); // if no local image ?
            const char *fname = pic_get_filename(x, x->x_filename->s_name); // every time?
            sys_vgui("img%x configure -file {%s}\n", x, fname);
            sys_vgui(".x%lx.c create image %d %d -image img%x -tags %xS\n",
                     glist_getcanvas(glist), text_xpix(&x->x_obj, glist),
                     text_ypix(&x->x_obj, glist), x, x);
        }
        // TODO callback from gui sys_vgui("pic_size logo");
    }
}

void pic_erase(t_pic *x,t_glist *glist){ // move to vis?
    sys_vgui(".x%lx.c delete %xS\n", glist_getcanvas(glist), x);
}

// ------------------------ pic widgetbehaviour-----------------------------

static void pic_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_pic *x = (t_pic *)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = text_xpix(&x->x_obj, glist) + x->x_width;
    *yp2 = text_ypix(&x->x_obj, glist) + x->x_height;
}

static void pic_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_pic *x = (t_pic *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    sys_vgui(".x%lx.c coords %xSEL %d %d %d %d\n", glist_getcanvas(glist), x, text_xpix(&x->x_obj,
        glist), text_ypix(&x->x_obj, glist), text_xpix(&x->x_obj, glist) + x->x_width,
        text_ypix(&x->x_obj, glist) + x->x_height);
    pic_draw(x, glist, 1);
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
    vis ? pic_draw(s, glist, 0) : pic_erase(s, glist);
}

static void pic_save(t_gobj *z, t_binbuf *b){
    t_pic *x = (t_pic *)z;
    if(x->x_filename == gensym("else_pic_def_img"))
        x->x_filename = x->x_arg != &s_ ? x->x_arg : &s_;
    binbuf_addv(b, "ssiiss", gensym("#X"), gensym("obj"), x->x_obj.te_xpix, x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)), x->x_filename);
    binbuf_addv(b, ";");
}

void pic_open(t_gobj *z,t_symbol *file){
    t_pic *x = (t_pic *)z;
    const char *fname = pic_get_filename(x, file->s_name);
    if(fname){
        x->x_filename = file;
        if(glist_isvisible(x->x_glist)){
            sys_vgui("image create photo img%x\n", x); // if no local image ?
            sys_vgui("img%x blank\n", x);
            sys_vgui("img%x configure -file {%s}\n", x, fname);
            if(x->x_def_img)
                sys_vgui(".x%lx.c itemconfigure %xS -image img%x\n", glist_getcanvas(x->x_glist), x, x);
        }
        x->x_def_img = 0;
    }
    else
        pd_error(x, "[pic]: error opening file '%s'", file->s_name);
}

t_widgetbehavior pic_widgetbehavior;

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
    if(x->x_filename == &s_){
        x->x_filename = gensym("else_pic_def_img");
        x->x_def_img = 1;
    }
    else if(!pic_get_filename(x, x->x_filename->s_name)){
        pd_error(x, "[pic]: error opening file '%s'", x->x_filename->s_name);
        x->x_filename = gensym("else_pic_def_img");
        x->x_def_img = 1;
    }
    return(x);
}

void pic_setup(void){
    pic_class = class_new(gensym("pic"), (t_newmethod)pic_new, 0, sizeof(t_pic), 0, A_DEFSYM, 0);
    class_addmethod(pic_class, (t_method)pic_open, gensym("open"), A_SYMBOL, 0);
    pic_setwidget();
    class_setwidget(pic_class, &pic_widgetbehavior);
    class_setsavefn(pic_class, &pic_save);
// default image (?)
    sys_vgui("image create photo else_pic_def_img -data {R0lGODdhJwAnAMQAAAAAAAsLCxMTExwcHCIiIisrKzw8PERERExMTFRUVF1dXWVlZW1tbXNzc3t7e4ODg42NjZOTk5qamqSkpK2trbS0tLy8vMPDw8zMzNTU1Nzc3OPj4+3t7fT09P///wAAACH5BAkKAB8ALAAAAAAnACcAAAX/oCeOJKlRTzIARwNVWinPNNYAeK4HjNXRQNLGkRsIArnAYIVbZILAjAFAYAICgmxyQMBZoDJNt1vUGc0CAAY86iioSdwgkTjIzQAEh+2hwHFpAhQbHIUZRGk5XRRsbldxazIQAFZpCz9QGjqUABM0HHZIjwMxUBgAiUh6QJOVAE9QGVRGBQCMQBJ/qGpgHUQ5l0GtOWmwUBwTCwoRe0AbZDhIBRt8Hh2YQURWKw/VfMOKvN5BHA+cObUR40EZCOc4tQzY6yUXONACXQzN9CUWV9twRJjXT4S9M/e8FJTBoVYiTgxKLSRRQdcKCAQnejDHZIUDjTNuJFphDOSICAAKUQiIZzLMpgstZWR4sKABzJgzMuLcyXMdBwsTKEjcqcFdjls4HRXgsuJmTHu1EjbYOeFdmgT8TFaEtiJYzA13UnbiWfERgAY6QdoQgGBCWiAhAAA7\n");
    sys_vgui("}\n");
}
