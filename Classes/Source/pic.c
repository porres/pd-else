// porres 2018-2019

#include <m_pd.h>
#include <g_canvas.h>
#include <unistd.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

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
     t_clock       *x_clock;
     int            x_zoom;
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
     int            x_flag;
     int            x_s_flag;
     int            x_r_flag;
     t_symbol      *x_fullname;
     t_symbol      *x_filename;
     t_symbol      *x_x;
     t_symbol      *x_receive;
     t_symbol      *x_rcv_raw;
     t_symbol      *x_send;
     t_symbol      *x_snd_raw;
     t_outlet      *x_outlet;
}t_pic;


// ------------------------ draw inlet -------------------------------------------------------
static void pic_draw_inlet(t_pic *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
    sys_vgui(".x%lx.c delete %lx_in\n", cv, x);
    if(x->x_edit && x->x_receive == &s_)
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lx_in\n",
            cv, xpos, ypos, xpos+(IOWIDTH*x->x_zoom), ypos+(IHEIGHT*x->x_zoom), x);
    sys_vgui(".x%lx.c delete %lx_out\n", cv, x);
    if(x->x_edit && x->x_send == &s_)
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags %lx_out\n",
            cv, xpos, ypos+x->x_height, xpos+IOWIDTH*x->x_zoom, ypos+x->x_height-IHEIGHT*x->x_zoom, x);
}

// --------------------------------------------------------------------------------------------

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
            int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
            sys_vgui(".x%lx.c delete %lxSEL\n", cv, x);
            if(x->x_sel) sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline blue -width %d\n",
                cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x, x->x_zoom);
            else sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black -width %d\n",
                cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x, x->x_zoom);
            pic_draw_inlet(x);
        }
    }
    pd_unbind(&x->x_obj.ob_pd, x->x_x);
    x->x_bound_to_x = 0;
    if(x->x_receive != &s_){
        pd_bind(&x->x_obj.ob_pd, x->x_receive);
        x->x_bound = 1;
    }
/*    if(1){
        t_atom at[2];
        SETFLOAT(at, w);
        SETFLOAT(at+1, h);
        outlet_list(x->x_obj.ob_outlet, &s_list, 2, at);
    }*/
}

// ------------------------ pic widgetbehaviour-------------------------------------------------------------------
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
    t_pic *obj = (t_pic *)z;
    obj->x_obj.te_xpix += dx;
    obj->x_obj.te_ypix += dy;
    t_canvas *cv = glist_getcanvas(glist);
    int x = text_xpix(&obj->x_obj, glist), y = text_ypix(&obj->x_obj, glist), h = obj->x_height, w = obj->x_width;
    sys_vgui(".x%lx.c coords %lxSEL %d %d %d %d\n", cv, obj, x, y, x+w, y+h);
    sys_vgui(".x%lx.c coords %lx_pic %d %d\n", cv, obj, x, y);
    if(obj->x_receive == &s_)
        sys_vgui(".x%lx.c move %lx_in %d %d\n", cv, obj, dx*obj->x_zoom, dy*obj->x_zoom);
    if(obj->x_send == &s_)
        sys_vgui(".x%lx.c move %lx_out %d %d\n", cv, obj, dx*obj->x_zoom, dy*obj->x_zoom);
    canvas_fixlinesfor(glist, (t_text*)obj);
}

