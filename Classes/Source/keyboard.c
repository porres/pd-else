// Written By Flávio Schiavoni and Alexandre Porres

#include "m_pd.h"
#include "g_canvas.h"

#define BLACK_ON    "#6666FF"
#define BLACK_OFF   "#000000"
#define WHITE_ON    "#9999FF"
#define WHITE_OFF   "#FFFFFF"
#define MIDDLE_C    "#F0FFFF" // color of Middle C when off

static t_class *keyboard_class;

typedef struct _keyboard{
    t_object    x_obj;
    t_canvas   *x_cv;
    t_glist    *x_glist;
    int        *x_notes;        // to store which notes should be played
    int         x_last_note;    // to store last note
    float       x_vel_in;       // to store the second inlet values
    float       x_space;
    int         x_width;
    int         x_height;
    int         x_octaves;
    int         x_first_c;
    int         x_low_c;
    int         x_toggle_mode;
    int         x_zoom;
    t_outlet   *x_out;
}t_keyboard;

/* ------------------------- Keyboard Play ------------------------------*/
static void keyboard_note_on(t_keyboard* x, int i){
    short key = i % 12;
    short black = (key == 1 || key == 3 || key == 6 || key == 8 || key == 10);
    sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill %s\n", x->x_cv, x, i, black ? BLACK_ON : WHITE_ON);
    t_atom a[2];
    SETFLOAT(a, (float)(x->x_first_c + i));
    SETFLOAT(a+1, x->x_notes[i] = 127);
    outlet_list(x->x_out, &s_list, 2, a);
}

static void keyboard_note_off(t_keyboard* x, int i){
    short key = i % 12;
    short black = (key == 1 || key == 3 || key == 6 || key == 8 || key == 10);
    if(black)
        sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill %s\n", x->x_cv, x, i, BLACK_OFF);
    else
        sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill %s\n", x->x_cv, x, i, i == 60 ? MIDDLE_C : WHITE_OFF);
    t_atom a[2];
    SETFLOAT(a, (float)(x->x_first_c + i));
    SETFLOAT(a+1, x->x_notes[i] = 0);
    outlet_list(x->x_out, &s_list, 2, a);
}

// TOGGLE MODE
static void keyboard_play_tgl(t_keyboard* x, int i){
    x->x_notes[i] = x->x_notes[i] ? 0 : 127;
    short key = i % 12;
    if(key == 1 || key == 3 || key == 6 || key == 8 || key == 10) // black
        sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill %s\n", x->x_cv, x, i, x->x_notes[i] ? BLACK_ON : BLACK_OFF);
     else{ // white
         if(x->x_notes[i])
             sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill %s\n", x->x_cv, x, i, WHITE_ON);
         else
             sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill %s\n", x->x_cv, x, i,  i == 60 ? MIDDLE_C : WHITE_OFF);
    }
    t_atom a[2];
    SETFLOAT(a, (float)(x->x_first_c + i));
    SETFLOAT(a+1, x->x_notes[i]);
    outlet_list(x->x_out, &s_list, 2, a);
}

// FLUSH
static void keyboard_flush(t_keyboard* x){
    if(!x->x_toggle_mode)
        return;
    t_atom a[2];
    for(short i = 0 ; i < x->x_octaves * 12 ; i++){
        float note = (float)(x->x_first_c + i);
        if(x->x_notes[i] > 0){
            short key = i % 12;
            if(key == 1 || key == 3 || key == 6 || key == 8 || key == 10) // <= BLACK KEY
                sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill %s\n", x->x_cv, x, i, BLACK_OFF);
            else
                sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill %s\n", x->x_cv, x, i, note == 60 ? MIDDLE_C : WHITE_OFF);
            SETFLOAT(a, (float)(x->x_first_c + i));
            SETFLOAT(a+1, x->x_notes[i] = 0);
            outlet_list(x->x_out, &s_list, 2, a);
        }
    }
}

