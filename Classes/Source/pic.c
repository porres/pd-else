
#include <m_pd.h>
#include <g_canvas.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

static t_class *pic_class;

t_widgetbehavior pic_widgetbehavior;

typedef struct _pic{
     t_object   x_obj;
     t_glist   *x_glist;
     int        x_width;
     int        x_height;
     int        x_snd_set;
     int        x_rcv_set;
     int        x_bound;
     t_symbol  *x_fullname;
     t_symbol  *x_filename;
     t_symbol  *x_x;
     t_symbol  *x_receive;
     t_symbol  *x_rcv_raw;
     t_symbol  *x_send;
     t_symbol  *x_snd_raw;
     t_outlet  *x_outlet;
}t_pic;

// widget helper functions
static const char* pic_get_filename(t_pic *x, const char *file){
    static char fname[MAXPDSTRING];
    char *bufptr;
    int fd;
    fd = open_via_path(canvas_getdir(glist_getcanvas(x->x_glist))->s_name,
        file, "", fname, &bufptr, MAXPDSTRING, 1);
    if(fd > 0){
        fname[strlen(fname)]='/';
        close(fd);
        return(fname);
    }
    else
        return(0);
}

static int pic_click(t_pic *x, struct _glist *glist, int xpos, int ypos, int shift, int alt, int dbl, int doit){
    glist = NULL, xpos = ypos = shift = alt = dbl = 0;
    if(doit){
        outlet_bang(x->x_outlet);
        if(x->x_send != &s_ && x->x_send->s_thing)
            pd_bang(x->x_send->s_thing);
    }
    return(1); // avoid warning
}

void pic_erase(t_pic* x,t_glist* glist){
     sys_vgui(".x%lx.c delete %lximage\n", glist_getcanvas(glist), x);
}

// ------------------------ image widgetbehaviour-------------------------------------------
static void pic_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_pic* x = (t_pic*)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = text_xpix(&x->x_obj, glist) + x->x_width;
    *yp2 = text_ypix(&x->x_obj, glist) + x->x_height;
}

static void pic_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_pic *x = (t_pic *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    sys_vgui(".x%lx.c coords %lxSEL %d %d %d %d\n", glist_getcanvas(glist), x, text_xpix(&x->x_obj, glist),
        text_ypix(&x->x_obj, glist), text_xpix(&x->x_obj, glist) + x->x_width, text_ypix(&x->x_obj, glist) + x->x_height);
    sys_vgui(".x%lx.c coords %lximage %d %d\n", glist_getcanvas(glist), x,
           text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist));
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void pic_select(t_gobj *z, t_glist *glist, int state){
    t_pic *x = (t_pic *)z;
    if(state){
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline blue\n",
            glist_getcanvas(glist), text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist),
            text_xpix(&x->x_obj, glist) + x->x_width, text_ypix(&x->x_obj, glist) + x->x_height, x);
     }
     else
         sys_vgui(".x%lx.c delete %lxSEL\n", glist_getcanvas(glist), x);
}

static void pic_delete(t_gobj *z, t_glist *glist){
    t_text *x = (t_text *)z;
    canvas_deletelinesfor(glist, x);
}
       
static void pic_vis(t_gobj *z, t_glist *glist, int vis){
    t_pic* x = (t_pic*)z;
    if(vis)
        sys_vgui(".x%lx.c create image %d %d -anchor nw -image %lx_pic -tags %lximage\n",
                 glist_getcanvas(glist), text_xpix(&x->x_obj, glist), text_ypix(&x->x_obj, glist), x->x_fullname, x);
    else
        pic_erase(x, glist);
}

static void pic_save(t_gobj *z, t_binbuf *b){
    t_pic *x = (t_pic *)z;
    t_binbuf *bb = x->x_obj.te_binbuf;
    int n_args = binbuf_getnatom(bb); // number of arguments
    char buf[80];
    if(n_args > 0){
        if(n_args > 2 && !x->x_snd_set){
            atom_string(binbuf_getvec(bb) + 2, buf, 80);
            x->x_snd_raw = gensym(buf);
        }
        if(n_args > 3 && !x->x_rcv_set){
            atom_string(binbuf_getvec(bb) + 3, buf, 80);
            x->x_rcv_raw = gensym(buf);
        }
    }
    if(x->x_filename == &s_)
        x->x_filename = gensym("empty");
    if(x->x_snd_raw == &s_)
        x->x_snd_raw = gensym("empty");
    if(x->x_rcv_raw == &s_)
        x->x_rcv_raw = gensym("empty");
    binbuf_addv(b, "ssiissss", gensym("#X"), gensym("obj"), x->x_obj.te_xpix, x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)), x->x_filename, x->x_snd_raw, x->x_rcv_raw);
    binbuf_addv(b, ";");
}

