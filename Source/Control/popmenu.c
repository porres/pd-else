//

#include <m_pd.h>
#include <g_canvas.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ITEMS 1024
#define DEF_FONTSIZE 11

#if __APPLE__
char def_font[100] = "Menlo";
#else
char def_font[100] = "DejaVu Sans Mono";
#endif

static t_class *menu_class, *edit_proxy_class;
static t_widgetbehavior menu_widgetbehavior;

typedef struct _edit_proxy{
    t_object      p_obj;
    t_symbol     *p_sym;
    t_clock      *p_clock;
    struct _menu *p_cnv;
}t_edit_proxy;

typedef struct _menu{
    t_object        x_obj;
    t_edit_proxy   *x_proxy;
    t_canvas       *x_cv;
    t_glist        *x_glist;
    int             x_width, x_height;   // Graphical Object's dimensions
    int             x_idx;               // selected item's index
    int             x_n_items;           // number of items in the popmenu
    int             x_maxitems;
    int             x_disabled;
    int             x_zoom;
    t_symbol       *x_name;
    t_symbol      **x_items;
    t_symbol       *x_sym;
    char            x_tag_obj[32];
    char            x_tag_outline[32];
    char            x_tag_in[32];
    char            x_tag_out[32];
    char            x_tag_sel[32];
    char            x_tag_mb[32];
    char            x_tag_popmenu[32];
    char            x_tag_menu[64];
    char            x_callback_proc[64];
    char            *x_cvId;
    int             x_edit;
    t_symbol       *x_bg;
    t_symbol       *x_fg;
    t_symbol       *x_ignore;
}t_menu;

static void menu_config_fg(t_menu *x){
    pdgui_vmess(0, "rr rs rs", x->x_tag_mb, "configure",
        "-fg", x->x_fg->s_name, "-activeforeground", x->x_fg->s_name);
}

static void menu_config_bg(t_menu *x){
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_outline,
        "-fill", x->x_bg->s_name);
    pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure", "-bg", x->x_bg->s_name);
}

static int vis(t_menu *x){
    return(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist));
}

static t_symbol *getcolor(int ac, t_atom *av){
    if((av)->a_type == A_SYMBOL)
        return(atom_getsymbol(av));
    else{
        int r = atom_getintarg(0, ac, av);
        int g = atom_getintarg(1, ac, av);
        int b = atom_getintarg(2, ac, av);
        r = r < 0 ? 0 : r > 255 ? 255 : r;
        g = g < 0 ? 0 : g > 255 ? 255 : g;
        b = b < 0 ? 0 : b > 255 ? 255 : b;
        char color[20];
        sprintf(color, "#%2.2x%2.2x%2.2x", r, g, b);
        return(gensym(color));
    }
}

static void menu_bg(t_menu *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        return;
    t_symbol *color = getcolor(ac, av);
    if(x->x_bg != color){
        x->x_bg = color;
        if(vis(x))
            menu_config_bg(x);
    }
}

static void menu_fg(t_menu *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        return;
    t_symbol *color = getcolor(ac, av);
    if(x->x_fg != color){
        x->x_fg = color;
        if(vis(x))
            menu_config_fg(x);
    }
}

static void menu_disablecheck(t_menu *x){
    if(vis(x)){
        t_canvas *cv = glist_getcanvas(x->x_glist);
        if(x->x_edit){
            pdgui_vmess(0, "rrrrrr", x->x_tag_mb, "configure",
                "-state", "disabled", "-cursor", "hand2");
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_in, "-state", "normal");
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_out, "-state", "normal");
        }
        else{
            pdgui_vmess(0, "rrrrrr", x->x_tag_mb, "configure",
                "-state", "active", "-cursor", "bottom_side");
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_in, "-state", "hidden");
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_out, "-state", "hidden");
        }
    }
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
            menu_disablecheck(p->p_cnv);
        }
    }
}

