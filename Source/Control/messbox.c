// based on entry

#include <m_pd.h>
#include <g_canvas.h>
#include <string.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

#define HANDLE_SIZE  12
#define MIN_WIDTH    60
#define MIN_HEIGHT   30
#define BORDER       5

typedef struct _messbox_proxy{
    t_object         p_ob;
    struct _messbox *p_master;
    t_symbol        *x_bind_sym;
}t_messbox_proxy;

typedef struct _messbox_edit_proxy{
    t_object             p_obj;
    t_symbol            *p_sym;
    t_clock             *p_clock;
    struct _messbox     *p_cnv;
}t_messbox_edit_proxy;

typedef struct _messbox_handle{
    t_pd            h_pd;
    struct _messbox *h_master;
    t_symbol       *h_bindsym;
    char            h_pathname[64], h_outlinetag[64];
    int             h_dragon;
    int             h_dragx, h_dragy;
    int             h_dx, h_dy;
}t_messbox_handle;

typedef struct _messbox{
    t_object         x_obj;
    t_canvas        *x_canvas;
    t_glist         *x_glist;
    t_symbol        *x_bind_sym;
    t_messbox_proxy *x_proxy;
    t_messbox_edit_proxy *x_edit_proxy;
    t_pd            *x_handle;
    t_symbol        *x_dollzero;
    int              x_flag;
    int              x_height;
    int              x_width;
    int              x_resizing;
    int              x_active;
    int              x_selected;
    int              x_edit;
    int              x_theme;
    char             x_fgcolor[8];
    unsigned int     x_fg[3];
    char             x_bgcolor[8];
    unsigned int     x_bg[3];
    int              x_font_size;
    int              x_zoom;
    t_symbol        *x_font_weight;
    char            *tcl_namespace;
    char            *x_cv_id;
    char            *frame_id;
    char            *text_id;
    char            *window_tag;
    char            *all_tag;
    char             x_tag_outline[32];
    char             x_tag_inlet[32];
    char             x_tag_outlet[32];
    char             x_tag_handle[32];
    t_symbol        *x_bindname;
    t_binbuf        *x_content; // copy of the text, survives widget destruction
}t_messbox;

static t_class *messbox_class;
static t_class *messbox_proxy_class;
static t_class *messbox_edit_proxy_class;
static t_class *messbox_handle_class;
static t_widgetbehavior messbox_widgetbehavior;

static void messbox_getrect(t_gobj *z, t_glist *owner, int *xp1, int *yp1, int *xp2, int *yp2);
static void messbox_dim(t_messbox *x, int w, int h);
static void messbox_insert(t_messbox *x, int ac, t_atom *av);

static int box_vis_check(t_messbox *x){
    return(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist));
}

static void messbox_get_bgcolor(t_messbox *x, char *buf, size_t size){
    if(x->x_theme)
        snprintf(buf, size, "#%06x", THISGUI->i_backgroundcolor);
    else
        snprintf(buf, size, "#%2.2x%2.2x%2.2x", x->x_bg[0], x->x_bg[1], x->x_bg[2]);
}

static void messbox_get_fgcolor(t_messbox *x, char *buf, size_t size){
    if(x->x_theme)
        snprintf(buf, size, "#%06x", THISGUI->i_foregroundcolor);
    else
        snprintf(buf, size, "#%2.2x%2.2x%2.2x", x->x_fg[0], x->x_fg[1], x->x_fg[2]);
}

static void messbox_config_iolets(t_messbox *x){
    if(!box_vis_check(x))
        return;
    pdgui_vmess(0, "crs rs", x->x_canvas,
        "itemconfigure", x->x_tag_inlet,
        "-state", x->x_edit ? "normal" : "hidden");
    pdgui_vmess(0, "crs rs", x->x_canvas,
        "itemconfigure", x->x_tag_outlet,
        "-state", x->x_edit ? "normal" : "hidden");
}

static void messbox_draw_handle(t_messbox *x, int state){
    t_messbox_handle *sh = (t_messbox_handle *)x->x_handle;
    if(sh->h_pathname[0])
        pdgui_vmess(0, "rs", "destroy", sh->h_pathname);
    if(state){
        unsigned int border = x->x_selected ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor;
        unsigned int bg = x->x_selected ? THISGUI->i_selectcolor : THISGUI->i_gopcolor;
        pdgui_vmess(0, "rs riri rkrkrk ri rr",
            "canvas", sh->h_pathname,
            "-width", HANDLE_SIZE, "-height", HANDLE_SIZE,
            "-bg", bg,
            "-highlightcolor", THISGUI->i_foregroundcolor,
            "-highlightbackground", border,
            "-highlightthickness", 1,
            "-cursor", "bottom_right_corner");
        int x1, y1, x2, y2;
        messbox_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        char *tags_handle[] = {x->x_tag_handle, x->all_tag};
        pdgui_vmess(0, "crr ii rs ri ri rs rS",
            x->x_canvas, "create", "window",
            x2 - HANDLE_SIZE + 1, y2 - HANDLE_SIZE + 1,
            "-anchor", "nw",
            "-width", HANDLE_SIZE,
            "-height", HANDLE_SIZE,
            "-window", sh->h_pathname,
            "-tags", 2, tags_handle);
        char buf[512];
        snprintf(buf, sizeof(buf),
            "bind %s <Button> {pdsend [concat %s _click 1 [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name,
            (unsigned long)x->x_canvas, (unsigned long)x->x_canvas);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            "bind %s <ButtonRelease> {pdsend [concat %s _click 0 [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name,
            (unsigned long)x->x_canvas, (unsigned long)x->x_canvas);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            "bind %s <Motion> {pdsend [concat %s _motion [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name,
            (unsigned long)x->x_canvas, (unsigned long)x->x_canvas);
        pdgui_vmess(buf, NULL);
    }
}