static void pic_imagesize_callback(t_pic *x, t_float w, t_float h){
    x->x_width = w;
    x->x_height = h;
    if(glist_isvisible(x->x_glist))
        canvas_fixlinesfor(x->x_glist, (t_text*)x);
    pd_unbind(&x->x_obj.ob_pd, x->x_x);
    if(x->x_receive != &s_){
        pd_bind(&x->x_obj.ob_pd, x->x_receive);
        x->x_bound = 1;
    }
}

void pic_open(t_pic* x, t_symbol *filename){
    if(filename){
        if(filename != x->x_filename){
            const char *file_name_open = pic_get_filename(x, filename->s_name); // path
            if(file_name_open){
                canvas_dirty(x->x_glist, 1);
                x->x_filename = filename;
                x->x_fullname = gensym(file_name_open);
                sys_vgui("if { [info exists %lx_pic] == 0 } { image create photo %lx_pic -file \"%s\"\n set %lx_pic 1\n} \n",
                    x->x_fullname, x->x_fullname, file_name_open, x->x_fullname);
                if(x->x_receive != &s_)
                    pd_unbind(&x->x_obj.ob_pd, x->x_receive);
                pd_bind(&x->x_obj.ob_pd, x->x_x);
                if(glist_isvisible(x->x_glist)){
                    pic_erase(x, x->x_glist);
                    sys_vgui(".x%lx.c create image %d %d -anchor nw -image %lx_pic -tags %lximage\n",
                        glist_getcanvas(x->x_glist), text_xpix(&x->x_obj, x->x_glist), text_ypix(&x->x_obj,
                        x->x_glist), x->x_fullname, x);
                }
                sys_vgui("pdsend \"%s _imagesize [image width %lx_pic] [image height %lx_pic]\"\n",
                         x->x_x->s_name, x->x_fullname, x->x_fullname);
            }
            else
                pd_error(x, "[pic]: error opening file '%s'", filename->s_name);
        }
    }
    else
        pd_error(x, "[pic]: open needs a file name");
}

static void pic_send(t_pic *x, t_symbol *s){
    if(s != gensym("")){
        x->x_snd_raw = s;
        x->x_snd_set = 1;
        t_symbol *snd = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, x->x_snd_raw);
        if(snd != x->x_send){
            canvas_dirty(x->x_glist, 1);
            x->x_send = snd;
        }
    }
}

static void pic_receive(t_pic *x, t_symbol *s){
    if(s != gensym("")){
        x->x_rcv_raw = s;
        x->x_rcv_set = 1;
        t_symbol *rcv = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, x->x_rcv_raw);
        if(rcv == &s_){
            if(rcv != x->x_receive){
                canvas_dirty(x->x_glist, 1);
                if(x->x_bound){
                    pd_unbind(&x->x_obj.ob_pd, x->x_receive);
                    x->x_bound = 0;
                }
                x->x_receive = rcv;
            }
        }
        else if(rcv != x->x_receive){
            if(x->x_bound)
                pd_unbind(&x->x_obj.ob_pd, x->x_receive);
            pd_bind(&x->x_obj.ob_pd, x->x_receive = rcv);
            x->x_bound = 1;
        }
    }
}

static void pic_bang(t_pic *x){
    outlet_bang(x->x_outlet);
    if(x->x_send != &s_ && x->x_send->s_thing)
        pd_bang(x->x_send->s_thing);
}

static void pic_free(t_pic *x){ // if variable is unset and image is unused then delete them
    sys_vgui("if { [info exists %lx_pic] == 1 && [image inuse %lx_pic] == 0} { image delete %lx_pic \n unset %lx_pic\n} \n",
        x->x_fullname, x->x_fullname, x->x_fullname, x->x_fullname);
    if(x->x_receive != &s_)
        pd_unbind(&x->x_obj.ob_pd, x->x_receive);
}