// DRAW functions
static void create_menu(t_menu *x, t_glist *glist){
    char buf[MAXPDSTRING];
    t_canvas *cv = glist_getcanvas(glist);
    sprintf(x->x_tag_mb, ".x%lx.c.s%lx", (unsigned long)cv, (unsigned long)x);
    sprintf(x->x_tag_menu, "%s.menu", x->x_tag_mb);
    t_symbol* temp_name;
    if(x->x_idx < 0)  // use default if not selected
        temp_name = x->x_name;
    else
        temp_name = x->x_items[x->x_idx];
    pdgui_vmess(0, "rr", "destroy", x->x_tag_mb); // in case it exists (vis?)
    t_atom at[2];
    SETSYMBOL(at, gensym(def_font));
    SETFLOAT(at+1, -DEF_FONTSIZE*x->x_zoom);
    pdgui_vmess(0, "rs rs rA rsrs rs rrrs rr",
        "menubutton", x->x_tag_mb,
        "-text", temp_name->s_name,
        "-font", 2, at,
        "-bg", x->x_bg->s_name, "-fg", x->x_fg->s_name,
        "-activeforeground", x->x_fg->s_name,
        "-direction", "below", "-menu", x->x_tag_menu,
        "-cursor", "bottom_side");
    pdgui_vmess(0, "rsri", "menu", x->x_tag_menu, "-tearoff", 0);
    for(int i = 0 ; i < x->x_n_items; i++){
        sprintf(buf, "%s", x->x_items[i]->s_name);
        sys_vgui("%s add command -label \"%s\" -command {%s \
            configure -text \"%s\" \n", x->x_tag_menu, buf, x->x_tag_mb, buf);
        sys_vgui("%s \"%d\"} \n", x->x_callback_proc, i);
    }
    // set tk widget ids
    sprintf(buf, ".x%lx.c", (long unsigned int)cv);
    x->x_cvId = getbytes(strlen(buf) + 1); // Tk ID for current canvas this object is drawn in
    strcpy(x->x_cvId, buf);
    sys_vgui("bind %s.s%lx <Button> {pdtk_canvas_mouse %s [expr %%X - [winfo rootx %s]] \
        [expr %%Y - [winfo rooty %s]] %%b 0}\n", x->x_cvId, x, x->x_cvId, x->x_cvId, x->x_cvId);
    sys_vgui("bind %s.s%lx <Button-2> {pdtk_canvas_rightclick %s [expr %%X - [winfo rootx %s]] \
        [expr %%Y - [winfo rooty %s]] %%b}\n", x->x_cvId, x, x->x_cvId, x->x_cvId, x->x_cvId);
}

static void menu_draw(t_menu *x, t_glist *glist){
    t_canvas *cv = x->x_glist = glist_getcanvas(glist);
    int z = x->x_zoom;
    int x1 = text_xpix(&x->x_obj, glist), y1 = text_ypix(&x->x_obj, glist);
    int x2 = x1 + x->x_width*z, y2 = y1 + x->x_height*z;
    create_menu(x, glist);
// popmenu
    int xb = 2; // horizontal border
    int yb = 4; // vertical border
    int w = x->x_width*z, h = x->x_height*z;
    char *tags_popmenu[] = {x->x_tag_popmenu, x->x_tag_obj};
    pdgui_vmess(0, "crr iiriri rsrs rS", cv, "create", "window",
        x1+xb, y1+yb, "-width", w-xb, "-height", h-yb*2,
        "-anchor", "nw", "-window", x->x_tag_mb,
        "-tags", 2, tags_popmenu);
    
//    sys_vgui("%s configure -state \"%s\" \n",
//        x->x_tag_mb, x->x_disabled ? "disabled" : "active");
    
 // Object Outline
    char *tags_outline[] = {x->x_tag_outline, x->x_tag_obj, x->x_tag_sel};
    pdgui_vmess(0, "crr iiii rs rS", cv, "create", "rectangle",
        x1, y1, x2, y2, "-fill", x->x_bg->s_name, "-tags", 3, tags_outline);
// inlet
    char *tags_in[] = {x->x_tag_in, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        x1, y1, x1+IOWIDTH*z, y1+IHEIGHT*z,
        "-fill", "black", "-state", "hidden", "-tags", 2, tags_in);
// outlet
    char *tags_out[] = {x->x_tag_out, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        x1, y2-OHEIGHT*z, x1+IOWIDTH*z, y2,
        "-fill", "black", "-state", "hidden", "-tags", 2, tags_out);
}