static void draw_box(t_messbox *x){
    int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
    char *tags_inlet[] = {x->x_tag_inlet, x->all_tag};
    char *tags_outlet[] = {x->x_tag_outlet, x->all_tag};
    const char *iostate = x->x_edit ? "normal" : "hidden";
    char bgcolor[20];
    messbox_get_bgcolor(x, bgcolor, sizeof(bgcolor));
    if(x->x_selected){
        pdgui_vmess(0, "crr iiii rk rr ri rr",
            x->x_canvas, "create", "rectangle",
            xpos, ypos, xpos+x->x_width*x->x_zoom, ypos+x->x_height*x->x_zoom,
            "-outline", THISGUI->i_selectcolor,
            "-fill", bgcolor, "-width", 2*x->x_zoom,
            "-tags", x->x_tag_outline);
        pdgui_vmess(0, "crr iiii rk rk rs rS",
            x->x_canvas, "create", "rectangle",
            xpos, ypos, xpos+(IOWIDTH*x->x_zoom), ypos+(IHEIGHT*x->x_zoom),
            "-fill", THISGUI->i_selectcolor, "-outline", THISGUI->i_selectcolor,
            "-state", iostate,
            "-tags", 2, tags_inlet);
        pdgui_vmess(0, "crr iiii rk rk rs rS",
            x->x_canvas, "create", "rectangle",
            xpos, ypos+x->x_height*x->x_zoom-1, xpos+IOWIDTH*x->x_zoom,
            ypos+x->x_height*x->x_zoom-IHEIGHT*x->x_zoom-1,
            "-fill", THISGUI->i_selectcolor, "-outline", THISGUI->i_selectcolor,
            "-state", iostate,
            "-tags", 2, tags_outlet);
    }
    else{
        pdgui_vmess(0, "crr iiii rk rr ri rr",
            x->x_canvas, "create", "rectangle",
            xpos, ypos, xpos+x->x_width*x->x_zoom, ypos+x->x_height*x->x_zoom,
            "-outline", THISGUI->i_foregroundcolor,
            "-fill", bgcolor, "-width", x->x_zoom,
            "-tags", x->x_tag_outline);
        pdgui_vmess(0, "crr iiii rk rk rs rS",
            x->x_canvas, "create", "rectangle",
            xpos, ypos, xpos+(IOWIDTH*x->x_zoom), ypos+(IHEIGHT*x->x_zoom),
            "-fill", THISGUI->i_foregroundcolor, "-outline", THISGUI->i_foregroundcolor,
            "-state", iostate,
            "-tags", 2, tags_inlet);
        pdgui_vmess(0, "crr iiii rk rk rs rS",
            x->x_canvas, "create", "rectangle",
            xpos, ypos+x->x_height*x->x_zoom-1, xpos+IOWIDTH*x->x_zoom,
            ypos+x->x_height*x->x_zoom-IHEIGHT*x->x_zoom-1,
            "-fill", THISGUI->i_foregroundcolor, "-outline", THISGUI->i_foregroundcolor,
            "-state", iostate,
            "-tags", 2, tags_outlet);
    }
}

static void erase_box(t_messbox *x){
    pdgui_vmess(0, "crs", x->x_canvas, "delete", x->x_tag_outline);
    pdgui_vmess(0, "crs", x->x_canvas, "delete", x->x_tag_inlet);
    pdgui_vmess(0, "crs", x->x_canvas, "delete", x->x_tag_outlet);
}

static void messbox_draw(t_messbox *x, t_glist *glist){
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    char buf[1024];
    char bgcolor[20], fgcolor[20];
    messbox_get_bgcolor(x, bgcolor, sizeof(bgcolor));
    messbox_get_fgcolor(x, fgcolor, sizeof(fgcolor));
    snprintf(buf, sizeof(buf), "namespace eval messbox%lx {}", (unsigned long)x);
    pdgui_vmess(buf, NULL);
    pdgui_vmess(0, "rr", "destroy", x->frame_id);
    pdgui_vmess(0, "rr", "frame", x->frame_id);
    snprintf(buf, sizeof(buf), "text %s -font {{%s} %d %s} -highlightthickness 0 -bg \"%s\" -fg \"%s\" -insertbackground \"%s\"",
        x->text_id, sys_font, x->x_font_size*x->x_zoom, x->x_font_weight->s_name,
        bgcolor, fgcolor, fgcolor);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "bindtags %s {pre%s Text %s . all}",
        x->text_id, x->text_id, x->text_id);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "bind pre%s <KeyPress-Return> {pdsend {%s bang}\nbreak}",
        x->text_id, x->x_bind_sym->s_name);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "bind pre%s <Shift-KeyPress-Return> {#}", x->text_id);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "pack %s -side left -fill both -expand 1", x->text_id);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "pack %s -side bottom -fill both -expand 1", x->frame_id);
    pdgui_vmess(buf, NULL);
    if(binbuf_getnatom(x->x_content)) // restore stored text into the new widget
        messbox_insert(x, binbuf_getnatom(x->x_content), binbuf_getvec(x->x_content));
    snprintf(buf, sizeof(buf),
        "bind %s <Button> {pdtk_canvas_mouse %s [expr %%X - [winfo rootx %s]] [expr %%Y - [winfo rooty %s]] %%b 0}",
        x->text_id, x->x_cv_id, x->x_cv_id, x->x_cv_id);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf),
        "bind %s <ButtonRelease> {pdtk_canvas_mouseup %s [expr %%X - [winfo rootx %s]] [expr %%Y - [winfo rooty %s]] %%b}",
        x->text_id, x->x_cv_id, x->x_cv_id, x->x_cv_id);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf),
        "bind %s <Button-2> {pdtk_canvas_rightclick %s [expr %%X - [winfo rootx %s]] [expr %%Y - [winfo rooty %s]] %%b}",
        x->text_id, x->x_cv_id, x->x_cv_id, x->x_cv_id);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf),
        "bind %s <Button-3> {pdtk_canvas_rightclick %s [expr %%X - [winfo rootx %s]] [expr %%Y - [winfo rooty %s]] %%b}",
        x->text_id, x->x_cv_id, x->x_cv_id, x->x_cv_id);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf),
        "bind %s <Shift-Button> {pdtk_canvas_mouse %s [expr %%X - [winfo rootx %s]] [expr %%Y - [winfo rooty %s]] %%b 1}",
        x->text_id, x->x_cv_id, x->x_cv_id, x->x_cv_id);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf),
        "bind %s <Motion> {pdtk_canvas_motion %s [expr %%X - [winfo rootx %s]] [expr %%Y - [winfo rooty %s]] 0}",
        x->text_id, x->x_cv_id, x->x_cv_id, x->x_cv_id);
    pdgui_vmess(buf, NULL);
    char *tags_window[] = {x->window_tag, x->all_tag};
    pdgui_vmess(0, "crr ii rr rr rS ri ri",
        x->x_canvas, "create", "window",
        xpos+BORDER, ypos+BORDER,
        "-anchor", "nw",
        "-window", x->frame_id,
        "-tags", 2, tags_window,
        "-width", x->x_width-BORDER*2,
        "-height", x->x_height-BORDER*2);
    draw_box(x);
}

