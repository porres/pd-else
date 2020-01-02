// porres 2018-2019

#include <m_pd.h>
#include <g_canvas.h>
#include <unistd.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

#define IOHEIGHT 2

static t_class *pic_class, *edit_proxy_class;
t_widgetbehavior pic_widgetbehavior;

typedef struct _edit_proxy{
    t_object    p_obj;
    t_symbol   *p_sym;
    t_clock    *p_clock;
    struct      _pic *p_cnv;
}t_edit_proxy;

typedef struct _pic{
     t_object       x_obj;
     t_glist       *x_glist;
     t_edit_proxy  *x_proxy;
     t_canvas      *x_canvas;
     int            x_width;
     int            x_height;
     int            x_snd_set;
     int            x_rcv_set;
     int            x_bound;
     int            x_bound_to_x;
     int            x_edit;
     int            x_init;
     int            x_def_img;
     int            x_sel;
     int            x_outline;
     t_symbol      *x_fullname;
     t_symbol      *x_filename;
     t_symbol      *x_x;
     t_symbol      *x_receive;
     t_symbol      *x_rcv_raw;
     t_symbol      *x_send;
     t_symbol      *x_snd_raw;
     t_outlet      *x_outlet;
}t_pic;

// helper functions
static const char* pic_filepath(t_pic *x, const char *file){
    static char fname[MAXPDSTRING];
    char *bufptr;
    int fd = open_via_path(canvas_getdir(glist_getcanvas(x->x_glist))->s_name,
        file, "", fname, &bufptr, MAXPDSTRING, 1);
    if(fd > 0){
        fname[strlen(fname)]='/';
        close(fd);
        return(fname);
    }
    else return(0);
}

static void pic_size_callback(t_pic *x, t_float w, t_float h){
    x->x_width = w;
    x->x_height = h;
    if(glist_isvisible(x->x_glist)){
        canvas_fixlinesfor(x->x_glist, (t_text*)x);
        if(x->x_edit || x->x_outline){
            t_canvas *cv = glist_getcanvas(x->x_glist);
            int xpos = text_xpix(&x->x_obj, x->x_glist);
            int ypos = text_ypix(&x->x_obj, x->x_glist);
            sys_vgui(".x%lx.c delete %lxSEL\n", cv, x);
            sys_vgui(".x%lx.c delete %lx_in\n", cv, x);
            sys_vgui(".x%lx.c delete %lx_out\n", cv, x);
            if(x->x_sel) sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline blue\n",
                cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x);
            else sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black\n",
                cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x);
            if(x->x_edit && x->x_receive == &s_)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lx_in\n",
                    cv, xpos, ypos, xpos+IOWIDTH, ypos+IOHEIGHT, x);
            if(x->x_edit && x->x_send == &s_){
                ypos += x->x_height;
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lx_out\n",
                    cv, xpos, ypos, xpos+IOWIDTH, ypos-IOHEIGHT, x);
            }
        }
    }
    pd_unbind(&x->x_obj.ob_pd, x->x_x);
    x->x_bound_to_x = 0;
    if(x->x_receive != &s_){
        pd_bind(&x->x_obj.ob_pd, x->x_receive);
        x->x_bound = 1;
    }
}

// ------------------------ pic widgetbehaviour-------------------------------------------
static int pic_click(t_pic *x, struct _glist *glist, int xpos, int ypos, int shift, int alt, int dbl, int doit){
    glist = NULL, xpos = ypos = shift = alt = dbl = 0;
    if(doit){
        outlet_bang(x->x_outlet);
        if(x->x_send != &s_ && x->x_send->s_thing)
            pd_bang(x->x_send->s_thing);
    }
    return(1);
}

static void pic_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_pic* x = (t_pic*)z;
    int xpos = *xp1 = text_xpix(&x->x_obj, glist);
    int ypos = *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = xpos + x->x_width;
    *yp2 = ypos + x->x_height;
}