static void menu_erase(t_menu* x,t_glist* glist){
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "rr", "destroy", x->x_tag_mb);
    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_obj);
}
	
// ------------------------ widgetbehaviour-----------------------------
static void menu_getrect(t_gobj *z, t_glist *gl, int *xp1, int *yp1, int *xp2, int *yp2){
    t_menu* x = (t_menu*)z;
    *xp1 = text_xpix(&x->x_obj, gl);
    *yp1 = text_ypix(&x->x_obj, gl);
    *xp2 = text_xpix(&x->x_obj, gl) + x->x_width*x->x_zoom;
    *yp2 = text_ypix(&x->x_obj, gl) + x->x_height*x->x_zoom;
}

static void menu_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_menu *x = (t_menu *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    dx *= x->x_zoom, dy *= x->x_zoom;
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "crs ii", cv, "move", x->x_tag_obj, dx, dy);
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void menu_select(t_gobj *z, t_glist *glist, int sel){
    t_menu *x = (t_menu *)z;
    t_canvas *cv = glist_getcanvas(glist);
    if(sel)
        pdgui_vmess(0, "crs rs", cv, "itemconfigure",
            x->x_tag_sel, "-outline", "blue");
    else
        pdgui_vmess(0, "crs rs", cv, "itemconfigure",
            x->x_tag_sel, "-outline", "black");
}

static void menu_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void menu_vis(t_gobj *z, t_glist *glist, int vis){
    t_menu* x = (t_menu*)z;
    x->x_cv = glist_getcanvas(x->x_glist = glist);
    vis ? menu_draw(x, glist) : menu_erase(x, glist);
}

static void menu_save(t_gobj *z, t_binbuf *b){
    t_menu *x = (t_menu *)z;
    binbuf_addv(b, "ssiis", gensym("#X"), gensym("obj"),
        (t_int)x->x_obj.te_xpix, (t_int)x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)));
    binbuf_addv(b, "iissssssiiiiiiii",
        x->x_width, x->x_height,
        x->x_bg, x->x_fg,
        gensym("empty"), gensym("empty"), gensym("empty"), gensym("empty"), // label rcv snd prm
        0, 0, // output mode, justify
        0, 0, 0, // initial value, loadbang, savestate
        0, // save contents flag
        0 , // flush / below?
        0); // placeholder
    for(int i = 0; i < x->x_n_items; i++) // Loop for menu items
        binbuf_addv(b, "s", x->x_items[i]);
    binbuf_addv(b, ";");
}

// ---------------------------------------------------------------------------
int ishex(const char *s){
    s++;
    if(*s == 'x' || *s == 'X')
        return(1);
    else
        return(0);
}

static void string2atom(t_atom *ap, char* cp, int clen){
    char *buffer = getbytes(sizeof(char)*(clen+1));
    char *endptr[1];
    t_float ftest;
    strncpy(buffer, cp, clen);
    buffer[clen] = 0;
    ftest = strtod(buffer, endptr);
    if(buffer+clen != *endptr) // strtof() failed, we have a symbol
        SETSYMBOL(ap, gensym(buffer));
    else{ // probably a number, let's test for hexadecimal (inf/nan are still floats)
        if(ishex(buffer))
            SETSYMBOL(ap, gensym(buffer));
        else
            SETFLOAT(ap, ftest);
    }
    freebytes(buffer, sizeof(char)*(clen+1));
}