/* -------------------- MOUSE Events ----------------------------------*/
// Map mouse event position and find corresponding note
static int find_note(t_keyboard* x, float xpix, float ypix){
    ypix = 0;
    short i, wcounter, bcounter;
    float size = x->x_space;
    float xpos = x->x_obj.te_xpix * x->x_zoom;
//    post("size = %d / xpos = %d", (int)size, (int)xpos);
//    post("xpix = %d / ypix - xpos = %d", (int)xpix, (int)xpix - (int)xpos);
    wcounter = bcounter = 0;
    for(i = 0; i < x->x_octaves * 12; i++){
        short key = i % 12;
        if(key == 4 || key == 11)
            bcounter++;
        if(key == 1 || key == 3 || key == 6 || key == 8 || key == 10){ // <= BLACK KEY
            if(xpix > xpos + ((bcounter + 1) * size) - ((int)(size / 3.f))
            && xpix < xpos + ((bcounter + 1) * size) + ((int)(size / 3.f)))
                return(i);
            bcounter++;
        }
        else{ // <= WHITE KEY
            if(xpix > xpos + wcounter * size //
            && xpix < xpos + (wcounter + 1) * ((int)(size)))// * 2.f/3.f)))
                return(i);
            wcounter++;
        }
    }
    return(-1);
}

// Mouse press
static void keyboard_mousepress(t_keyboard* x, float xpix, float ypix, float id){
    if((int)x != (int)id) // Check if it's the right instance to receive this message
        return;
    if(x->x_glist->gl_edit) // If edit mode, give up!
        return;
    int note = find_note(x, xpix, ypix);
    if(note >= 0)
        x->x_toggle_mode ? keyboard_play_tgl(x, note) : keyboard_note_on(x, x->x_last_note = note);
}

// Mouse release
static void keyboard_mouserelease(t_keyboard* x, float xpix, float ypix, float id){
    xpix = ypix = 0;
    if((int)x != (int)id) // Check if it's the right instance to receive this message
        return;
    if(x->x_toggle_mode || x->x_glist->gl_edit) // Give up if toggle or edit mode!
        return;
    keyboard_note_off(x, x->x_last_note);
}

// Mouse Drag event
static void keyboard_mousemotion(t_keyboard* x, float xpix, float ypix, float id){
    ypix = 0;
    if((int)x != (int)id) // Check if it's the right instance to receive this message
        return;
    if(x->x_toggle_mode || x->x_glist->gl_edit) // Give up if toggle or edit mode!
        return;
    int xpos = (int)(x->x_obj.te_xpix * x->x_zoom);
    if((int)xpix < xpos || (int)xpix > xpos + x->x_width) // ignore if out of bounds
        return;
    int new_note = find_note(x, (int)xpix, (int)ypix);
    if(new_note == -1 || new_note == x->x_last_note)
        return;
    keyboard_note_off(x, x->x_last_note);
    keyboard_note_on(x, x->x_last_note = new_note);
}

/* ------------------------ GUI SHIT----------------------------- */
// Erase the GUI
static void keyboard_erase(t_keyboard *x){
    sys_vgui(".x%lx.c delete %xrr\n", x->x_cv, x);
    for(int i = 0; i < x->x_octaves * 12; i++)
        sys_vgui(".x%lx.c delete %xrrk%d\n", x->x_cv, x, i);
}

