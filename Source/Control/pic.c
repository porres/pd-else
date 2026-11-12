// porres 2018-2020

#include <m_pd.h>
#include <g_canvas.h>

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>

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
     int            x_zoom;
     int            x_width;
     int            x_height;
     int            x_snd_set;
     int            x_rcv_set;
     int            x_edit;
     int            x_init;
     int            x_def_img;
     int            x_sel;
     int            x_outline;
     int            x_s_flag;
     int            x_r_flag;
     int            x_flag;
     int            x_size;
     int            x_latch;
     int            x_offset_x;
     int            x_offset_y;
     t_symbol      *x_fullname;
     t_symbol      *x_filename;
     t_symbol      *x_x;
     t_symbol      *x_receive;
     t_symbol      *x_rcv_raw;
     t_symbol      *x_send;
     t_symbol      *x_snd_raw;
     t_outlet      *x_outlet;
     char           x_tag_in[32];
     char           x_tag_out[32];
     char           x_tag_picture[32];
     char           x_tag_outline[32];
}t_pic;

// draw inlet/ outlet
static void pic_draw_io_let(t_pic *x){
t_canvas *cv = glist_getcanvas(x->x_glist);
int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
int iocol = x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor;
pdgui_vmess(0, "crs", cv, "delete", x->x_tag_in);
if(x->x_edit && x->x_receive == &s_)
    pdgui_vmess(0, "crs iiii rk rk rs", cv, "create", "rectangle",
        xpos, ypos, xpos+(IOWIDTH*x->x_zoom), ypos+(IHEIGHT*x->x_zoom),
        "-fill", iocol, "-outline", iocol, "-tags", x->x_tag_in);
pdgui_vmess(0, "crs", cv, "delete", x->x_tag_out);
if(x->x_edit && x->x_send == &s_)
    pdgui_vmess(0, "crs iiii rk rk rs", cv, "create", "rectangle",
        xpos, ypos+(x->x_height * x->x_zoom),
        xpos+IOWIDTH*x->x_zoom, ypos+(x->x_height * x->x_zoom)-(IHEIGHT*x->x_zoom),
        "-fill", iocol, "-outline", iocol, "-tags", x->x_tag_out);
}

// --------------------------------------------------------------------------------------
// helper functions
static t_symbol* pic_filepath(t_pic *x, const char *filename){
    static char fn[MAXPDSTRING];
    char *bufptr;
    int fd = canvas_open(glist_getcanvas(x->x_glist),
        filename, "", fn, &bufptr, MAXPDSTRING, 1);
    if(fd > 0){
        fn[strlen(fn)]='/';
        sys_close(fd);
        return gensym(fn);
    }
    else
        return NULL;
}

static void pic_mouserelease(t_pic* x){
    if(x->x_latch){
        outlet_float(x->x_outlet, 0);
        if(x->x_send != &s_ && x->x_send->s_thing)
            pd_float(x->x_send->s_thing, 0);
    }
}