static void symbol2any_process(t_menu *x, t_symbol *s){
    char *cc;
    char *deli;
    int   dell;
    char *cp, *d;
    int i = 1;
    cc = (char*)s->s_name;
    cp = cc;
    deli = " ";
    dell = strlen(deli);
    while((d = strstr(cp, deli))){ // get the number of items
        if(d != NULL && d != cp)
            i++;
        cp = d+dell;
    }
    t_atom *av = getbytes(i*sizeof(t_atom));
    int ac = i;
    /* parse the items into the list-buffer */
    i = 0;
    /* find the first item */
    cp = cc;
    while(cp == (d = strstr(cp,deli)))
        cp += dell;
    while((d = strstr(cp, deli))){
        if(d != cp){
            string2atom(av+i, cp, d-cp);
            i++;
        }
        cp = d+dell;
    }
    if(cp)
        string2atom(av+i, cp, strlen(cp));
    if(ac){
        if(av->a_type == A_FLOAT)
            outlet_list(x->x_obj.ob_outlet, &s_list, ac, av);
        else{
            s = atom_getsymbolarg(0, ac, av);
            outlet_anything(x->x_obj.ob_outlet, s, ac-1, av+1);
        }
    }
}
// ---------------------------------------------------------------------------

static void menu_output(t_menu* x, t_floatarg menu_index){
    x->x_idx = menu_index;
    symbol2any_process(x, x->x_items[(int)menu_index]);
//    outlet_float(x->x_obj.ob_outlet, menu_index);
}

static void menu_disable(t_menu*x, t_float f){
    x->x_disabled = (f > 0.f);
    if(vis(x))
        pdgui_vmess(0, "rr rr", x->x_tag_mb, "configure",
            "-state",  x->x_disabled ? "disabled" : "active");
}

static void menu_items(t_menu* x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    int visible = vis(x);
    if(ac > x->x_maxitems){ // resize the items-array
        if(x->x_items)freebytes(x->x_items, sizeof(t_symbol*)*x->x_maxitems);
        x->x_maxitems = ac;
        x->x_items = (t_symbol**)getbytes(sizeof(t_symbol*)*x->x_maxitems);
    }
    x->x_name = gensym(" ");
    if(vis(x)){
        pdgui_vmess(0, "rr ir", x->x_tag_menu, "delete", 0, "end");
        pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure", "-text", x->x_name->s_name);
    }
    char buf[MAXPDSTRING];
    for(int i = 0; i < ac; i++){
        if((av+i)->a_type == A_FLOAT){
            sprintf(buf, "%g", atom_getfloat(av+i));
            x->x_items[i] = gensym(buf);
        }
        else
            x->x_items[i] = atom_getsymbol(av+i);
        if(visible)
            sys_vgui(".x%lx.c.s%lx.menu add command -label \"%s\" \
                -command {.x%lx.c.s%lx configure -text \"%s\"; \
                %s \"%d\"} \n",
                x->x_glist, x, x->x_items[i]->s_name, x->x_glist,
                x, x->x_items[i]->s_name, x->x_callback_proc, i);
    }
}

static void menu_add(t_menu* x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_n_items == 0){
        x->x_name = gensym(" ");
        menu_disable(x, 0);
        if(vis(x))
            pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure",
                "-text", x->x_name->s_name);
    }
    int new_limit = x->x_n_items + ac;
    if(new_limit > x->x_maxitems){
        int new_size = new_limit * 2;
        t_symbol **dummy = (t_symbol**)getbytes(sizeof(t_symbol*)*new_size);
        if(dummy){
            memcpy(dummy, x->x_items, sizeof(t_symbol*)*x->x_maxitems);
            freebytes(x->x_items, sizeof(t_symbol*)*x->x_maxitems);
            x->x_maxitems = new_size;
            x->x_items = dummy;
        }
        else{
            pd_error(x, "menu: no memory for items");
            return;
        }
    }
    char buf[MAXPDSTRING];
    for(int i = x->x_n_items; i < new_limit; i++){
        if((av+i-x->x_n_items)->a_type == A_FLOAT){
            sprintf(buf, "%g", atom_getfloat(av+i-x->x_n_items));
            x->x_items[i] = gensym(buf);
        }
        else
            x->x_items[i] = atom_getsymbol(av+i-x->x_n_items);
        if(vis(x))
            sys_vgui(".x%lx.c.s%lx.menu add command -label \"%s\" \
                -command {.x%lx.c.s%lx configure -text \"%s\" ;\
                %s \"%d\"} \n",
                x->x_glist, x, x->x_items[i]->s_name, x->x_glist,
                x, x->x_items[i]->s_name, x->x_callback_proc, i);
    }
    x->x_n_items = new_limit;
}