static void pic_select(t_gobj *z, t_glist *glist, int state){
    t_pic *x = (t_pic *)z;
    int xpos = text_xpix(&x->x_obj, glist);
    int ypos = text_ypix(&x->x_obj, glist);
    t_canvas *cv = glist_getcanvas(glist);
    x->x_sel = state;
    if(state){
        sys_vgui(".x%lx.c delete %lxSEL\n", cv, x);
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline blue -width %d\n",
            cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x, x->x_zoom);
     }
    else{
        sys_vgui(".x%lx.c delete %lxSEL\n", cv, x);
        if(x->x_edit || x->x_outline)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black -width %d\n",
                cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x, x->x_zoom);
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
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black -width %d\n",
                    cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x, x->x_zoom);
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
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black -width %d\n",
                    cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x, x->x_zoom);
        }
        pic_draw_inlet(x);
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
    int n_args = binbuf_getnatom(bb), i = 0; // number of arguments
    char buf[80];
    if(!x->x_snd_set){ // no send set, search arguments/flags
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // we got flags
                if(x->x_s_flag){ // we got a search flag, let's get it
                    for(i = 0;  i < n_args; i++){
                        atom_string(binbuf_getvec(bb) + i, buf, 80);
                        if(!strcmp(buf, "-send")){
                            i++;
                            atom_string(binbuf_getvec(bb) + i, buf, 80);
                            x->x_snd_raw = gensym(buf);
                            break;
                        }
                    }
                }
            }
            else{ // we got no flags, let's search for argument
                int arg_n = 3; // receive argument number
                if(n_args >= arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 80);
                    x->x_snd_raw = gensym(buf);
                }
            }
        }
    }
    if(!x->x_rcv_set){ // no receive set, search arguments
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // search for receive name in flags
                if(x->x_r_flag){ // we got a receive flag, let's get it
                    for(i = 0;  i < n_args; i++){
                        atom_string(binbuf_getvec(bb) + i, buf, 80);
                        if(!strcmp(buf, "-receive")){
                            i++;
                            atom_string(binbuf_getvec(bb) + i, buf, 80);
                            x->x_rcv_raw = gensym(buf);
                            break;
                        }
                    }
                }
            }
            else{ // we got no flags, let's search for argument
                int arg_n = 4; // receive argument number
                if(n_args >= arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 80);
                    x->x_rcv_raw = gensym(buf);
                }
            }
        }
    }
    if(x->x_filename == &s_)
        x->x_filename = gensym("empty");
    if(x->x_snd_raw == &s_)
        x->x_snd_raw = gensym("empty");
    if(x->x_rcv_raw == &s_)
        x->x_rcv_raw = gensym("empty");
    binbuf_addv(b, "ssiisisss", gensym("#X"), gensym("obj"), x->x_obj.te_xpix, x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)), x->x_outline, x->x_filename, x->x_snd_raw, x->x_rcv_raw);
    binbuf_addv(b, ";");
}

//------------------------------- METHODS --------------------------------------------
void pic_open(t_pic* x, t_symbol *filename){
    if(filename){
        if(filename == gensym("empty") && x->x_def_img)
            return;
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
        t_symbol *snd = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
        if(snd != x->x_send){
            x->x_snd_raw = s;
            x->x_send = snd;
            x->x_snd_set = 1;
            canvas_dirty(x->x_glist, 1);
            t_canvas *cv = glist_getcanvas(x->x_glist);
            sys_vgui(".x%lx.c delete %lx_out\n", cv, x);
            if(x->x_send == &s_ && x->x_edit)
                pic_draw_inlet(x);
        }
    }
}

static void pic_receive(t_pic *x, t_symbol *s){
    if(s != gensym("")){
        t_symbol *rcv = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
        if(rcv != x->x_receive){
            x->x_rcv_set = 1;
            x->x_rcv_raw = s;
            x->x_receive = rcv;
            canvas_dirty(x->x_glist, 1);
            t_canvas *cv = glist_getcanvas(x->x_glist);
            if(x->x_bound)
                pd_unbind(&x->x_obj.ob_pd, x->x_receive);
            sys_vgui(".x%lx.c delete %lx_in\n", cv, x);
            if(rcv == &s_){
                x->x_bound = 0;
                if(x->x_edit)
                    pic_draw_inlet(x);
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
            if(x->x_sel) sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline blue -width %d\n",
                cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x, x->x_zoom);
            else sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black -width %d\n",
                cv, xpos, ypos, xpos+x->x_width, ypos+x->x_height, x, x->x_zoom);
        }
        else if(!x->x_edit)
            sys_vgui(".x%lx.c delete %lxSEL\n", cv, x);
    }
}

static void pic_tick(t_pic *x){
    outlet_float(x->x_outlet, x->x_width);
}

static void pic_size(t_pic *x){
    clock_delay(x->x_clock, 10);
}


static void pic_bang(t_pic *x){
    outlet_bang(x->x_outlet);
    if(x->x_send != &s_ && x->x_send->s_thing)
        pd_bang(x->x_send->s_thing);
}