static void messbox_erase(t_messbox* x){
    pdgui_vmess(0, "crs", x->x_canvas, "delete", x->x_tag_outline);
    pdgui_vmess(0, "crs", x->x_canvas, "delete", x->x_tag_inlet);
    pdgui_vmess(0, "crs", x->x_canvas, "delete", x->x_tag_outlet);
    pdgui_vmess(0, "rr", "destroy", x->frame_id);
    pdgui_vmess(0, "crs", x->x_canvas, "delete", x->all_tag);
}

static void messbox_getrect(t_gobj *z, t_glist *owner, int *xp1, int *yp1, int *xp2, int *yp2){
    t_messbox *x = (t_messbox*)z;
    *xp1 = text_xpix(&x->x_obj, owner);
    *yp1 = text_ypix(&x->x_obj, owner);
    *xp2 = *xp1 + x->x_width*x->x_zoom;
    *yp2 = *yp1 + x->x_height*x->x_zoom;
}

static void messbox_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_messbox *x = (t_messbox *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    if(glist_isvisible(glist)){
        char rsz_tag[32];
        pdgui_vmess(0, "crs ii", x->x_canvas, "move", x->all_tag, dx*x->x_zoom, dy*x->x_zoom);
        pdgui_vmess(0, "crs ii", x->x_canvas, "move", x->x_tag_outline, dx*x->x_zoom, dy*x->x_zoom);
        snprintf(rsz_tag, sizeof(rsz_tag), "%lxRSZ", (unsigned long)x);
        pdgui_vmess(0, "crs ii", x->x_canvas, "move", rsz_tag, dx*x->x_zoom, dy*x->x_zoom);
        canvas_fixlinesfor(glist, (t_text*)x);
    }
}

static void messbox_key(void *z, t_floatarg f);

static void messbox_select(t_gobj *z, t_glist *glist, int state){
    t_messbox *x = (t_messbox *)z;
    if(state){
        pdgui_vmess(0, "rrrkrrrr",
            x->text_id, "configure", "-fg", THISGUI->i_selectcolor,
            "-state", "disabled", "-cursor", "$cursor_editmode_nothing");
        x->x_selected = 1;
        glist = NULL;
        pdgui_vmess(0, "crs rk ri", x->x_canvas, "itemconfigure", x->x_tag_outline,
            "-outline", THISGUI->i_selectcolor, "-width", 2*x->x_zoom);
        pdgui_vmess(0, "crs rk rk", x->x_canvas, "itemconfigure", x->x_tag_inlet,
            "-fill", THISGUI->i_selectcolor, "-outline", THISGUI->i_selectcolor);
        pdgui_vmess(0, "crs rk rk", x->x_canvas, "itemconfigure", x->x_tag_outlet,
            "-fill", THISGUI->i_selectcolor, "-outline", THISGUI->i_selectcolor);
    }
    else{
        char fgcolor[20];
        messbox_get_fgcolor(x, fgcolor, sizeof(fgcolor));
        pdgui_vmess(0, "rrrrrrrrrr",
            x->text_id, "configure", "-fg", fgcolor,
            "-state", "normal", "-cursor", "xterm",
            "-insertbackground", fgcolor);
        x->x_selected = 0;
        pdgui_vmess(0, "crs rk ri", x->x_canvas, "itemconfigure", x->x_tag_outline,
            "-outline", THISGUI->i_foregroundcolor, "-width", x->x_zoom);
        pdgui_vmess(0, "crs rk rk", x->x_canvas, "itemconfigure", x->x_tag_inlet,
            "-fill", THISGUI->i_foregroundcolor, "-outline", THISGUI->i_foregroundcolor);
        pdgui_vmess(0, "crs rk rk", x->x_canvas, "itemconfigure", x->x_tag_outlet,
            "-fill", THISGUI->i_foregroundcolor, "-outline", THISGUI->i_foregroundcolor);
        messbox_key(z, 0);
    }
    if(x->x_edit)
        messbox_draw_handle(x, 1);
}

static void messbox_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void messbox_vis(t_gobj *z, t_glist *gl, int vis){
    t_messbox *x = (t_messbox*)z;
    t_messbox_handle *sh = (t_messbox_handle *)x->x_handle;
    sprintf(sh->h_pathname, ".x%lx.h%lx", (unsigned long)x->x_canvas, (unsigned long)sh);
    if(x->x_edit)
        pdgui_vmess(0, "rs", "destroy", sh->h_pathname);
    if(vis){
        messbox_draw(x, gl);
        messbox_draw_handle(x, x->x_edit);
    }
    else
        messbox_erase(x);
}

static void messbox_list(t_messbox* x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    char cmd[1024];
    if(!ac){
        snprintf(cmd, sizeof(cmd),
            "pdsend \"%s [string map {\\$0 %s} [%s get 0.0 end]]\"",
            x->x_proxy->x_bind_sym->s_name, x->x_dollzero->s_name, x->text_id);
        pdgui_vmess(cmd, NULL);
        return;
    }
    char buf[MAXPDSTRING];
    sprintf(buf, "\\$0 %s", x->x_dollzero->s_name);
    char symbuf[32];
    size_t length = MAXPDSTRING - 1, tmplength;
    length -= strlen(buf);
    for(int i = 0; i < ac; i++) {
        sprintf(symbuf, " \\$%i ", i+1);
        tmplength = strlen(symbuf);
        length -= tmplength;
        if(length <= 0)
            break;
        strncat(buf, symbuf, tmplength);
        atom_string(av + i, symbuf, 32);
        tmplength = strlen(symbuf);
        length -= tmplength;
        if(length <= 0)
            break;
        strncat(buf, symbuf, tmplength);
    }
    snprintf(cmd, sizeof(cmd),
        "pdsend \"%s [string map {%s} [%s get 0.0 end]]\"",
        x->x_proxy->x_bind_sym->s_name, buf, x->text_id);
    pdgui_vmess(cmd, NULL);
}

static void messbox_proxy_anything(t_messbox_proxy* x, t_symbol *s, int ac,
    t_atom *av){
    outlet_anything(x->p_master->x_obj.ob_outlet, s, ac, av);
}