// Draw the GUI
static void keyboard_draw(t_keyboard *x){
    int xpos = text_xpix(&x->x_obj, x->x_glist);
    int ypos = text_ypix(&x->x_obj, x->x_glist);
//    sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %xrr -fill #FFFFFF\n", // Main rectangle
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %xrr\n", // Main rectangle
             x->x_cv,
             xpos,
             ypos,
             xpos + x->x_width,
             ypos + x->x_height,
             x);
    int i, wcounter, bcounter;
    wcounter = bcounter = 0;
    for(i = 0 ; i < x->x_octaves * 12 ; i++){ // white keys 1st (blacks overlay it)
        short key = i % 12;
        if(key != 1 && key != 3 && key != 6 && key != 8 && key != 10){
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags {%xrrk%d %xrr} -fill %s\n",
                     x->x_cv,
                     xpos + wcounter * (int)x->x_space,
                     ypos,
                     xpos + (wcounter + 1) * (int)x->x_space,
                     ypos + x->x_height,
                     x,
                     i,
                     x,
                     x->x_first_c + i == 60 ? MIDDLE_C : WHITE_OFF);
            wcounter++;
        }
    }
    for(i = 0 ; i < x->x_octaves * 12 ; i++){ // Black keys
        short key = i % 12;
        if(key == 4 || key == 11){
            bcounter++;
            continue;
        }
        if(key == 1 || key == 3 || key ==6 || key == 8 || key == 10){
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags {%xrrk%d %xrr} -fill %s\n",
                     x->x_cv,
                     xpos + ((bcounter + 1) * (int)x->x_space) - ((int)(x->x_space / 3.f)) ,
                     ypos,
                     xpos + ((bcounter + 1) * (int)x->x_space) + ((int)(x->x_space / 3.f)) ,
                     ypos + x->x_height * 2/3,
                     x,
                     i,
                     x,
                     BLACK_OFF);
            bcounter++;
        }
    }
    canvas_fixlinesfor(x->x_glist, (t_text *)x);
}

/* ------------------------ widgetbehaviour----------------------------- */
// GET RECT
static void keyboard_getrect(t_gobj *z, t_glist *owner, int *xp1, int *yp1, int *xp2, int *yp2){
    t_keyboard *x = (t_keyboard *)z;
    *xp1 = text_xpix(&x->x_obj, owner);
    *yp1 = text_ypix(&x->x_obj, owner) ;
    *xp2 = text_xpix(&x->x_obj, owner) + x->x_width;
    *yp2 = text_ypix(&x->x_obj, owner) + x->x_height;
}

// DISPLACE
void keyboard_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_keyboard *x = (t_keyboard *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    sys_vgui(".x%lx.c move %xrr %d %d\n", glist_getcanvas(glist), x, dx * x->x_zoom, dy * x->x_zoom);
    canvas_fixlinesfor(glist, (t_text *)x);
}

// SELECT
static void keyboard_select(t_gobj *z, t_glist *gl, int sel){
    sys_vgui(".x%lx.c itemconfigure %xrr -outline %s\n", glist_getcanvas(gl), (t_keyboard *)z, sel ? "blue" : "black");
}

// Delete the GUI
static void keyboard_delete(t_gobj *z, t_glist *glist){
    t_text *x = (t_text *)z;
    canvas_deletelinesfor(glist_getcanvas(glist), x);
}

// VIS
static void keyboard_vis(t_gobj *z, t_glist *glist, int vis){
    t_keyboard *x = (t_keyboard *)z;
    t_canvas *cv = x->x_cv = glist_getcanvas(x->x_glist = glist);
    if(vis){
        keyboard_draw(x);
        sys_vgui(".x%lx.c bind %xrr <ButtonPress-1> {\n keyboard_mousepress \"%d\" %%x %%y %%b\n}\n", cv, x, x);
        sys_vgui(".x%lx.c bind %xrr <ButtonRelease-1> {\n keyboard_mouserelease \"%d\" %%x %%y %%b\n}\n", cv, x, x);
        sys_vgui(".x%lx.c bind %xrr <B1-Motion> {\n keyboard_mousemotion \"%d\" %%x %%y\n}\n", cv, x, x);
    }
    else
        keyboard_erase(x);
}