static void pic_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_pic *x = (t_pic *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    int xpos = text_xpix(&x->x_obj, glist);
    int ypos = text_ypix(&x->x_obj, glist);
    t_canvas *cv = glist_getcanvas(glist);
    sys_vgui(".x%lx.c coords %lxSEL %d %d %d %d\n", cv, x, xpos, ypos, xpos+x->x_width, ypos+x->x_height);
    sys_vgui(".x%lx.c coords %lx_pic %d %d\n", cv, x, xpos, ypos);
    if(x->x_receive == &s_)
        sys_vgui(".x%lx.c coords %lx_in %d %d %d %d\n", cv, x, xpos, ypos, xpos+IOWIDTH, ypos+IOHEIGHT);
    if(x->x_send == &s_){
        ypos += x->x_height;
        sys_vgui(".x%lx.c coords %lx_out %d %d %d %d\n", cv, x, xpos, ypos, xpos+IOWIDTH, ypos-IOHEIGHT);
    }
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void pic_select(t_gobj *z, t_glist *glist, int state){
    t_pic *x = (t_pic *)z;
    int xpos = text_xpix(&x->x_obj, glist);
    int ypos = text_ypix(&x->x_obj, glist);
    t_canvas *cv = glist_getcanvas(glist);
    x->x_sel = state;
    if(state){
        sys_vgui(".x%lx.c delete %lxSEL\n", cv, x);
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline blue -width 1\n",
            cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x);
     }
    else{
        sys_vgui(".x%lx.c delete %lxSEL\n", cv, x);
        if(x->x_edit || x->x_outline)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black\n",
                cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x);
    }
}

static void pic_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}
       
static void pic_vis(t_gobj *z, t_glist *glist, int vis){
    t_pic* x = (t_pic*)z;
    t_canvas *cv = glist_getcanvas(glist);
    if(vis){
        int xpos = text_xpix(&x->x_obj, glist);
        int ypos = text_ypix(&x->x_obj, glist);
        if(x->x_def_img){ // DEFAULT PIC
            sys_vgui(".x%lx.c create image %d %d -anchor nw -tags %lx_pic\n", cv, xpos, ypos, x);
            sys_vgui(".x%lx.c itemconfigure %lx_pic -image %s\n", cv, x, "pic_def_img");
            if(x->x_edit || x->x_outline)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black\n",
                    cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x);
        }
        else{
            sys_vgui(".x%lx.c create image %d %d -anchor nw -image %lx_picname -tags %lx_pic\n",
                cv, xpos, ypos, x->x_fullname, x);
            if(!x->x_init){
                x->x_init = 1;
                if(x->x_bound){
                    pd_unbind(&x->x_obj.ob_pd, x->x_receive);
                    x->x_bound = 0;
                }
                if(!x->x_bound_to_x){
                    pd_bind(&x->x_obj.ob_pd, x->x_x);
                    x->x_bound_to_x = 1;
                }
                sys_vgui("pdsend \"%s _picsize [image width %lx_picname] [image height %lx_picname]\"\n",
                     x->x_x->s_name, x->x_fullname, x->x_fullname);
            }
            else if(x->x_edit || x->x_outline)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black\n",
                    cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x);
        }
        sys_vgui(".x%lx.c delete %lx_in\n", cv, x);
        sys_vgui(".x%lx.c delete %lx_out\n", cv, x);
        if(x->x_edit && x->x_receive == &s_)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lx_in\n",
                     cv, xpos, ypos, xpos+IOWIDTH, ypos+IOHEIGHT, x);
        if(x->x_edit && x->x_send == &s_){
            ypos += x->x_height;
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lx_out\n",
                cv, xpos, ypos, xpos+IOWIDTH, ypos-IOHEIGHT, x);
        }
    }
    else{
        sys_vgui(".x%lx.c delete %lx_pic\n", cv, x); // ERASE
        sys_vgui(".x%lx.c delete %lx_in\n", cv, x);
        sys_vgui(".x%lx.c delete %lx_out\n", cv, x);
        sys_vgui(".x%lx.c delete %lxSEL\n", cv, x); // if edit?
    }
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
    binbuf_addv(b, "ssiissssi", gensym("#X"), gensym("obj"), x->x_obj.te_xpix, x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)), x->x_filename, x->x_snd_raw, x->x_rcv_raw, x->x_outline);
    binbuf_addv(b, ";");
}