static void edit_proxy_any(t_edit_proxy *p, t_symbol *s, int ac, t_atom *av){
    int edit = ac = 0;
    if(p->p_cnv){
        if(s == gensym("editmode"))
            edit = (int)(av->a_w.w_float);
        else if(s == gensym("obj") || s == gensym("msg") || s == gensym("floatatom") || s == gensym("text")){
            if(av->a_w.w_float == 0)
                edit = 1;
        }
        else return;
        if(p->p_cnv->x_edit != edit){
            p->p_cnv->x_edit = edit;
            t_canvas *cv = glist_getcanvas(p->p_cnv->x_glist);
            if(edit){
                int x = text_xpix(&p->p_cnv->x_obj, p->p_cnv->x_glist), y = text_ypix(&p->p_cnv->x_obj, p->p_cnv->x_glist), w = p->p_cnv->x_width, h = p->p_cnv->x_height;
                if(!p->p_cnv->x_outline)
                    sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxSEL -outline black -width %d\n",
                             cv, x, y, x+w, y+h, p->p_cnv, p->p_cnv->x_zoom);
                pic_draw_inlet(p->p_cnv);
            }
            else{
                if(!p->p_cnv->x_outline)
                    sys_vgui(".x%lx.c delete %lxSEL\n", cv, p->p_cnv);
                sys_vgui(".x%lx.c delete %lx_in\n", cv, p->p_cnv);
                sys_vgui(".x%lx.c delete %lx_out\n", cv, p->p_cnv);
            }
        }
    }
}

static void pic_zoom(t_pic *x, t_floatarg zoom){
    x->x_zoom = (int)zoom;
    pic_draw_inlet(x);
}

//------------------- Properties --------------------------------------------------------
void pic_properties(t_gobj *z, t_glist *gl){
    gl = NULL;
    t_pic *x = (t_pic *)z;
    char buf[256];
    sprintf(buf, "pic_properties %%s %s %d %s %s\n",
        x->x_filename == &s_ ? gensym("empty")->s_name : x->x_filename->s_name,
        x->x_outline,
        x->x_send == &s_ ? gensym("empty")->s_name : x->x_send->s_name,
        x->x_receive == &s_ ? gensym("empty")->s_name : x->x_receive->s_name);
    gfxstub_new(&x->x_obj.ob_pd, x, buf);
}

static void pic_ok(t_pic *x, t_symbol *name, t_floatarg outline, t_symbol *snd, t_symbol *rcv){
    pic_open(x, name);
    pic_outline(x, outline);
    pic_send(x, snd);
    pic_receive(x, rcv);
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
    if(x->x_clock)
        clock_free(x->x_clock);
}