/* ------------------------ GUI Behaviour -----------------------------*/
// Set Properties
static void keyboard_set_properties(t_keyboard *x, float space,
        float height, float oct, float low_c, float tgl){
    x->x_toggle_mode = 0; // tgl != 0;
    x->x_height = (height < 10) ? 10 : height;
    x->x_octaves = oct < 1 ? 1 : oct > 10 ? 10 : oct;
    x->x_low_c = low_c < 0 ? 0 : low_c > 8 ? 8 : low_c;
    x->x_space = (space < 7) ? 7 : space; // key width
    x->x_width = ((int)(x->x_space)) * 7 * (int)x->x_octaves;
    x->x_first_c = ((int)(x->x_low_c * 12)) + 12;
    x->x_toggle_mode = tgl != 0;
// get and clear notes
    x->x_notes = getbytes(sizeof(int) * 12 * x->x_octaves);
    for(int i = 0; i < 12 * x->x_octaves; i++)
        x->x_notes[i] = 0;
}

// Keyboard Properties
void keyboard_properties(t_gobj *z, t_glist *owner){
    owner = NULL;
    t_keyboard *x = (t_keyboard *)z;
    char cmdbuf[256];
    sprintf(cmdbuf, "keyboard_properties %%s %d %d %d %d %d\n",
        (int)x->x_space,
        (int)x->x_height,
        (int)x->x_octaves,
        (int)x->x_low_c,
        (int)x->x_toggle_mode);
    gfxstub_new(&x->x_obj.ob_pd, x, cmdbuf);
}

// Save Properties
static void keyboard_save(t_gobj *z, t_binbuf *b){
    t_keyboard *x = (t_keyboard *)z;
    binbuf_addv(b,
                "ssiisiiiii",
                gensym("#X"),
                gensym("obj"),
                (int)x->x_obj.te_xpix,
                (int)x->x_obj.te_ypix,
                atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)),
                (int)x->x_space / x->x_zoom,
                (int)x->x_height / x->x_zoom,
                (int)x->x_octaves,
                (int)x->x_low_c,
                (int)x->x_toggle_mode);
    binbuf_addv(b, ";");
}

t_widgetbehavior keyboard_widgetbehavior ={
    keyboard_getrect,
    keyboard_displace,
    keyboard_select,
    NULL, // Activate not used
    keyboard_delete,
    keyboard_vis,
    NULL, // can't use it because it doesn't have press and release
};

// Apply changes of property windows
static void keyboard_apply(t_keyboard *x, float w, float h, float oct, float low_c, float tgl){
    keyboard_erase(x);
    keyboard_set_properties(x, w, h, oct, low_c, tgl);
    keyboard_draw(x);
}

/* ------------------------- Methods ------------------------------*/
void keyboard_float(t_keyboard *x, t_floatarg note){
    t_atom a[2];
    SETFLOAT(a, note);
    SETFLOAT(a+1, x->x_vel_in);
    outlet_list(x->x_out, &s_list, 2, a);
    if(x->x_glist->gl_havewindow){
        if(note > x->x_first_c && note < x->x_first_c + (x->x_octaves * 12)){
            int i = (int)(note - x->x_first_c);
            int on = (x->x_notes[i] = x->x_vel_in) > 0;
            short key = i % 12;
            if(key == 1 || key == 3 || key == 6 || key == 8 || key == 10) // black
                sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill %s\n", x->x_cv, x, i, on ? BLACK_ON : BLACK_OFF);
            else // white
                sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill %s\n", x->x_cv, x, i, on ? WHITE_ON : note == 60 ? MIDDLE_C : WHITE_OFF);
        }
    }
}

static void keyboard_height(t_keyboard *x, t_floatarg f){
    f =  f < 10 ? 10 : (int)(f);
    if(x->x_height != f){
        keyboard_erase(x);
        x->x_height = f;
        keyboard_draw(x);
    }
}

static void keyboard_width(t_keyboard *x, t_floatarg f){
    f = f < 7 ? 7 : (int)(f);
    if(x->x_space != f){
        keyboard_erase(x);
        x->x_space = f;
        x->x_width = ((int)(x->x_space)) * 7 * (int)x->x_octaves;
        keyboard_draw(x);
    }
}