//------------------------------- METHODS --------------------------------------------
void pic_open(t_pic* x, t_symbol *filename){
    if(filename){
        if(filename != x->x_filename){
            const char *file_name_open = pic_filepath(x, filename->s_name); // path
            if(file_name_open){
                t_canvas *cv = glist_getcanvas(x->x_glist);
                int xpos = text_xpix(&x->x_obj, x->x_glist);
                int ypos = text_ypix(&x->x_obj, x->x_glist);
                x->x_filename = filename;
                x->x_fullname = gensym(file_name_open);
                sys_vgui("if { [info exists %lx_picname] == 0 } { image create photo %lx_picname -file \"%s\"\n set %lx_picname 1\n} \n",
                x->x_fullname, x->x_fullname, file_name_open, x->x_fullname);
                if(glist_isvisible(x->x_glist)){
                    sys_vgui(".x%lx.c delete %lx_pic\n", cv, x); // ERASE
                    sys_vgui(".x%lx.c delete %lx_in\n", cv, x);
                    sys_vgui(".x%lx.c delete %lx_out\n", cv, x);
                    sys_vgui(".x%lx.c delete %lxSEL\n", cv, x);
                    sys_vgui(".x%lx.c create image %d %d -anchor nw -image %lx_picname -tags %lx_pic\n",
                        cv, xpos, ypos, x->x_fullname, x); // CREATE NEW IMAGE
                }
                if(x->x_bound){
                    pd_unbind(&x->x_obj.ob_pd, x->x_receive);
                    x->x_bound = 0;
                }
                if(!x->x_bound_to_x){
                    pd_bind(&x->x_obj.ob_pd, x->x_x);
                    x->x_bound_to_x = 1;
                }
                sys_vgui("pdsend \"%s _picsize [image width %lx_picname] [image height %lx_picname]\"\n",
                         x->x_x->s_name, x->x_fullname, x->x_fullname);
                 if(x->x_def_img)
                     x->x_def_img = 0;
                canvas_dirty(x->x_glist, 1);
            }
            else pd_error(x, "[pic]: error opening file '%s'", filename->s_name);
        }
    }
    else pd_error(x, "[pic]: open needs a file name");
}

static void pic_send(t_pic *x, t_symbol *s){
    if(s != gensym("")){
        t_symbol *snd = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, x->x_snd_raw = s);
        if(snd != x->x_send){
            x->x_snd_set = 1;
            canvas_dirty(x->x_glist, 1);
            x->x_send = snd;
            t_canvas *cv = glist_getcanvas(x->x_glist);
            sys_vgui(".x%lx.c delete %lx_out\n", cv, x);
            if(x->x_send == &s_){
                int xpos = text_xpix(&x->x_obj, x->x_glist);
                int ypos = text_ypix(&x->x_obj, x->x_glist) + x->x_height;
                if(x->x_edit)
                    sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lx_out\n",
                             cv, xpos, ypos, xpos+IOWIDTH, ypos-IOHEIGHT, x);
            }
        }
    }
}

static void pic_receive(t_pic *x, t_symbol *s){
    if(s != gensym("")){
        x->x_rcv_set = 1;
        t_symbol *rcv = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, x->x_rcv_raw = s);
        if(rcv != x->x_receive){
            x->x_rcv_set = 1;
            canvas_dirty(x->x_glist, 1);
            t_canvas *cv = glist_getcanvas(x->x_glist);
            int xpos = text_xpix(&x->x_obj, x->x_glist);
            int ypos = text_ypix(&x->x_obj, x->x_glist);
            if(x->x_bound)
                pd_unbind(&x->x_obj.ob_pd, x->x_receive);
            x->x_receive = rcv;
            sys_vgui(".x%lx.c delete %lx_in\n", cv, x);
            if(rcv == &s_){
                x->x_bound = 0;
                if(x->x_edit)
                    sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lx_in\n",
                        cv, xpos, ypos, xpos+IOWIDTH, ypos+IOHEIGHT, x);
            }
            else{
                pd_bind(&x->x_obj.ob_pd, x->x_receive);
                x->x_bound = 1;
            }
        }
    }
}