static void messbox_edit_proxy_free(t_messbox_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_messbox_edit_proxy *messbox_edit_proxy_new(t_messbox *x, t_symbol *s){
    t_messbox_edit_proxy *p = (t_messbox_edit_proxy*)pd_new(messbox_edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)messbox_edit_proxy_free);
    return(p);
}

static void messbox_edit_proxy_any(t_messbox_edit_proxy *p, t_symbol *s, int ac, t_atom *av){
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
            messbox_config_iolets(p->p_cnv);
            if(box_vis_check(p->p_cnv))
                messbox_draw_handle(p->p_cnv, edit);
        }
    }
}

static void messbox_insert(t_messbox *x, int ac, t_atom *av){
    char buf[40];
    size_t length;
    pdgui_vmess(0, "rrrr", x->text_id, "configure", "-state", "normal");
    if(ac){
        size_t pos;
        for(int i = 0; i < ac; i++){
            t_symbol *sym = atom_getsymbolarg(i, ac, av);
            char cmd[128];
            if(sym == &s_){
                snprintf(cmd, sizeof(cmd), "%s insert end \"%g \"",
                    x->text_id, atom_getfloatarg(i, ac , av));
                pdgui_vmess(cmd, NULL);
            }
            else{
                int j = 0;
                length = 39;
                for(pos = 0; pos < strlen(sym->s_name); pos++) {
                    char c = sym->s_name[pos];
                    if(c == '\\' || c == '[' || c == '$' || c == ';') {
                        length--;
                        if(length <= 0) break;
                        buf[j++] = '\\';
                    }
                    length--;
                    if(length <= 0) break;
                    buf[j++] = c;
                }
                buf[j] = '\0';
                if(sym->s_name[pos-1] == ';')
                    snprintf(cmd, sizeof(cmd), "%s insert end %s\\n", x->text_id, buf);
                else
                    snprintf(cmd, sizeof(cmd), "%s insert end \"%s \"", x->text_id, buf);
                pdgui_vmess(cmd, NULL);
            }
        }
        char cmd2[128];
        snprintf(cmd2, sizeof(cmd2), "%s yview end-2char", x->text_id);
        pdgui_vmess(cmd2, NULL);
    }
    if(!x->x_active)
        pdgui_vmess(0, "rrrr", x->text_id, "configure", "-state", "disabled");
}

static void messbox_append(t_messbox* x,  t_symbol *s, int ac, t_atom *av){
    (void)s;
    binbuf_add(x->x_content, ac, av);
    if(!box_vis_check(x))
        return;
    messbox_insert(x, ac, av);
}

static void messbox_set(t_messbox *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    binbuf_clear(x->x_content);
    binbuf_add(x->x_content, ac, av);
    if(!box_vis_check(x))
        return;
    pdgui_vmess(0, "rrrr", x->text_id, "configure", "-state", "normal");
    pdgui_vmess(0, "rrrr", x->text_id, "delete", "0.0", "end");
    messbox_insert(x, ac, av);
}

static void messbox_parse_hex(unsigned int *color, const char *hex){
    if(hex && hex[0] == '#' && strlen(hex) == 7){
        char pair[3] = {0};
        pair[0] = hex[1]; pair[1] = hex[2];
        color[0] = (unsigned int)strtol(pair, NULL, 16);
        pair[0] = hex[3]; pair[1] = hex[4];
        color[1] = (unsigned int)strtol(pair, NULL, 16);
        pair[0] = hex[5]; pair[1] = hex[6];
        color[2] = (unsigned int)strtol(pair, NULL, 16);
    }
}

static void messbox_send_color_validate(t_messbox *x, const char *color, int which){
    char buf[512];
    snprintf(buf, sizeof(buf), "set color_name {%s}", color);
    pdgui_vmess(buf, NULL);
    pdgui_vmess("set color_escaped [string map {{ } {\\ }} $color_name]", NULL);
    pdgui_vmess("set hex_color [color2hex $color_name]", NULL);
    snprintf(buf, sizeof(buf),
        "if {$hex_color eq \"\"} "
        "{pdsend \"%s _validate 0 $color_escaped %d\"} "
        "else {pdsend \"%s _validate 1 $hex_color %d\"}",
        x->x_bindname->s_name, which,
        x->x_bindname->s_name, which);
    pdgui_vmess(buf, NULL);
}

static void messbox_validate(t_messbox *x, t_floatarg v, t_symbol *color, t_floatarg which){
    if(!(int)v){
        pd_error(0, "[messbox] invalid color '%s'", color->s_name);
        return;
    }
    if((int)which == 0){
        messbox_parse_hex(x->x_bg, color->s_name);
        sprintf(x->x_bgcolor, "#%2.2x%2.2x%2.2x", x->x_bg[0], x->x_bg[1], x->x_bg[2]);
        if(box_vis_check(x)){
            char bgcolor[20];
            messbox_get_bgcolor(x, bgcolor, sizeof(bgcolor));
            pdgui_vmess(0, "rrrr", x->text_id, "configure", "-background", bgcolor);
            pdgui_vmess(0, "crs rs", x->x_canvas, "itemconfigure", x->x_tag_outline, "-fill", bgcolor);
        }
    }
    else{
        messbox_parse_hex(x->x_fg, color->s_name);
        sprintf(x->x_fgcolor, "#%2.2x%2.2x%2.2x", x->x_fg[0], x->x_fg[1], x->x_fg[2]);
        if(box_vis_check(x) && !x->x_selected){
            char fgcolor[20];
            messbox_get_fgcolor(x, fgcolor, sizeof(fgcolor));
            pdgui_vmess(0, "rrrrrr", x->text_id, "configure", "-foreground", fgcolor, "-insertbackground", fgcolor);
        }
    }
}