static void *pic_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pic *x = (t_pic *)pd_new(pic_class);
    char buf[MAXPDSTRING];
    sprintf(buf, "#%lx", (long)x);
    x->x_x = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_x);
    x->x_glist = (t_glist*)canvas_getcurrent();
    x->x_send = x->x_snd_raw = x->x_receive = x->x_rcv_raw = x->x_filename = &s_;
    int loaded = x->x_rcv_set = x->x_snd_set = x->x_bound = 0;
    x->x_fullname = NULL;
    if(ac && (av)->a_type == A_SYMBOL)
        x->x_filename = atom_getsymbol(av);
    if((av+1)->a_type == A_SYMBOL){
        t_symbol *snd = atom_getsymbol(av+1);
        if(snd != gensym("empty"))
            x->x_send = snd;
    }
    if((av+2)->a_type == A_SYMBOL){
        t_symbol *rcv = atom_getsymbol(av+2);
        if(rcv != gensym("empty"))
            x->x_receive = rcv;
    }
    if(x->x_filename !=  &s_ && x->x_filename !=  gensym("empty")){
        const char *fname = pic_get_filename(x, x->x_filename->s_name); // full path
        if(fname){
            loaded = 1;
            x->x_fullname = gensym(fname);
            sys_vgui("if { [info exists %lx_pic] == 0 } { image create photo %lx_pic -file \"%s\"\n set %lx_pic 1\n} \n",
                    x->x_fullname, x->x_fullname, fname, x->x_fullname);
            sys_vgui("pdsend \"%s _imagesize [image width %lx_pic] [image height %lx_pic]\"\n",
                     x->x_x->s_name, x->x_fullname, x->x_fullname);
        }
        else
            pd_error(x, "[pic]: error opening file '%s'", x->x_filename->s_name);
    }
    if(!loaded){ // default image
        x->x_width = 28;
        x->x_height = 33;
        const char *fname = pic_get_filename(x, "question.gif"); 
        x->x_fullname = gensym(fname);
        sys_vgui("if { [info exists %lx_pic] == 0 } { image create photo %lx_pic -file \"%s\"\n set %lx_pic 1\n} \n", x->x_fullname, x->x_fullname, fname, x->x_fullname);
        sys_vgui("pdsend \"%s _imagesize [image width %lx_pic] [image height %lx_pic]\"\n",
                 x->x_x->s_name, x->x_fullname, x->x_fullname);
    }
    x->x_outlet = outlet_new(&x->x_obj, &s_bang);
    return(x);
}

void pic_setup(void){
    pic_class = class_new(gensym("pic"), (t_newmethod)pic_new, (t_method)pic_free, sizeof(t_pic),0, A_GIMME,0);
    class_addbang(pic_class, pic_bang);
    class_addmethod(pic_class, (t_method)pic_imagesize_callback, gensym("_imagesize"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(pic_class, (t_method)pic_open, gensym("open"), A_SYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_send, gensym("send"), A_DEFSYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_receive, gensym("receive"), A_DEFSYMBOL, 0);
    pic_widgetbehavior.w_getrectfn =  pic_getrect;
    pic_widgetbehavior.w_displacefn = pic_displace;
    pic_widgetbehavior.w_selectfn =   pic_select;
    pic_widgetbehavior.w_deletefn =   pic_delete;
    pic_widgetbehavior.w_visfn =      pic_vis;
    pic_widgetbehavior.w_clickfn =    (t_clickfn)pic_click;
    class_setwidget(pic_class, &pic_widgetbehavior);
    class_setsavefn(pic_class, &pic_save);
/*    sys_vgui("image create photo pic_def_img -data {R0lGODdhJwAnAMQAAAAAAAsLCxMTExwcHCIiIisrKzw8PERERExMTFRUVF1dXWVlZW1tbXNzc3t7e4ODg42NjZOTk5qamqSkpK2trbS0tLy8vMPDw8zMzNTU1Nzc3OPj4+3t7fT09P///wAAACH5BAkKAB8ALAAAAAAnACcAAAX/oCeOJKlRTzIARwNVWinPNNYAeK4HjNXRQNLGkRsIArnAYIVbZILAjAFAYAICgmxyQMBZoDJNt1vUGc0CAAY86iioSdwgkTjIzQAEh+2hwHFpAhQbHIUZRGk5XRRsbldxazIQAFZpCz9QGjqUABM0HHZIjwMxUBgAiUh6QJOVAE9QGVRGBQCMQBJ/qGpgHUQ5l0GtOWmwUBwTCwoRe0AbZDhIBRt8Hh2YQURWKw/VfMOKvN5BHA+cObUR40EZCOc4tQzY6yUXONACXQzN9CUWV9twRJjXT4S9M/e8FJTBoVYiTgxKLSRRQdcKCAQnejDHZIUDjTNuJFphDOSICAAKUQiIZzLMpgstZWR4sKABzJgzMuLcyXMdBwsTKEjcqcFdjls4HRXgsuJmTHu1EjbYOeFdmgT8TFaEtiJYzA13UnbiWfERgAY6QdoQgGBCWiAhAAA7\n");
    sys_vgui("}\n");*/
}