static void keyboard_8ves(t_keyboard *x, t_floatarg f){
    f = f > 10 ? 10 : f < 1 ? 1 : (int)(f);
    if(x->x_octaves != f){
        keyboard_erase(x);
        x->x_octaves = f;
        x->x_width = x->x_space * 7 * x->x_octaves;
        keyboard_draw(x);
    }
}

static void keyboard_low_c(t_keyboard *x, t_floatarg f){
    f = f > 8 ? 8 : f < 0 ? 0 : (int)(f);
    if(x->x_low_c != f){
        keyboard_erase(x);
        x->x_low_c = f;
        x->x_first_c = ((int)(x->x_low_c * 12)) + 12;
        keyboard_draw(x);
    }
}

static void keyboard_oct(t_keyboard *x, t_floatarg f){
    keyboard_low_c(x, x->x_low_c + (int)(f));
}

static void keyboard_toggle(t_keyboard *x, t_floatarg f){
    int tgl = f != 0;
    if(x->x_toggle_mode && !tgl)
        keyboard_flush(x);
    x->x_toggle_mode = tgl;
}

/* ------------------------ Free / New / Setup ------------------------------*/
// Free
void keyboard_free(t_keyboard *x){
    pd_unbind(&x->x_obj.ob_pd, gensym("keyboard"));
    gfxstub_deleteforkey(x);
}

// New
void * keyboard_new(t_symbol *s, int ac, t_atom* av){
    t_symbol *dummy = s;
    dummy = NULL;
    t_keyboard *x = (t_keyboard *) pd_new(keyboard_class);
    x->x_glist = (t_glist*)canvas_getcurrent();
    x->x_zoom = x->x_glist->gl_zoom;
    x->x_last_note = -1;
    int tgl = 0;
    float init_space = 17;
    float init_height = 80;
    float init_8ves = 4;
    float init_low_c = 3;
    if(ac) // 1st ARGUMENT: WIDTH
        init_space = atom_getfloat(av++), ac--;
    if(ac) // 2nd ARGUMENT: HEIGHT
        init_height = atom_getfloat(av++), ac--;
    if(ac) // 3rd ARGUMENT: Octaves
        init_8ves = atom_getfloat(av++), ac--;
    if(ac) // 4th ARGUMENT: Lowest C ("First C")
        init_low_c = atom_getfloat(av++), ac--;
    if(ac) // 5th ARGUMENT: Toggle Mode)
        tgl = (int)(atom_getfloat(av++) != 0), ac--;
    keyboard_set_properties(x, init_space, init_height, init_8ves, init_low_c, tgl);
    x->x_out = outlet_new(&x->x_obj, &s_list);
    floatinlet_new(&x->x_obj, &x->x_vel_in);
    pd_bind(&x->x_obj.ob_pd, gensym("keyboard"));
    return(void *) x;
}

static void keyboard_zoom(t_keyboard *x, t_floatarg zoom){
    float mul = zoom == 1.0 ? 0.5 : 2.0;
    x->x_space = (int)((float)x->x_space * mul);
    x->x_width = (int)((float)x->x_width * mul);
    x->x_height = (int)((float)x->x_height * mul);
    x->x_zoom = (int)zoom;
    keyboard_erase(x);
    keyboard_draw(x);
}