static void menu_clear(t_menu* x){
    t_symbol *s = gensym("dummy");
    t_atom at[1];
    SETSYMBOL(at, s);
    menu_items(x, s, 1, at);
    x->x_name = gensym("empty");
    menu_disable(x, x->x_n_items == 1);
    x->x_n_items = 0;
    if(vis(x)){
        pdgui_vmess(0, "rr ir", x->x_tag_menu, "delete", 0, "end");
        pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure", "-text", x->x_name->s_name);
    }
}

static void menu_float(t_menu* x, t_floatarg f){
	int i = (int)f;
	if(x->x_n_items > 0 && i < x->x_n_items && i >= 0){
        if(vis(x)){
            t_canvas *cv = glist_getcanvas(x->x_glist);
            pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure",
                "-text", x->x_items[i]->s_name);
            pdgui_vmess(0, "ri", x->x_callback_proc, i);
        }
        else
            menu_output(x, i);
	}
    else{
        if(x->x_n_items == 0)
            post("[popmenu]: empty items");
        else
            post("[popmenu]: index %d out of range", i);
    }
}

static void menu_bang(t_menu* x){
    if(x->x_idx >= 0)
        menu_output(x, x->x_idx);
    else
        post("[popmenu]: nothing selected or empty");
}

static void menu_width(t_menu* x, t_floatarg f){
    x->x_width = f < 40 ? 40 : (int)f;
    int z = x->x_zoom;
    int xb = 2;
    pdgui_vmess(0, "crs ri", x->x_cv, "itemconfigure",
        x->x_tag_popmenu, "-width", x->x_width*z-xb);
    int x1 = text_xpix(&x->x_obj, x->x_glist);
    int y1 = text_ypix(&x->x_obj, x->x_glist);
    int x2 = x1 + x->x_width*z;
    int y2 = y1 + x->x_height*z;
    if(vis(x))
        pdgui_vmess(0, "crs iiii", x->x_cv, "coords",
            x->x_tag_outline, x1, y1, x2, y2);
}

static void menu_height(t_menu* x, t_floatarg f){
    x->x_height = f < 25 ? 25 : (int)f;
    int z = x->x_zoom;
    int yb = 4;
    pdgui_vmess(0, "crs ri", x->x_cv, "itemconfigure",
        x->x_tag_popmenu, "-height", x->x_height*z-yb*2);
    int x1 = text_xpix(&x->x_obj, x->x_glist);
    int y1 = text_ypix(&x->x_obj, x->x_glist);
    int x2 = x1 + x->x_width*z;
    int y2 = y1 + x->x_height*z;
    if(vis(x)){
        pdgui_vmess(0, "crs iiii", x->x_cv, "coords",
            x->x_tag_outline, x1, y1, x2, y2);
        pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_out,
            x1, y2 - OHEIGHT*z, x1 + IOWIDTH*z, y2);
        canvas_fixlinesfor(x->x_glist, (t_text *)x);
    }
}

static void menu_set(t_menu* x, t_floatarg f){
    int i = (int)f;
    if(x->x_n_items > 0 && i < x->x_n_items && i >= 0){
        x->x_idx = i;
        if(vis(x))
            pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure",
                "-text", x->x_items[i]->s_name);
    }
    else{
        if(x->x_n_items == 0)
            post("[popmenu]: empty items");
        else
            post("[popmenu]: index %d out of range", i);
    }
}

static void menu_zoom(t_menu *x, t_floatarg f){
    x->x_zoom = (int)f;
}
// ---------------------------------------------------------------------------

static void menu_free(t_menu*x){
    if(x->x_items)
        freebytes(x->x_items, sizeof(t_symbol*)*x->x_maxitems);
    pd_unbind(&x->x_obj.ob_pd, x->x_sym);
    x->x_proxy->p_cnv = NULL;
    pdgui_stub_deleteforkey(x);
}