static void pic_get_snd_rcv(t_pic* x){
    t_binbuf *bb = x->x_obj.te_binbuf;
    int n_args = binbuf_getnatom(bb), i = 0; // number of arguments
    char buf[128];
    if(!x->x_snd_set){ // no send set, search arguments/flags
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // arguments are flags actually
                if(x->x_s_flag){ // we got a search flag, let's get it
                    for(i = 0;  i < n_args; i++){
                        atom_string(binbuf_getvec(bb) + i, buf, 80);
                        if(gensym(buf) == gensym("-send")){
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
    if(x->x_snd_raw == &s_)
        x->x_snd_raw = gensym("empty");
    if(!x->x_rcv_set){ // no receive set, search arguments
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // arguments are flags actually
                if(x->x_r_flag){ // we got a receive flag, let's get it
                    for(i = 0;  i < n_args; i++){
                        atom_string(binbuf_getvec(bb) + i, buf, 80);
                        if(gensym(buf) == gensym("-receive")){
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
    if(x->x_rcv_raw == &s_)
        x->x_rcv_raw = gensym("empty");
}

// ------------------------ pic widgetbehaviour-------------------------------------------------------------------
static int pic_click(t_pic *x, struct _glist *glist, int xpos, int ypos, int shift, int alt, int dbl, int doit){
    glist = NULL, xpos = ypos = shift = alt = dbl = 0;
    if(doit){
        x->x_latch ? outlet_float(x->x_outlet, 1) : outlet_bang(x->x_outlet) ;
        if(x->x_send != &s_ && x->x_send->s_thing)
            x->x_latch ? pd_float(x->x_send->s_thing, 1) : pd_bang(x->x_send->s_thing);
    }
    return(1);
}

static void pic_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_pic* x = (t_pic*)z;
    int xpos = *xp1 = text_xpix(&x->x_obj, glist), ypos = *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = xpos + (x->x_width * x->x_zoom), *yp2 = ypos + (x->x_height * x->x_zoom);
}

static void pic_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_pic *obj = (t_pic *)z;
    obj->x_obj.te_xpix += dx, obj->x_obj.te_ypix += dy;
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "crs ii", cv, "move", obj->x_tag_outline, dx*obj->x_zoom, dy*obj->x_zoom);
    pdgui_vmess(0, "crs ii", cv, "move", obj->x_tag_picture, dx*obj->x_zoom, dy*obj->x_zoom);
    if(obj->x_receive == &s_)
        pdgui_vmess(0, "crs ii", cv, "move", obj->x_tag_in, dx*obj->x_zoom, dy*obj->x_zoom);
    if(obj->x_send == &s_)
        pdgui_vmess(0, "crs ii", cv, "move", obj->x_tag_out, dx*obj->x_zoom, dy*obj->x_zoom);
    canvas_fixlinesfor(glist, (t_text*)obj);
}

static void pic_select(t_gobj *z, t_glist *glist, int state){
    t_pic *x = (t_pic *)z;
    int xpos = text_xpix(&x->x_obj, glist);
    int ypos = text_ypix(&x->x_obj, glist);
    t_canvas *cv = glist_getcanvas(glist);
    x->x_sel = state;
    int iocol = state ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor;
    if(x->x_edit){
        if(x->x_receive == &s_)
            pdgui_vmess(0, "crs rk rk", cv, "itemconfigure", x->x_tag_in,
                "-outline", iocol, "-fill", iocol);
        if(x->x_send == &s_)
            pdgui_vmess(0, "crs rk rk", cv, "itemconfigure", x->x_tag_out,
                "-outline", iocol, "-fill", iocol);
    }
    if(state){
        pdgui_vmess(0, "crs", cv, "delete", x->x_tag_outline);
        pdgui_vmess(0, "crr iiii rs rk ri", cv, "create", "rectangle",
            xpos, ypos, xpos + (x->x_width*x->x_zoom), ypos + (x->x_height*x->x_zoom),
            "-tags", x->x_tag_outline, "-outline", THISGUI->i_selectcolor,
            "-width", x->x_zoom);
    }
    else{
        pdgui_vmess(0, "crs", cv, "delete", x->x_tag_outline);
        if(x->x_edit || x->x_outline)
            pdgui_vmess(0, "crr iiii rs rk ri", cv, "create", "rectangle",
                xpos, ypos, xpos + (x->x_width*x->x_zoom), ypos + (x->x_height*x->x_zoom),
                "-tags", x->x_tag_outline, "-outline", THISGUI->i_foregroundcolor,
                "-width", x->x_zoom);
    }
}

static void pic_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void pic_draw(t_pic* x, struct _glist *glist, t_floatarg vis){
    t_canvas *cv = glist_getcanvas(glist);
    int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
    int visible = (glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist));
    if(x->x_def_img && (visible || vis)){ // DEFAULT PIC
        pdgui_vmess(0, "crs ii rs rs", cv, "create", "image",
            xpos, ypos, "-anchor", "nw", "-tags", x->x_tag_picture);
        pdgui_vmess(0, "crs rs", cv, "itemconfigure",
            x->x_tag_picture, "-image", "pic_def_img");
        if(x->x_edit || x->x_outline)
            pdgui_vmess(0, "crr iiii rs rk ri", cv, "create", "rectangle",
                xpos, ypos, xpos + (x->x_width*x->x_zoom), ypos + (x->x_height*x->x_zoom),
                "-tags", x->x_tag_outline,
                "-outline", x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor,
                "-width", x->x_zoom);
    }
    else{
        if(visible || vis){
            int clipped_w = x->x_width - abs(x->x_offset_x);
            int clipped_h = x->x_height - abs(x->x_offset_y);
            if(clipped_w < 0) clipped_w = 0;
            if(clipped_h < 0) clipped_h = 0;
            int source_offset_x = -x->x_offset_x < 0 ? 0 : -x->x_offset_x;
            int source_offset_y = -x->x_offset_y < 0 ? 0 : -x->x_offset_y;
            int dest_offset_x = x->x_offset_x < 0 ? 0 : x->x_offset_x;
            int dest_offset_y = x->x_offset_y < 0 ? 0 : x->x_offset_y;

            char buf[1024];
            snprintf(buf, sizeof(buf),
                "if {[info exists %lx_picname] == 1} {\n"
                "    .x%lx.c delete %lx_picture\n"
                "    image create photo p%lx_piccopy\n"
                "    p%lx_piccopy copy %lx_picname -shrink -from %d %d %d %d -to %d %d %d %d -zoom %d %d\n"
                "    .x%lx.c create image %d %d -anchor nw -image p%lx_piccopy -tags %lx_picture\n"
                "}",
                (unsigned long)x->x_fullname,
                (unsigned long)cv, (unsigned long)x,
                (unsigned long)x,
                (unsigned long)x, (unsigned long)x->x_fullname,
                source_offset_x, source_offset_y,
                source_offset_x + clipped_w, source_offset_y + clipped_h,
                dest_offset_x, dest_offset_y,
                dest_offset_x + clipped_w * x->x_zoom, dest_offset_y + clipped_h * x->x_zoom,
                x->x_zoom, x->x_zoom,
                (unsigned long)cv, xpos, ypos,
                (unsigned long)x, (unsigned long)x);
            pdgui_vmess(buf, NULL);
        }
        if(!x->x_init)
            x->x_init = 1;
        else if((visible || vis) && (x->x_edit || x->x_outline)){
            char fg[32];
            snprintf(fg, sizeof(fg), "#%06X",
                x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
            char buf[512];
            snprintf(buf, sizeof(buf),
                "if {[info exists %lx_picname] == 1} {.x%lx.c create rectangle %d %d %d %d -tags %lx_outline -outline %s -width %d}",
                (unsigned long)x->x_fullname,
                (unsigned long)cv,
                xpos, ypos, xpos + (x->x_width*x->x_zoom), ypos + (x->x_height*x->x_zoom),
                (unsigned long)x, fg, x->x_zoom);
            pdgui_vmess(buf, NULL);
        }

        if(x->x_width == 0 || x->x_height == 0) {
            char buf[512];
            snprintf(buf, sizeof(buf),
                "if {[info exists %lx_picname] == 1} {pdsend \"%s _picsize [image width %lx_picname] [image height %lx_picname]\"}",
                (unsigned long)x->x_fullname,
                x->x_x->s_name,
                (unsigned long)x->x_fullname,
                (unsigned long)x->x_fullname);
            pdgui_vmess(buf, NULL);
        }
    }
    char buf2[512];
    snprintf(buf2, sizeof(buf2),
        ".x%lx.c bind %lx_picture <ButtonRelease> {pdsend [concat %s _mouserelease \\;]}",
        (unsigned long)cv, (unsigned long)x, x->x_x->s_name);
    pdgui_vmess(buf2, NULL);
    pic_draw_io_let(x);
}

static void pic_erase(t_pic* x, struct _glist *glist){
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_picture);
    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_in);
    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_out);
    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_outline);
}

static void pic_vis(t_gobj *z, t_glist *glist, int vis){
    t_pic* x = (t_pic*)z;
    vis ? pic_draw(x, glist, 1) : pic_erase(x, glist);
}

static void pic_save(t_gobj *z, t_binbuf *b){
    t_pic *x = (t_pic *)z;
    if(x->x_filename == &s_)
        x->x_filename = gensym("empty");
    pic_get_snd_rcv(x);
    binbuf_addv(b, "ssiisisssii", gensym("#X"), gensym("obj"), x->x_obj.te_xpix, x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)), x->x_outline, x->x_filename, x->x_snd_raw,
        x->x_rcv_raw, x->x_size, x->x_latch);
    binbuf_addv(b, ";");
}