void messbox_bgcolor(t_messbox *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    x->x_theme = 0;
    if(av->a_type == A_SYMBOL){
        messbox_send_color_validate(x, atom_getsymbol(av)->s_name, 0);
        return;
    }
    if(ac < 3)
        return;
    if(av[0].a_type == A_FLOAT && av[1].a_type == A_FLOAT && av[2].a_type == A_FLOAT){
        float r = atom_getfloatarg(0, ac, av);
        float g = atom_getfloatarg(1, ac, av);
        float b = atom_getfloatarg(2, ac, av);
        x->x_bg[0] = r < 0 ? 0 : r > 255 ? 255 : (int)r;
        x->x_bg[1] = g < 1 ? 0 : g > 255 ? 255 : (int)g;
        x->x_bg[2] = b < 2 ? 0 : b > 255 ? 255 : (int)b;
        sprintf(x->x_bgcolor, "#%2.2x%2.2x%2.2x", x->x_bg[0], x->x_bg[1], x->x_bg[2]);
        if(box_vis_check(x)){
            char bgcolor[20];
            messbox_get_bgcolor(x, bgcolor, sizeof(bgcolor));
            pdgui_vmess(0, "rrrr", x->text_id, "configure", "-background", bgcolor);
            pdgui_vmess(0, "crs rs", x->x_canvas, "itemconfigure", x->x_tag_outline, "-fill", bgcolor);
        }
    }
}

void messbox_fgcolor(t_messbox *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    x->x_theme = 0;
    if(av->a_type == A_SYMBOL){
        messbox_send_color_validate(x, atom_getsymbol(av)->s_name, 1);
        return;
    }
    if(ac < 3)
        return;
    if(av[0].a_type == A_FLOAT && av[1].a_type == A_FLOAT && av[2].a_type == A_FLOAT){
        float r = atom_getfloatarg(0, ac, av);
        float g = atom_getfloatarg(1, ac, av);
        float b = atom_getfloatarg(2, ac, av);
        x->x_fg[0] = r < 0 ? 0 : r > 255 ? 255 : (int)r;
        x->x_fg[1] = g < 1 ? 0 : g > 255 ? 255 : (int)g;
        x->x_fg[2] = b < 2 ? 0 : b > 255 ? 255 : (int)b;
        sprintf(x->x_fgcolor, "#%2.2x%2.2x%2.2x", x->x_fg[0], x->x_fg[1], x->x_fg[2]);
        if(box_vis_check(x) && !x->x_selected){
            char fgcolor[20];
            messbox_get_fgcolor(x, fgcolor, sizeof(fgcolor));
            pdgui_vmess(0, "rrrrrr", x->text_id, "configure", "-foreground", fgcolor, "-insertbackground", fgcolor);
        }
    }
}

static void messbox_colors(t_messbox *x, t_symbol *bg, t_symbol *fg){
    t_atom at[1];
    SETSYMBOL(at, bg);
    messbox_bgcolor(x, NULL, 1, at);
    SETSYMBOL(at, fg);
    messbox_fgcolor(x, NULL, 1, at);
}

static void messbox_theme(t_messbox *x, t_floatarg f){
    int theme = (int)(f != 0);
    if(theme == x->x_theme)
        return;
    x->x_theme = theme;
    if(box_vis_check(x)){
        char bgcolor[20], fgcolor[20];
        messbox_get_bgcolor(x, bgcolor, sizeof(bgcolor));
        messbox_get_fgcolor(x, fgcolor, sizeof(fgcolor));
        pdgui_vmess(0, "rrrr", x->text_id, "configure", "-background", bgcolor);
        if(!x->x_selected)
            pdgui_vmess(0, "rrrrrr", x->text_id, "configure", "-foreground", fgcolor, "-insertbackground", fgcolor);
        pdgui_vmess(0, "crs rs", x->x_canvas, "itemconfigure", x->x_tag_outline, "-fill", bgcolor);
    }
}

static void messbox_bold(t_messbox *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(av[0].a_type == A_FLOAT){
        float bold = atom_getfloatarg(0, ac, av);
        x->x_font_weight = (int)(bold != 0) ? gensym("bold") : gensym("normal");
        if(box_vis_check(x)){
            char buf[256];
            snprintf(buf, sizeof(buf), "%s configure -font {{%s} %d %s}",
                x->text_id, sys_font, x->x_font_size*x->x_zoom, x->x_font_weight->s_name);
            pdgui_vmess(buf, NULL);
        }
    }
}

static void messbox_fontsize(t_messbox *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(av[0].a_type == A_FLOAT){
        float font_size = atom_getfloatarg(0, ac, av);
        x->x_font_size = font_size < 8 ? 8 : (int)font_size;
        if(box_vis_check(x)){
            char buf[256];
            snprintf(buf, sizeof(buf), "%s configure -font {{%s} %d %s}",
                x->text_id, sys_font, x->x_font_size*x->x_zoom, x->x_font_weight->s_name);
            pdgui_vmess(buf, NULL);
        }
    }
}

static void messbox_dim(t_messbox *x, int w, int h){
    x->x_width = w < MIN_WIDTH ? MIN_WIDTH : w;
    x->x_height = h < MIN_HEIGHT ? MIN_HEIGHT : h;
    if(glist_isvisible(x->x_glist)){
        pdgui_vmess(0, "crs ri ri", x->x_canvas, "itemconfigure", x->window_tag,
            "-width", x->x_width-BORDER*2, "-height", x->x_height-BORDER*2);
        canvas_fixlinesfor(x->x_glist, (t_text *)x);
        erase_box(x), draw_box(x);
        if(x->x_edit)
            messbox_draw_handle(x, 1);
    }
}

static void messbox_size(t_messbox *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(ac >= 2 && av[0].a_type == A_FLOAT && av[1].a_type == A_FLOAT)
        messbox_dim(x, (int)atom_getfloatarg(0, ac, av), (int)atom_getfloatarg(1, ac, av));
}

static void messbox_key(void *z, t_floatarg f){
    if(f == 0){
        t_messbox *x = z;
        pdgui_vmess(0, "rrrr", x->text_id, "configure", "-state", "disabled");
        pdgui_vmess(0, "rr", "focus", x->x_cv_id);
        x->x_active = 0;
        pdgui_vmess(0, "crs ri", x->x_canvas, "itemconfigure", x->x_tag_outline,
            "-width", x->x_selected ? 2*x->x_zoom : x->x_zoom);
    }
}

static int messbox_click(t_gobj *z, t_glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit){
    xpix = ypix = alt = dbl = shift = 0;
    if(doit){
        t_messbox *x = (t_messbox *)z;
        x->x_active = 1;
        pdgui_vmess(0, "crs ri", x->x_canvas, "itemconfigure", x->x_tag_outline,
            "-width", 2*x->x_zoom);
        pdgui_vmess(0, "rrrr", x->text_id, "configure", "-state", "normal");
        pdgui_vmess(0, "rr", "focus", x->text_id);
        glist_grab(glist, (t_gobj *)x, 0, (t_glistkeyfn)messbox_key, 0, 0);
    }
    return(1);
}