// Setup
void keyboard_setup(void){
    keyboard_class = class_new(gensym("keyboard"), (t_newmethod) keyboard_new, (t_method) keyboard_free,
        sizeof (t_keyboard), CLASS_DEFAULT, A_GIMME, 0);
    class_addfloat(keyboard_class, keyboard_float);
    class_addmethod(keyboard_class, (t_method)keyboard_8ves, gensym("8ves"), A_DEFFLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_width, gensym("width"), A_DEFFLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_height, gensym("height"), A_DEFFLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_oct, gensym("oct"), A_DEFFLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_low_c, gensym("lowc"), A_DEFFLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_toggle, gensym("toggle"), A_DEFFLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_flush, gensym("flush"), 0);
    class_addmethod(keyboard_class, (t_method)keyboard_zoom, gensym("zoom"), A_CANT, 0);
// Methods to receive TCL/TK events
    class_addmethod(keyboard_class, (t_method)keyboard_mousepress,gensym("_mousepress"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_mouserelease,gensym("_mouserelease"), A_FLOAT, A_FLOAT, A_FLOAT, 0); 
    class_addmethod(keyboard_class, (t_method)keyboard_mousemotion,gensym("_mousemotion"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_apply, gensym("apply"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
// Widget
    class_setwidget(keyboard_class, &keyboard_widgetbehavior);
    class_setsavefn(keyboard_class, keyboard_save);
    class_setpropertiesfn(keyboard_class, keyboard_properties);
    
    sys_vgui("if {[catch {pd}] } {\n");
    sys_vgui("    proc pd {args} {pdsend [join $args \" \"]}\n");
    sys_vgui("}\n");
    
    sys_vgui("proc keyboard_mousepress {id x y b} {\n");
    sys_vgui("    if {$b == 1} {\n");
    sys_vgui("        pd [concat keyboard _mousepress $x $y $id\\;]\n");
    sys_vgui("    }\n");
    sys_vgui("}\n");
    
    sys_vgui("proc keyboard_mouserelease {id x y b} {\n");
    sys_vgui("    if {$b == 1} {\n");
    sys_vgui("        pd [concat keyboard _mouserelease $x $y $id\\;]\n");
    sys_vgui("    }\n");
    sys_vgui("}\n");
    
    sys_vgui("proc keyboard_mousemotion {id x y} {\n");
    sys_vgui("    set cmd [concat keyboard _mousemotion $x $y $id\\;]\n");
    sys_vgui("    pd $cmd\n");
    sys_vgui("}\n");
    
    sys_vgui("proc keyboard_ok {id} {\n");
    sys_vgui("    keyboard_apply $id\n");
    sys_vgui("    keyboard_cancel $id\n");
    sys_vgui("}\n");
    
    sys_vgui("proc keyboard_cancel {id} {\n");
    sys_vgui("    set cmd [concat $id cancel \\;]\n");
    sys_vgui("    pd $cmd\n");
    sys_vgui("}\n");
    
    sys_vgui("proc keyboard_apply {id} {\n");
    sys_vgui("    set vid [string trimleft $id .]\n");
    sys_vgui("    set var_width [concat var_width_$vid]\n");
    sys_vgui("    set var_height [concat var_height_$vid]\n");
    sys_vgui("    set var_octaves [concat var_octaves_$vid]\n");
    sys_vgui("    set var_low_c [concat var_low_c_$vid]\n");
    sys_vgui("    set var_tgl [concat var_tgl_$vid]\n");
    sys_vgui("\n");
    sys_vgui("    global $var_width\n");
    sys_vgui("    global $var_height\n");
    sys_vgui("    global $var_octaves\n");
    sys_vgui("    global $var_low_c\n");
    sys_vgui("    global $var_tgl\n");
    sys_vgui("\n");
    sys_vgui("    set cmd [concat $id apply \\\n");
    sys_vgui("        [eval concat $$var_width] \\\n");
    sys_vgui("        [eval concat $$var_height] \\\n");
    sys_vgui("        [eval concat $$var_octaves] \\\n");
    sys_vgui("        [eval concat $$var_low_c] \\\n");
    sys_vgui("        [eval concat $$var_tgl] \\;]\n");
    sys_vgui("    pd $cmd\n");
    sys_vgui("}\n");
    
    sys_vgui("proc keyboard_properties {id width height octaves low_c tgl} {\n");
    sys_vgui("    set vid [string trimleft $id .]\n");
    sys_vgui("    set var_width [concat var_width_$vid]\n");
    sys_vgui("    set var_height [concat var_height_$vid]\n");
    sys_vgui("    set var_octaves [concat var_octaves_$vid]\n");
    sys_vgui("    set var_low_c [concat var_low_c_$vid]\n");
    sys_vgui("    set var_tgl [concat var_tgl_$vid]\n");
    sys_vgui("\n");
    sys_vgui("    global $var_width\n");
    sys_vgui("    global $var_height\n");
    sys_vgui("    global $var_octaves\n");
    sys_vgui("    global $var_low_c\n");
    sys_vgui("    global $var_tgl\n");
    sys_vgui("\n");
    sys_vgui("    set $var_width $width\n");
    sys_vgui("    set $var_height $height\n");
    sys_vgui("    set $var_octaves $octaves\n");
    sys_vgui("    set $var_low_c $low_c\n");
    sys_vgui("    set $var_tgl $tgl\n");
    sys_vgui("\n");
    sys_vgui("    toplevel $id\n");
    sys_vgui("    wm title $id {Keyboard}\n");
    sys_vgui("    wm protocol $id WM_DELETE_WINDOW [concat keyboard_cancel $id]\n");
    sys_vgui("\n");
    sys_vgui("    label $id.label -text {Keyboard properties}\n");
    sys_vgui("    pack $id.label -side top\n");
    sys_vgui("\n");
    sys_vgui("    frame $id.size\n");
    sys_vgui("    pack $id.size -side top\n");
    sys_vgui("    label $id.size.lwidth -text \"Key Width:\"\n");
    sys_vgui("    entry $id.size.width -textvariable $var_width -width 7\n");
    sys_vgui("    label $id.size.lheight -text \"Height:\"\n");
    sys_vgui("    entry $id.size.height -textvariable $var_height -width 7\n");
    sys_vgui("    pack $id.size.lwidth $id.size.width $id.size.lheight $id.size.height -side left\n");
    sys_vgui("\n");
    sys_vgui("    frame $id.notes\n");
    sys_vgui("    pack $id.notes -side top\n");
    sys_vgui("    label $id.notes.loctaves -text \"Octaves:\"\n");
    sys_vgui("    entry $id.notes.octaves -textvariable $var_octaves -width 7\n");
    sys_vgui("    label $id.notes.llow_c -text \"Low C:\"\n");
    sys_vgui("    entry $id.notes.low_c -textvariable $var_low_c -width 7\n");
    sys_vgui("    pack $id.notes.loctaves $id.notes.octaves $id.notes.llow_c $id.notes.low_c -side left\n");
    sys_vgui("\n");
// new tgl mode
    sys_vgui("    frame $id.mode\n");
    sys_vgui("    pack $id.mode -side top\n");
    sys_vgui("    label $id.mode.ltgl -text \"Toggle Mode:\"\n");
    sys_vgui("    entry $id.mode.tgl -textvariable $var_tgl -width 2\n");
    sys_vgui("    pack $id.mode.ltgl $id.mode.tgl -side left\n");
    sys_vgui("\n");
//
    sys_vgui("    frame $id.buttonframe\n");
    sys_vgui("    pack $id.buttonframe -side bottom -fill x -pady 2m\n");
    sys_vgui("    button $id.buttonframe.cancel -text {Cancel} -command \"keyboard_cancel $id\"\n");
    sys_vgui("    button $id.buttonframe.apply -text {Apply} -command \"keyboard_apply $id\"\n");
    sys_vgui("    button $id.buttonframe.ok -text {OK} -command \"keyboard_ok $id\"\n");
    sys_vgui("    pack $id.buttonframe.cancel -side left -expand 1\n");
    sys_vgui("    pack $id.buttonframe.apply -side left -expand 1\n");
    sys_vgui("    pack $id.buttonframe.ok -side left -expand 1\n");
    sys_vgui("\n");
    sys_vgui("    focus $id.size.width\n");
    sys_vgui("}\n");
}