//------------------------------- METHODS --------------------------------------------
static void pic_size_callback(t_pic *x, t_float w, t_float h){ // callback
    x->x_width = w;
    x->x_height = h;
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
        t_canvas *cv = glist_getcanvas(x->x_glist);
        int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
        pic_draw(x, x->x_glist, 0);
        canvas_fixlinesfor(x->x_glist, (t_text*)x);
        if(x->x_edit || x->x_outline){
            pdgui_vmess(0, "crs", cv, "delete", x->x_tag_outline);
            pdgui_vmess(0, "crr iiii rs rk ri", cv, "create", "rectangle",
                xpos, ypos, xpos + (x->x_width*x->x_zoom), ypos + (x->x_height*x->x_zoom),
                "-tags", x->x_tag_outline,
                "-outline", x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor,
                "-width", x->x_zoom);
            pic_draw_io_let(x);
        }
    }
    else
        pic_erase(x, x->x_glist);
    if(x->x_size){
        t_atom at[2];
        SETFLOAT(at, w);
        SETFLOAT(at+1, h);
        outlet_list(x->x_obj.ob_outlet, &s_list, 2, at);
    }
}

void pic_open(t_pic* x, t_symbol *filename){
    if(filename){
        if(filename == gensym("empty") && x->x_def_img)
            return;
        if(filename != x->x_filename){
            const t_symbol *file_name_open = pic_filepath(x, filename->s_name); // path
            if(file_name_open){
                x->x_filename = filename;
                x->x_fullname = (t_symbol *)file_name_open;
                if(x->x_def_img)
                    x->x_def_img = 0;
                if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
                    pic_erase(x, x->x_glist);
                    char buf[1024];
                    snprintf(buf, sizeof(buf),
                        "if {[info exists %lx_picname] == 0} {image create photo %lx_picname -file \"%s\"\n set %lx_picname 1\n}",
                        (unsigned long)x->x_fullname, (unsigned long)x->x_fullname,
                        file_name_open->s_name, (unsigned long)x->x_fullname);
                    pdgui_vmess(buf, NULL);
                    x->x_width = 0;
                    x->x_height = 0;
                    pic_draw(x, x->x_glist, 0);
                }
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
        t_symbol *snd = (s == gensym("empty")) ? &s_ : canvas_realizedollar(x->x_glist, s);
        if(snd != x->x_send){
            x->x_snd_raw = s;
            x->x_send = snd;
            x->x_snd_set = 1;
            if(x->x_edit && glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
                if(x->x_send == &s_)
                    pic_draw_io_let(x);
                else
                    pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_out);
            }
        }
    }
}