static void messbox_properties(t_gobj *z, t_glist *owner){
    (void)owner;
    t_messbox *x = (t_messbox *)z;
    char bgcolor[20], fgcolor[20];
    snprintf(bgcolor, sizeof(bgcolor), "#%2.2x%2.2x%2.2x", x->x_bg[0], x->x_bg[1], x->x_bg[2]);
    snprintf(fgcolor, sizeof(fgcolor), "#%2.2x%2.2x%2.2x", x->x_fg[0], x->x_fg[1], x->x_fg[2]);
    char pd_bg[32], pd_fg[32];
    snprintf(pd_bg, sizeof(pd_bg), "#%06X", THISGUI->i_backgroundcolor);
    snprintf(pd_fg, sizeof(pd_fg), "#%06X", THISGUI->i_foregroundcolor);
    pdgui_stub_vnew(&x->x_obj.ob_pd, "messbox_dialog", x,
        "ii ii ssss i",
        x->x_width, x->x_height,
        x->x_font_size, (int)(x->x_font_weight == gensym("bold")),
        bgcolor, fgcolor, pd_bg, pd_fg,
        x->x_theme);
}

static void messbox_apply(t_messbox *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    char bgstr[20], fgstr[20];
    snprintf(bgstr, sizeof(bgstr), "#%2.2x%2.2x%2.2x", x->x_bg[0], x->x_bg[1], x->x_bg[2]);
    snprintf(fgstr, sizeof(fgstr), "#%2.2x%2.2x%2.2x", x->x_fg[0], x->x_fg[1], x->x_fg[2]);
    t_atom undo[7];
    SETFLOAT(undo+0, x->x_width);
    SETFLOAT(undo+1, x->x_height);
    SETFLOAT(undo+2, x->x_font_size);
    SETFLOAT(undo+3, x->x_font_weight == gensym("bold"));
    SETSYMBOL(undo+4, gensym(bgstr));
    SETSYMBOL(undo+5, gensym(fgstr));
    SETFLOAT(undo+6, x->x_theme);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dialog"), 7, undo, ac, av);

    int w = atom_getintarg(0, ac, av);
    int h = atom_getintarg(1, ac, av);
    int fontsize = atom_getintarg(2, ac, av);
    int bold = atom_getintarg(3, ac, av);
    t_symbol *bg = atom_getsymbolarg(4, ac, av);
    t_symbol *fg = atom_getsymbolarg(5, ac, av);
    int theme = atom_getintarg(6, ac, av);

    x->x_font_size = fontsize < 8 ? 8 : fontsize;
    x->x_font_weight = bold ? gensym("bold") : gensym("normal");
    x->x_theme = (theme != 0);
    messbox_parse_hex(x->x_bg, bg->s_name);
    messbox_parse_hex(x->x_fg, fg->s_name);
    sprintf(x->x_bgcolor, "#%2.2x%2.2x%2.2x", x->x_bg[0], x->x_bg[1], x->x_bg[2]);
    sprintf(x->x_fgcolor, "#%2.2x%2.2x%2.2x", x->x_fg[0], x->x_fg[1], x->x_fg[2]);

    messbox_dim(x, w, h);

    if(box_vis_check(x)){
        char buf[256], bgcolor_str[20], fgcolor_str[20];
        messbox_get_bgcolor(x, bgcolor_str, sizeof(bgcolor_str));
        messbox_get_fgcolor(x, fgcolor_str, sizeof(fgcolor_str));
        snprintf(buf, sizeof(buf), "%s configure -font {{%s} %d %s} -background \"%s\"",
            x->text_id, sys_font, x->x_font_size*x->x_zoom, x->x_font_weight->s_name, bgcolor_str);
        pdgui_vmess(buf, NULL);
        if(!x->x_selected)
            pdgui_vmess(0, "rrrrrr", x->text_id, "configure", "-foreground", fgcolor_str, "-insertbackground", fgcolor_str);
    }

    canvas_dirty(x->x_glist, 1);
}

static void handle__click_callback(t_messbox_handle *sh, t_floatarg f, t_floatarg xarg, t_floatarg yarg){
    int click = (int)f;
    t_messbox *x = sh->h_master;
    if(sh->h_dragon && click == 0){
        char buf[128];
        snprintf(buf, sizeof(buf), ".x%lx.c delete %s", (unsigned long)x->x_canvas, sh->h_outlinetag);
        pdgui_vmess(buf, NULL);
        int new_w = x->x_width + sh->h_dx;
        int new_h = x->x_height + sh->h_dy;
        if(new_w != x->x_width || new_h != x->x_height){
            t_atom undo[2], redo[2];
            SETFLOAT(undo+0, x->x_width);
            SETFLOAT(undo+1, x->x_height);
            SETFLOAT(redo+0, new_w);
            SETFLOAT(redo+1, new_h);
            pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dim"), 2, undo, 2, redo);
            messbox_dim(x, new_w, new_h);
            canvas_dirty(x->x_glist, 1);
        }
        if(x->x_edit)
            messbox_draw_handle(x, 1);
    }
    else if(!sh->h_dragon && click){
        int x1, y1, x2, y2;
        messbox_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        pdgui_vmess(0, "crr iiii rk ri rs",
            x->x_canvas, "create", "rectangle",
            x1, y1, x2, y2,
            "-outline", THISGUI->i_selectcolor,
            "-width", 2,
            "-tags", sh->h_outlinetag);
        pdgui_vmess(0, "rr rk", sh->h_pathname, "configure",
            "-highlightcolor", THISGUI->i_selectcolor);
        sh->h_dragx = (int)xarg;
        sh->h_dragy = (int)yarg;
        sh->h_dx = sh->h_dy = 0;
    }
    sh->h_dragon = click;
}

static void handle__motion_callback(t_messbox_handle *sh, t_floatarg xarg, t_floatarg yarg){
    if(sh->h_dragon){
        t_messbox *x = sh->h_master;
        int dx = ((int)xarg - sh->h_dragx) / (x->x_zoom > 0 ? x->x_zoom : 1);
        int dy = ((int)yarg - sh->h_dragy) / (x->x_zoom > 0 ? x->x_zoom : 1);
        if(x->x_width + dx < MIN_WIDTH)
            dx = MIN_WIDTH - x->x_width;
        if(x->x_height + dy < MIN_HEIGHT)
            dy = MIN_HEIGHT - x->x_height;
        int x1, y1, x2, y2;
        messbox_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        char buf[128];
        snprintf(buf, sizeof(buf), ".x%lx.c coords %s %d %d %d %d",
            (unsigned long)x->x_canvas, sh->h_outlinetag, x1, y1,
            x2 + dx * x->x_zoom, y2 + dy * x->x_zoom);
        pdgui_vmess(buf, NULL);
        sh->h_dx = dx;
        sh->h_dy = dy;
    }
}