static void pic_outline(t_pic *x, t_float f){
    int outline = (int)(f !=0);
    if(x->x_outline != outline){
        x->x_outline = outline;
        canvas_dirty(x->x_glist, 1);
        t_canvas *cv = glist_getcanvas(x->x_glist);
        if(x->x_outline){
            int xpos = text_xpix(&x->x_obj, x->x_glist);
            int ypos = text_ypix(&x->x_obj, x->x_glist);
            sys_vgui(".x%lx.c delete %lxSEL\n", cv, x);
            if(x->x_sel) sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline blue\n",
                cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x);
            else sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black\n",
                cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x);
        }
        else if(!x->x_edit)
            sys_vgui(".x%lx.c delete %lxSEL\n", cv, x);
    }
}

static void pic_bang(t_pic *x){
    outlet_bang(x->x_outlet);
    if(x->x_send != &s_ && x->x_send->s_thing)
        pd_bang(x->x_send->s_thing);
}

static void edit_proxy_any(t_edit_proxy *p, t_symbol *s, int ac, t_atom *av){
    ac = 0;
    if(p->p_cnv && s == gensym("editmode")){//} && !p->p_cnv->x_outline){
        t_canvas *cv = glist_getcanvas(p->p_cnv->x_glist);
        if((p->p_cnv->x_edit = (int)(av->a_w.w_float))){
            int x = text_xpix(&p->p_cnv->x_obj, p->p_cnv->x_glist);
            int y = text_ypix(&p->p_cnv->x_obj, p->p_cnv->x_glist);
            int width = p->p_cnv->x_width;
            int height = p->p_cnv->x_height;
            sys_vgui(".x%lx.c delete %lx_in\n", cv, x);
            sys_vgui(".x%lx.c delete %lx_out\n", cv, x);
            if(!p->p_cnv->x_outline)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black\n",
                         cv, x, y, x+width, y+height, p->p_cnv);
            if(p->p_cnv->x_receive == &s_)
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lx_in\n",
                    cv, x, y, x+IOWIDTH, y+IOHEIGHT, p->p_cnv);
            if(p->p_cnv->x_send == &s_){
                y += height;
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lx_out\n",
                    cv, x, y, x+IOWIDTH, y-IOHEIGHT, p->p_cnv);
            }
        }
        else{
            if(!p->p_cnv->x_outline)
                sys_vgui(".x%lx.c delete %lxSEL\n", cv, p->p_cnv);
            sys_vgui(".x%lx.c delete %lx_in\n", cv, p->p_cnv);
            sys_vgui(".x%lx.c delete %lx_out\n", cv, p->p_cnv);
        }
    }
}

//-------------------------------------------------------------------------------------
static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy * edit_proxy_new(t_pic *x, t_symbol *s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

static void pic_free(t_pic *x){ // delete if variable is unset and image is unused
    sys_vgui("if { [info exists %lx_picname] == 1 && [image inuse %lx_picname] == 0} { image delete %lx_picname \n unset %lx_picname\n} \n",
        x->x_fullname, x->x_fullname, x->x_fullname, x->x_fullname);
    if(x->x_receive != &s_)
        pd_unbind(&x->x_obj.ob_pd, x->x_receive);
    x->x_proxy->p_cnv = NULL;
    clock_delay(x->x_proxy->p_clock, 0);
}

static void *pic_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pic *x = (t_pic *)pd_new(pic_class);
    x->x_canvas = canvas_getcurrent();
    x->x_glist = (t_glist*)x->x_canvas;
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_canvas);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    sprintf(buf, "#%lx", (long)x);
    x->x_x = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_x);
    x->x_bound_to_x = 1;
    x->x_edit = x->x_canvas->gl_edit;
    x->x_send = x->x_snd_raw = x->x_receive = x->x_rcv_raw = x->x_filename = &s_;
    int loaded = x->x_rcv_set = x->x_snd_set = x->x_bound = x->x_def_img = x->x_init = x->x_outline = 0;
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
    if((av+3)->a_type == A_FLOAT)
        x->x_outline = (int)(atom_getfloat(av+3) != 0);
    if(x->x_filename !=  &s_ && x->x_filename !=  gensym("empty")){
        const char *fname = pic_filepath(x, x->x_filename->s_name); // full path
        if(fname){
            loaded = 1;
            x->x_fullname = gensym(fname);
            sys_vgui("if { [info exists %lx_picname] == 0 } { image create photo %lx_picname -file \"%s\"\n set %lx_picname 1\n} \n",
                    x->x_fullname, x->x_fullname, fname, x->x_fullname);
            if(x->x_receive != &s_){
                pd_bind(&x->x_obj.ob_pd, x->x_receive);
                x->x_bound = 1;
            }
        }
        else
            pd_error(x, "[pic]: error opening file '%s'", x->x_filename->s_name);
    }
    if(!loaded){ // default image
        x->x_width = x->x_height = 38;
        x->x_def_img = 1;
        pd_unbind(&x->x_obj.ob_pd, x->x_x);
        x->x_bound_to_x = 0;
        if(x->x_receive != &s_){
            pd_bind(&x->x_obj.ob_pd, x->x_receive);
            x->x_bound = 1;
        }
    }
    x->x_outlet = outlet_new(&x->x_obj, &s_bang);
    return(x);
}