static void pic_receive(t_pic *x, t_symbol *s){
    if(s != gensym("")){
        t_symbol *rcv = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
        if(rcv != x->x_receive){
            if(x->x_receive != &s_)
                pd_unbind(&x->x_obj.ob_pd, x->x_receive);
            x->x_rcv_set = 1;
            x->x_rcv_raw = s;
            x->x_receive = rcv;
            if(x->x_receive == &s_){
                if(x->x_edit && glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
                    pic_draw_io_let(x);
            }
            else{
                pd_bind(&x->x_obj.ob_pd, x->x_receive);
                if(x->x_edit && glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
                    pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_in);
            }
        }
    }
}

static void pic_outline(t_pic *x, t_float f){
    int outline = (int)(f != 0);
    if(x->x_outline != outline){
        x->x_outline = outline;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
            t_canvas *cv = glist_getcanvas(x->x_glist);
            if(x->x_outline){
                int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
                pdgui_vmess(0, "crr iiii rs rk ri", cv, "create", "rectangle",
                    xpos, ypos, xpos + (x->x_width*x->x_zoom), ypos + (x->x_height*x->x_zoom),
                    "-tags", x->x_tag_outline,
                    "-outline", x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor,
                    "-width", x->x_zoom);
            }
            else if(!x->x_edit)
                pdgui_vmess(0, "crs", cv, "delete", x->x_tag_outline);
        }
    }
}

static void pic_size(t_pic *x, t_float f){
    x->x_size = f != 0.0f;
}

static void pic_offset(t_pic *x, t_float x_offset, t_float y_offset){
    x->x_offset_x = (int)x_offset;
    x->x_offset_y = (int)y_offset;
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
        pic_erase(x, x->x_glist);
        pic_draw(x, x->x_glist, 1);
    }
}

static void pic_latch(t_pic *x, t_float f){
    x->x_latch = f != 0.0f;
}