static void messbox_save(t_gobj *z, t_binbuf *b){
    t_messbox *x = (t_messbox *)z;
    binbuf_addv(b, "ssiisiiiiiiiiiii;", &s__X, gensym("obj"),
        x->x_obj.te_xpix, x->x_obj.te_ypix, atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)),
        x->x_width, x->x_height, x->x_bg[0], x->x_bg[1], x->x_bg[2],
        x->x_fg[0], x->x_fg[1], x->x_fg[2],
        x->x_font_weight == gensym("bold"), x->x_font_size, x->x_theme);
}

static void messbox_zoom(t_messbox *x, t_floatarg zoom){
    x->x_zoom = (int)zoom;
}

static void messbox_free(t_messbox *x){
    binbuf_free(x->x_content);
    pd_unbind(&x->x_obj.ob_pd, x->x_bind_sym);
    pd_unbind(&x->x_obj.ob_pd, x->x_bindname);
    pd_unbind(&x->x_proxy->p_ob.ob_pd, x->x_proxy->x_bind_sym);
    pd_free((t_pd *)x->x_proxy);
    if(x->x_edit_proxy){
        x->x_edit_proxy->p_cnv = NULL;
        clock_delay(x->x_edit_proxy->p_clock, 0);
    }
    if(x->x_handle){
        pd_unbind(x->x_handle, ((t_messbox_handle *)x->x_handle)->h_bindsym);
        pd_free(x->x_handle);
    }
}