static void *pic_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pic *x = (t_pic *)pd_new(pic_class);
    x->x_canvas = canvas_getcurrent();
    x->x_glist = (t_glist*)x->x_canvas;
    x->x_zoom = x->x_glist->gl_zoom;
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
    int loaded = x->x_rcv_set = x->x_snd_set = x->x_bound = x->x_def_img = x->x_init = x->x_outline = x->x_flag = 0;
    x->x_fullname = NULL;
    if(ac && av->a_type == A_FLOAT){ // 1ST outline
        x->x_outline = (int)(av->a_w.w_float != 0);
        ac--; av++;
        if(ac && av->a_type == A_SYMBOL){ // 2ND filename
            if(av->a_w.w_symbol == gensym("empty")) // ignore empty symbol
                ac--, av++;
            else{
                x->x_filename = av->a_w.w_symbol;
                ac--, av++;
            }
        }
        if(ac && av->a_type == A_SYMBOL){ // 3RD Send
            if(av->a_w.w_symbol == gensym("empty")) // ignore
                ac--, av++;
            else{
                x->x_send = av->a_w.w_symbol;
                ac--, av++;
            }
            if(ac && av->a_type == A_SYMBOL){ // 4TH Receive
                if(av->a_w.w_symbol == gensym("empty")) // ignore
                    ac--, av++;
                else{
                    x->x_receive = av->a_w.w_symbol;
                    ac--, av++;
                }
            }
        }
    }
    t_symbol *cursym = NULL, *sym = NULL;
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            cursym = atom_getsymbolarg(0, ac, av);
            if(cursym == gensym("-outline")){ // no arg
                x->x_outline = x->x_flag = 1;
                ac--, av++;
            }
            else if(cursym == gensym("-send")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    sym = atom_getsymbolarg(1, ac, av);
                    x->x_flag = x->x_s_flag = 1;
                    if(sym != gensym("empty"))
                        x->x_send = sym;
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(cursym == gensym("-receive")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    sym = atom_getsymbolarg(1, ac, av);
                    x->x_flag = x->x_r_flag = 1;
                    if(sym != gensym("empty"))
                        x->x_receive = sym;
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(cursym == gensym("-open")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    sym = atom_getsymbolarg(1, ac, av);
                    x->x_flag = 1;
                    if(sym != gensym("empty"))
                        x->x_filename = sym;
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else goto errstate;
        }
        else goto errstate;
    };
    if(x->x_filename !=  &s_){
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
    x->x_snd_raw = x->x_send;
    x->x_rcv_raw = x->x_receive;
    x->x_outlet = outlet_new(&x->x_obj, &s_bang);
    x->x_clock = clock_new(x, (t_method)pic_tick);
    return(x);
    errstate:
        pd_error(x, "[pic]: improper args");
        return NULL;
}

void pic_setup(void){
    pic_class = class_new(gensym("pic"), (t_newmethod)pic_new, (t_method)pic_free, sizeof(t_pic),0, A_GIMME,0);
    class_addbang(pic_class, pic_bang);
    class_addmethod(pic_class, (t_method)pic_outline, gensym("outline"), A_DEFFLOAT, 0);
    class_addmethod(pic_class, (t_method)pic_size, gensym("size"), 0);
    class_addmethod(pic_class, (t_method)pic_open, gensym("open"), A_DEFSYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_send, gensym("send"), A_DEFSYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_size_callback, gensym("_picsize"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(pic_class, (t_method)pic_ok, gensym("ok"), A_SYMBOL, A_FLOAT, A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_receive, gensym("receive"), A_DEFSYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_zoom, gensym("zoom"), A_CANT, 0);
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
    class_setpropertiesfn(pic_class, &pic_properties);
    sys_vgui("image create photo pic_def_img -data {R0lGODdhJwAnAMQAAAAAAAsLCxMTExwcHCIiIisrKzw8PERERExMTFRUVF1dXWVlZW1tbXNzc3t7e4ODg42NjZOTk5qamqSkpK2trbS0tLy8vMPDw8zMzNTU1Nzc3OPj4+3t7fT09P///wAAACH5BAkKAB8ALAAAAAAnACcAAAX/oCeOJKlRTzIARwNVWinPNNYAeK4HjNXRQNLGkRsIArnAYIVbZILAjAFAYAICgmxyQMBZoDJNt1vUGc0CAAY86iioSdwgkTjIzQAEh+2hwHFpAhQbHIUZRGk5XRRsbldxazIQAFZpCz9QGjqUABM0HHZIjwMxUBgAiUh6QJOVAE9QGVRGBQCMQBJ/qGpgHUQ5l0GtOWmwUBwTCwoRe0AbZDhIBRt8Hh2YQURWKw/VfMOKvN5BHA+cObUR40EZCOc4tQzY6yUXONACXQzN9CUWV9twRJjXT4S9M/e8FJTBoVYiTgxKLSRRQdcKCAQnejDHZIUDjTNuJFphDOSICAAKUQiIZzLMpgstZWR4sKABzJgzMuLcyXMdBwsTKEjcqcFdjls4HRXgsuJmTHu1EjbYOeFdmgT8TFaEtiJYzA13UnbiWfERgAY6QdoQgGBCWiAhAAA7}\n");
    sys_vgui("if {[catch {pd}]} {\n");
    sys_vgui("    proc pd {args} {pdsend [join $args \" \"]}\n");
    sys_vgui("}\n");
    
    sys_vgui("proc pic_ok {id} {\n");
    sys_vgui("    set vid [string trimleft $id .]\n");
    sys_vgui("    set var_name [concat var_name_$vid]\n");
    sys_vgui("    set var_outline [concat var_outline_$vid]\n");
    sys_vgui("    set var_snd [concat var_snd_$vid]\n");
    sys_vgui("    set var_rcv [concat var_rcv_$vid]\n");
    sys_vgui("\n");
    sys_vgui("    global $var_name\n");
    sys_vgui("    global $var_outline\n");
    sys_vgui("    global $var_snd\n");
    sys_vgui("    global $var_rcv\n");
    sys_vgui("\n");
    sys_vgui("    set cmd [concat $id ok \\\n");
    sys_vgui("        [eval concat $$var_name] \\\n");
    sys_vgui("        [eval concat $$var_outline] \\\n");
    sys_vgui("        [eval concat $$var_snd] \\\n");
    sys_vgui("        [eval concat $$var_rcv] \\;]\n");
    sys_vgui("    pd $cmd\n");
    sys_vgui("    pic_cancel $id\n");
    sys_vgui("}\n");

    sys_vgui("proc pic_cancel {id} {\n");
    sys_vgui("    set cmd [concat $id cancel \\;]\n");
    sys_vgui("    pd $cmd\n");
    sys_vgui("}\n");
    
    sys_vgui("proc pic_properties {id name outline snd rcv} {\n");
    sys_vgui("    set vid [string trimleft $id .]\n");
    sys_vgui("    set var_name [concat var_name_$vid]\n");
    sys_vgui("    set var_outline [concat var_outline_$vid]\n");
    sys_vgui("    set var_snd [concat var_snd_$vid]\n");
    sys_vgui("    set var_rcv [concat var_rcv_$vid]\n");
    sys_vgui("\n");
    sys_vgui("    global $var_name\n");
    sys_vgui("    global $var_outline\n");
    sys_vgui("    global $var_snd\n");
    sys_vgui("    global $var_rcv\n");
    sys_vgui("\n");
    sys_vgui("    set $var_name $name\n");
    sys_vgui("    set $var_outline $outline\n");
    sys_vgui("    set $var_snd $snd\n");
    sys_vgui("    set $var_rcv $rcv\n");
    sys_vgui("\n");
    sys_vgui("    toplevel $id\n");
    sys_vgui("    wm title $id {[pic] Properties}\n");
    sys_vgui("    wm protocol $id WM_DELETE_WINDOW [concat pic_cancel $id]\n");
    sys_vgui("\n");
    sys_vgui("    frame $id.pic\n");
    sys_vgui("    pack $id.pic -side top\n");
    sys_vgui("    label $id.pic.lname -text \"File Name:\"\n");
    sys_vgui("    entry $id.pic.name -textvariable $var_name -width 7\n");
    sys_vgui("    label $id.pic.loutline -text \"Outline:\"\n");
    sys_vgui("    entry $id.pic.outline -textvariable $var_outline -width 1\n");
    sys_vgui("    pack $id.pic.lname $id.pic.name $id.pic.loutline $id.pic.outline -side left\n");
    sys_vgui("\n");
    sys_vgui("    frame $id.snd_rcv\n");
    sys_vgui("    pack $id.snd_rcv -side top\n");
    sys_vgui("    label $id.snd_rcv.lsnd -text \"Send symbol:\"\n");
    sys_vgui("    entry $id.snd_rcv.snd -textvariable $var_snd -width 7\n");
    sys_vgui("    label $id.snd_rcv.lrcv -text \"Receive symbol:\"\n");
    sys_vgui("    entry $id.snd_rcv.rcv -textvariable $var_rcv -width 7\n");
    sys_vgui("    pack $id.snd_rcv.lsnd $id.snd_rcv.snd $id.snd_rcv.lrcv $id.snd_rcv.rcv -side left\n");
    sys_vgui("\n");
    sys_vgui("    frame $id.buttonframe\n");
    sys_vgui("    pack $id.buttonframe -side bottom -fill x -pady 2m\n");
    sys_vgui("    button $id.buttonframe.cancel -text {Cancel} -command \"pic_cancel $id\"\n");
    sys_vgui("    button $id.buttonframe.ok -text {OK} -command \"pic_ok $id\"\n");
    sys_vgui("    pack $id.buttonframe.cancel -side left -expand 1\n");
    sys_vgui("    pack $id.buttonframe.ok -side left -expand 1\n");
    sys_vgui("}\n");
}