static void edit_proxy_any(t_edit_proxy *p, t_symbol *s, int ac, t_atom *av){
    int edit = ac = 0;
    if(p->p_cnv){
        if(s == gensym("editmode"))
            edit = (int)(av->a_w.w_float);
         else if(s == gensym("obj") || s == gensym("msg") || s == gensym("floatatom")
        || s == gensym("symbolatom") || s == gensym("text") || s == gensym("bng")
        || s == gensym("toggle") || s == gensym("numbox") || s == gensym("vslider")
        || s == gensym("hslider") || s == gensym("vradio") || s == gensym("hradio")
        || s == gensym("vumeter") || s == gensym("mycnv") || s == gensym("selectall")){
            edit = 1;
        }
        else
            return;
        if(p->p_cnv->x_edit != edit){
            p->p_cnv->x_edit = edit;
            t_canvas *cv = glist_getcanvas(p->p_cnv->x_glist);
            if(edit){
                int x = text_xpix(&p->p_cnv->x_obj, p->p_cnv->x_glist);
                int y = text_ypix(&p->p_cnv->x_obj, p->p_cnv->x_glist);
                int w = p->p_cnv->x_width, h = p->p_cnv->x_height;
                if(!p->p_cnv->x_outline)
                    pdgui_vmess(0, "crr iiii rs rk ri", cv, "create", "rectangle",
                        x, y, x+(w * cv->gl_zoom), y+(h * cv->gl_zoom),
                        "-tags", p->p_cnv->x_tag_outline,
                        "-outline", p->p_cnv->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor,
                        "-width", p->p_cnv->x_zoom);
                pic_draw_io_let(p->p_cnv);
            }
            else{
                if(!p->p_cnv->x_outline)
                    pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_outline);
                pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_in);
                pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_out);
            }
        }
    }
}

static void pic_zoom(t_pic *x, t_floatarg zoom){
    x->x_zoom = (int)zoom;
    canvas_fixlinesfor(x->x_glist, (t_text*)x);
}

//------------------- Properties --------------------------------------------------------
void pic_properties(t_gobj *z, t_glist *gl){
    gl = NULL;
    t_pic *x = (t_pic *)z;
    if(x->x_filename ==  &s_)
        x->x_filename = gensym("empty");
    pic_get_snd_rcv(x);
    pdgui_stub_vnew(&x->x_obj.ob_pd, "pic_properties", x,
        "s i i i s s",
        x->x_filename->s_name,
        x->x_outline,
        x->x_size,
        x->x_latch,
        x->x_snd_raw->s_name,
        x->x_rcv_raw->s_name);
}

static void pic_ok(t_pic *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_atom undo[6];
    SETSYMBOL(undo+0, x->x_filename);
    SETFLOAT(undo+1, x->x_outline);
    SETFLOAT(undo+2, x->x_size);
    SETFLOAT(undo+3, x->x_latch);
    SETSYMBOL(undo+4, x->x_snd_raw);
    SETSYMBOL(undo+5, x->x_rcv_raw);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("ok"), 6, undo, ac, av);
    pic_open(x, atom_getsymbolarg(0, ac, av));
    pic_outline(x, atom_getfloatarg(1, ac, av));
    pic_size(x, atom_getfloatarg(2, ac, av));
    pic_latch(x, atom_getfloatarg(3, ac, av));
    pic_send(x, atom_getsymbolarg(4, ac, av));
    pic_receive(x, atom_getsymbolarg(5, ac, av));
    canvas_dirty(x->x_glist, 1);
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
    char buf[512];
    snprintf(buf, sizeof(buf),
        "if {[info exists %lx_picname] == 1 && [image inuse %lx_picname] == 0} { image delete %lx_picname\n unset %lx_picname\n}",
        (unsigned long)x->x_fullname, (unsigned long)x->x_fullname,
        (unsigned long)x->x_fullname, (unsigned long)x->x_fullname);
    pdgui_vmess(buf, NULL);
    if(x->x_receive != &s_)
        pd_unbind(&x->x_obj.ob_pd, x->x_receive);
    pd_unbind(&x->x_obj.ob_pd, x->x_x);
    x->x_proxy->p_cnv = NULL;
    clock_delay(x->x_proxy->p_clock, 0);
    pdgui_stub_deleteforkey(x);
}