static void *messbox_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_messbox *x = (t_messbox *)pd_new(messbox_class);
    char buf[MAXPDSTRING];
    x->x_glist = canvas_getcurrent();
    x->x_canvas = glist_getcanvas(x->x_glist);
    x->x_zoom = x->x_glist->gl_zoom;
    x->x_dollzero = binbuf_realizedollsym(gensym("$0"), 0, 0, 0);
    x->x_content = binbuf_new();
    if(!(x->x_proxy = (t_messbox_proxy *)pd_new(messbox_proxy_class)))
        return(0);
    x->x_proxy->p_master = x;

    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_glist);
    x->x_edit_proxy = messbox_edit_proxy_new(x, gensym(buf));
    x->x_edit = x->x_glist->gl_edit;

    sprintf(buf,".x%lx.c", (long unsigned int) x->x_canvas);
    x->x_cv_id = getbytes(strlen(buf)+1);
    strcpy(x->x_cv_id, buf);
    sprintf(buf,"%s.frame%lx", x->x_cv_id, (long unsigned int)x);
    x->frame_id = getbytes(strlen(buf)+1);
    strcpy(x->frame_id, buf);
    sprintf(buf,"%s.text%lx", x->frame_id, (long unsigned int)x);
    x->text_id = getbytes(strlen(buf)+1);
    strcpy(x->text_id, buf);
    sprintf(buf,"%s.window%lx", x->x_cv_id, (long unsigned int)x);
    x->window_tag = getbytes(strlen(buf)+1);
    strcpy(x->window_tag, buf);
    sprintf(buf,"all%lx", (long unsigned int)x);
    x->all_tag = getbytes(strlen(buf)+1);
    strcpy(x->all_tag, buf);
    sprintf(x->x_tag_outline, "%lx_outline", (unsigned long)x);
    sprintf(x->x_tag_inlet, "%lx_inlet", (unsigned long)x);
    sprintf(x->x_tag_outlet, "%lx_outlet", (unsigned long)x);

    sprintf(buf, "messbox%lx", (long unsigned int)x);
    x->tcl_namespace = getbytes(strlen(buf)+1);
    strcpy(x->tcl_namespace, buf);
    sprintf(buf,"#%s", x->tcl_namespace);
    x->x_bind_sym = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_bind_sym);
    sprintf(buf, "#%s_proxy", x->tcl_namespace);
    x->x_proxy->x_bind_sym = gensym(buf);
    pd_bind(&x->x_proxy->p_ob.ob_pd, x->x_proxy->x_bind_sym);
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindname = gensym(buf));
    x->x_font_size = glist_getfont(x->x_glist);
    x->x_selected = x->x_resizing = x->x_active = x->x_flag = 0;
    x->x_theme = 0;
    int w = x->x_font_size * 15, h = x->x_font_size * 5;
    int weight = 0;
    x->x_bg[0] = x->x_bg[1] = x->x_bg[2] = 235;
    x->x_fg[0] = x->x_fg[1] = x->x_fg[2] = 0;
    if(ac){
        if((ac == 10 || ac == 11) && av->a_type == A_FLOAT){
               w = atom_getfloatarg(0, ac, av);
               h = atom_getfloatarg(1, ac, av);
               x->x_bg[0] = (unsigned int)atom_getfloatarg(2, ac, av);
               x->x_bg[1] = (unsigned int)atom_getfloatarg(3, ac, av);
               x->x_bg[2] = (unsigned int)atom_getfloatarg(4, ac, av);
               x->x_fg[0] = (unsigned int)atom_getfloatarg(5, ac, av);
               x->x_fg[1] = (unsigned int)atom_getfloatarg(6, ac, av);
               x->x_fg[2] = (unsigned int)atom_getfloatarg(7, ac, av);
               weight = (int)(atom_getfloatarg(8, ac, av));
               x->x_font_size = (int)(atom_getfloatarg(9, ac, av));
               if(ac == 11)
                   x->x_theme = (int)(atom_getfloatarg(10, ac, av));
           }
           else{
               t_symbol *sym;
               for(int i = 0; i < ac; i++){
                   if(av[i].a_type == A_SYMBOL)
                       sym = av[i].a_w.w_symbol;
                   else goto errstate;
                   if(sym == gensym("-size")){
                       if((ac-i) >= 3){
                           x->x_flag = 1, i++;
                           if(av[i].a_type == A_FLOAT){
                               w = av[i].a_w.w_float, i++;
                               if(av[i].a_type == A_FLOAT)
                                   h = av[i].a_w.w_float;
                               else goto errstate;
                            }
                            else goto errstate;
                        }
                        else goto errstate;
                    }
                    else if(sym == gensym("-fontsize")){
                        if((ac-i) >= 2){
                            x->x_flag = 1, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_font_size = av[i].a_w.w_float;
                            else goto errstate;
                        }
                        else goto errstate;                    }
                    else if(sym == gensym("-bold"))
                        x->x_flag = weight = 1;
                    else if(sym == gensym("-theme"))
                        x->x_flag = 1, x->x_theme = 1;
                    else if(sym == gensym("-fgcolor")){
                        if((ac-i) >= 4){
                            x->x_flag = 1, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_fg[0] = (unsigned char)av[i].a_w.w_float, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_fg[1] = (unsigned char)av[i].a_w.w_float, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_fg[2] = (unsigned char)av[i].a_w.w_float;
                            else goto errstate;
                        }
                        else goto errstate;
                    }
                    else if(sym == gensym("-bgcolor")){
                        if((ac-i) >= 4){
                            x->x_flag = 1, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_bg[0] = (unsigned char)av[i].a_w.w_float, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_bg[1] = (unsigned char)av[i].a_w.w_float, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_bg[2] = (unsigned char)av[i].a_w.w_float;
                            else
                                goto errstate;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
           }
    }
    x->x_width = w < MIN_WIDTH ? MIN_WIDTH : (int)w;
    x->x_height = w < MIN_HEIGHT ? MIN_HEIGHT : (int)h;
    if(x->x_font_size < 8)
        x->x_font_size = 8;
    sprintf(x->x_bgcolor, "#%2.2x%2.2x%2.2x", x->x_bg[0], x->x_bg[1], x->x_bg[2]);
    sprintf(x->x_fgcolor, "#%2.2x%2.2x%2.2x", x->x_fg[0], x->x_fg[1], x->x_fg[2]);
    x->x_font_weight = weight ? gensym("bold") : gensym("normal");

    sprintf(x->x_tag_handle, "%pMHANDLE", x);
    x->x_handle = pd_new(messbox_handle_class);
    t_messbox_handle *sh = (t_messbox_handle *)x->x_handle;
    sh->h_master = x;
    sh->h_dragon = 0;
    sh->h_pathname[0] = 0;
    sh->h_dx = sh->h_dy = 0;
    char hbuf[64];
    sprintf(hbuf, "_mh%lx", (unsigned long)sh);
    pd_bind(x->x_handle, sh->h_bindsym = gensym(hbuf));
    sprintf(sh->h_outlinetag, "mh%lx", (unsigned long)sh);

    outlet_new(&x->x_obj, &s_float);
    return(x);
    errstate:
        pd_error(x, "[messbox]: improper creation arguments");
        return(NULL);
}

void messbox_setup(void){
    messbox_class = class_new(gensym("messbox"), (t_newmethod)messbox_new,
        (t_method)messbox_free, sizeof(t_messbox), 0, A_GIMME, 0);
    messbox_proxy_class = class_new(0, 0, 0, sizeof(t_messbox_proxy),
        CLASS_PD | CLASS_NOINLET, 0);
    messbox_edit_proxy_class = class_new(0, 0, 0, sizeof(t_messbox_edit_proxy),
        CLASS_PD | CLASS_NOINLET, 0);
    class_addanything(messbox_edit_proxy_class, messbox_edit_proxy_any);
    messbox_handle_class = class_new(gensym("_messbox_handle"), 0, 0,
        sizeof(t_messbox_handle), CLASS_PD, 0);
    class_addmethod(messbox_handle_class, (t_method)handle__click_callback,
        gensym("_click"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(messbox_handle_class, (t_method)handle__motion_callback,
        gensym("_motion"), A_FLOAT, A_FLOAT, 0);
    class_addlist(messbox_class, (t_method)messbox_list);
    class_addanything(messbox_proxy_class, (t_method)messbox_proxy_anything);
    class_addmethod(messbox_class, (t_method)messbox_size, gensym("dim"), A_GIMME, 0);
    class_addmethod(messbox_class, (t_method)messbox_fontsize, gensym("fontsize"), A_GIMME, 0);
    class_addmethod(messbox_class, (t_method)messbox_bold, gensym("bold"), A_GIMME, 0);
    class_addmethod(messbox_class, (t_method)messbox_set, gensym("set"), A_GIMME, 0);
    class_addmethod(messbox_class, (t_method)messbox_append, gensym("append"), A_GIMME, 0);
    class_addmethod(messbox_class, (t_method)messbox_bgcolor, gensym("bgcolor"), A_GIMME, 0);
    class_addmethod(messbox_class, (t_method)messbox_fgcolor, gensym("fgcolor"), A_GIMME, 0);
    class_addmethod(messbox_class, (t_method)messbox_colors, gensym("colors"), A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(messbox_class, (t_method)messbox_theme, gensym("theme"), A_FLOAT, 0);
    class_addmethod(messbox_class, (t_method)messbox_apply, gensym("dialog"), A_GIMME, 0);
    class_addmethod(messbox_class, (t_method)messbox_validate, gensym("_validate"),
        A_FLOAT, A_SYMBOL, A_FLOAT, 0);
    class_addmethod(messbox_class, (t_method)messbox_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(messbox_class, (t_method)messbox_zoom, gensym("zoom"), A_CANT, 0);
    class_setwidget(messbox_class, &messbox_widgetbehavior);
    class_setsavefn(messbox_class, messbox_save);
    class_setpropertiesfn(messbox_class, messbox_properties);
    messbox_widgetbehavior.w_getrectfn  = messbox_getrect;
    messbox_widgetbehavior.w_displacefn = messbox_displace;
    messbox_widgetbehavior.w_selectfn   = messbox_select;
    messbox_widgetbehavior.w_activatefn = NULL;
    messbox_widgetbehavior.w_deletefn   = messbox_delete;
    messbox_widgetbehavior.w_visfn      = messbox_vis;
    messbox_widgetbehavior.w_clickfn    = (t_clickfn)messbox_click;
    #include "../Extra/else_tcl_utils.h"
    pdgui_vmess(else_tcl_utils, NULL);
    #include "../Extra/messbox_dialog.h"
    pdgui_vmess(messbox_dialog_tcl, NULL);
}