void pic_setup(void){
    pic_class = class_new(gensym("pic"), (t_newmethod)pic_new, (t_method)pic_free, sizeof(t_pic),0, A_GIMME,0);
    class_addbang(pic_class, pic_bang);
    class_addmethod(pic_class, (t_method)pic_size_callback, gensym("_picsize"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(pic_class, (t_method)pic_outline, gensym("outline"), A_DEFFLOAT, 0);
    class_addmethod(pic_class, (t_method)pic_open, gensym("open"), A_DEFSYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_send, gensym("send"), A_DEFSYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_receive, gensym("receive"), A_DEFSYMBOL, 0);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
    pic_widgetbehavior.w_getrectfn  = pic_getrect;
    pic_widgetbehavior.w_displacefn = pic_displace;
    pic_widgetbehavior.w_selectfn   = pic_select;
    pic_widgetbehavior.w_deletefn   = pic_delete;
    pic_widgetbehavior.w_visfn      = pic_vis;
    pic_widgetbehavior.w_clickfn    = (t_clickfn)pic_click;
    class_setwidget(pic_class, &pic_widgetbehavior);
    class_setsavefn(pic_class, &pic_save);
    sys_vgui("image create photo pic_def_img -data {R0lGODdhJwAnAMQAAAAAAAsLCxMTExwcHCIiIisrKzw8PERERExMTFRUVF1dXWVlZW1tbXNzc3t7e4ODg42NjZOTk5qamqSkpK2trbS0tLy8vMPDw8zMzNTU1Nzc3OPj4+3t7fT09P///wAAACH5BAkKAB8ALAAAAAAnACcAAAX/oCeOJKlRTzIARwNVWinPNNYAeK4HjNXRQNLGkRsIArnAYIVbZILAjAFAYAICgmxyQMBZoDJNt1vUGc0CAAY86iioSdwgkTjIzQAEh+2hwHFpAhQbHIUZRGk5XRRsbldxazIQAFZpCz9QGjqUABM0HHZIjwMxUBgAiUh6QJOVAE9QGVRGBQCMQBJ/qGpgHUQ5l0GtOWmwUBwTCwoRe0AbZDhIBRt8Hh2YQURWKw/VfMOKvN5BHA+cObUR40EZCOc4tQzY6yUXONACXQzN9CUWV9twRJjXT4S9M/e8FJTBoVYiTgxKLSRRQdcKCAQnejDHZIUDjTNuJFphDOSICAAKUQiIZzLMpgstZWR4sKABzJgzMuLcyXMdBwsTKEjcqcFdjls4HRXgsuJmTHu1EjbYOeFdmgT8TFaEtiJYzA13UnbiWfERgAY6QdoQgGBCWiAhAAA7\n");
    sys_vgui("}\n");
}