static void *pic_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pic *x = (t_pic *)pd_new(pic_class);
    t_canvas *cv = canvas_getcurrent();
    x->x_glist = (t_glist*)cv;
    x->x_zoom = x->x_glist->gl_zoom;
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)cv);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_x = gensym(buf));
    x->x_edit = cv->gl_edit;
    x->x_send = x->x_snd_raw = x->x_receive = x->x_rcv_raw = x->x_filename = &s_;
    int loaded = x->x_rcv_set = x->x_snd_set = x->x_def_img = x->x_init = x->x_latch = 0;
    x->x_outline = x->x_size = x->x_offset_x = x->x_offset_y = 0;
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
                    if(ac && av->a_type == A_FLOAT){ // 5TH size
                        x->x_size = (int)(av->a_w.w_float != 0);
                        ac--; av++;
                    }
                    if(ac && av->a_type == A_FLOAT){ // 6TH latch
                        x->x_latch = (int)(av->a_w.w_float != 0);
                        ac--; av++;
                    }
                }
            }
        }
    }
    t_symbol *cursym = NULL, *sym = NULL;
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            cursym = atom_getsymbolarg(0, ac, av);
            if(cursym == gensym("-outline")){ // no arg
                x->x_flag = x->x_outline = 1;
                ac--, av++;
            }
            else if(cursym == gensym("-size")){ // no arg
                x->x_flag = x->x_size = 1;
                ac--, av++;
            }
            else if(cursym == gensym("-latch")){ // no arg
                x->x_flag = x->x_latch = 1;
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
                    x->x_flag = 1;
                    sym = atom_getsymbolarg(1, ac, av);
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
        t_symbol *fn = pic_filepath(x, x->x_filename->s_name); // full path
        if(fn){
            loaded = 1;
            x->x_fullname = fn;
            char buf2[1024];
            snprintf(buf2, sizeof(buf2),
                "if {[info exists %lx_picname] == 0} {image create photo %lx_picname -file \"%s\"\n set %lx_picname 1\n}",
                (unsigned long)x->x_fullname, (unsigned long)x->x_fullname,
                fn->s_name, (unsigned long)x->x_fullname);
            pdgui_vmess(buf2, NULL);
        }
        else
            pd_error(x, "[pic]: error opening file '%s'", x->x_filename->s_name);
    }
    if(!loaded){ // default image
        x->x_width = x->x_height = 38;
        x->x_def_img = 1;
    }
    if(x->x_receive != &s_)
        pd_bind(&x->x_obj.ob_pd, x->x_receive);
    x->x_outlet = outlet_new(&x->x_obj, &s_bang);
    // tags
    snprintf(x->x_tag_in,      sizeof(x->x_tag_in),      "%lx_in",      (unsigned long)x);
    snprintf(x->x_tag_out,     sizeof(x->x_tag_out),     "%lx_out",     (unsigned long)x);
    snprintf(x->x_tag_picture, sizeof(x->x_tag_picture), "%lx_picture", (unsigned long)x);
    snprintf(x->x_tag_outline, sizeof(x->x_tag_outline), "%lx_outline", (unsigned long)x);
    return(x);
    errstate:
        pd_error(x, "[pic]: improper args");
        return NULL;
}

void pic_setup(void){
    pic_class = class_new(gensym("pic"), (t_newmethod)pic_new, (t_method)pic_free, sizeof(t_pic),0, A_GIMME,0);
    class_addmethod(pic_class, (t_method)pic_outline, gensym("outline"), A_DEFFLOAT, 0);
    class_addmethod(pic_class, (t_method)pic_size, gensym("size"), A_DEFFLOAT, 0);
    class_addmethod(pic_class, (t_method)pic_latch, gensym("latch"), A_DEFFLOAT, 0);
    class_addmethod(pic_class, (t_method)pic_open, gensym("open"), A_DEFSYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_send, gensym("send"), A_DEFSYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_ok, gensym("ok"), A_GIMME, 0);
    class_addmethod(pic_class, (t_method)pic_receive, gensym("receive"), A_DEFSYMBOL, 0);
    class_addmethod(pic_class, (t_method)pic_offset, gensym("offset"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(pic_class, (t_method)pic_zoom, gensym("zoom"), A_CANT, 0);
    class_addmethod(pic_class, (t_method)pic_size_callback, gensym("_picsize"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(pic_class, (t_method)pic_mouserelease, gensym("_mouserelease"), 0);
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
    #include "../Extra/pic_dialog.h"
    pdgui_vmess(pic_dialog_tcl, NULL);
}