static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy *edit_proxy_new(t_menu *x, t_symbol *s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

static void *menu_new(t_symbol *s, int ac, t_atom *av){
    t_menu *x = (t_menu *)pd_new(menu_class);
    x->x_ignore = s;
	char buf[256];
    t_canvas *cv = canvas_getcurrent();
    x->x_glist = (t_glist*)cv;
    x->x_idx = -1;
    x->x_maxitems = MAX_ITEMS;
    x->x_items = (t_symbol**)getbytes(sizeof(t_symbol*)*x->x_maxitems);
    x->x_width = 128;
    x->x_height = 26;
    x->x_fg = gensym("black");
    x->x_bg = gensym("#dfdfdf");
    x->x_n_items = 0;
    x->x_name = gensym("empty");
    x->x_disabled = 1;
    if(ac >= 16){
        x->x_width = atom_getintarg(0, ac, av);
        x->x_height = atom_getintarg(1, ac, av);
        x->x_bg = atom_getsymbolarg(2, ac, av);
        x->x_fg = atom_getsymbolarg(3, ac, av);
        x->x_name = atom_getsymbolarg(4, ac, av); // label name?
        //  label rcv snd prm
        // output mode, justify
        // initial value, loadbang, savestate
        // save contents flag
        // 2 placeholders
        ac-=16, av+=16;
        x->x_disabled = 0;
        x->x_n_items = ac;
        for(int i = 0; i < x->x_n_items; i++){
            if((av+i)->a_type == A_FLOAT){
                sprintf(buf, "%g", atom_getfloat(av+i));
                x->x_items[i] = gensym(buf);
            }
            else
                x->x_items[i] = atom_getsymbol(av+i);
        }
    }
    x->x_edit = x->x_glist->gl_edit;
    x->x_zoom = x->x_glist->gl_zoom;
    // callback function
    sprintf(buf,"menu%lx", (long unsigned int)x);
    x->x_sym = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_sym); // bind receiver to the object
    sprintf(x->x_callback_proc, "%menu_callback%lx", (unsigned long)x);
    sys_vgui("proc %s {index} {\n pdsend \"%s _callback $index \"\n }\n",
        x->x_callback_proc, buf);
    char buff[MAXPDSTRING];
    snprintf(buff, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_glist);
    buff[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buff));
    sprintf(x->x_tag_obj, "%pOBJ", x);
    sprintf(x->x_tag_outline, "%pOUTLINE", x);
    sprintf(x->x_tag_mb, ".x%lx.c.s%lx", (unsigned long)cv, (unsigned long)x);
    sprintf(x->x_tag_popmenu, "%pMENU", x);
    sprintf(x->x_tag_in, "%pIN", x);
    sprintf(x->x_tag_out, "%pOUT", x);
    sprintf(x->x_tag_sel, "%pSEL", x);
    outlet_new(&x->x_obj, &s_float);
    return(x);
}

void popmenu_setup(void){
    menu_class = class_new(gensym("popmenu"), (t_newmethod)menu_new,
        (t_method)menu_free, sizeof(t_menu), 0, A_GIMME, 0);
    class_addbang(menu_class, (t_method)menu_bang);
    class_addfloat(menu_class, (t_method)menu_float);
    class_addmethod(menu_class, (t_method)menu_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_width, gensym("width"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_height, gensym("height"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_clear, gensym("clear"), 0);
    class_addmethod(menu_class, (t_method)menu_add, gensym("add"), A_GIMME, 0);
    class_addmethod(menu_class, (t_method)menu_bg, gensym("bg"), A_GIMME, 0);
    class_addmethod(menu_class, (t_method)menu_fg, gensym("fg"), A_GIMME, 0);
    class_addmethod(menu_class, (t_method)menu_output, gensym("_callback"), A_DEFFLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_zoom, gensym("zoom"), A_CANT, 0);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
    class_setwidget(menu_class, &menu_widgetbehavior);
    menu_widgetbehavior.w_getrectfn  = menu_getrect;
    menu_widgetbehavior.w_displacefn = menu_displace;
    menu_widgetbehavior.w_selectfn   = menu_select;
    menu_widgetbehavior.w_deletefn   = menu_delete;
    menu_widgetbehavior.w_visfn      = menu_vis;
    class_setsavefn(menu_class, &menu_save);
}
