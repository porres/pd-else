// Written By Flávio Schiavoni and Alexandre Porres

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "m_pd.h"
#include "g_canvas.h"

#define MOUSE_PRESS     127
#define MOUSE_RELEASE   -1

static t_class *keyboard_class;

typedef struct _keyboard{
    t_object x_obj;
    t_float velocity_input; // to store the second inlet values
    t_int width;
    t_int height;
    t_int octaves;
    t_int first_c;
    t_int low_c;
    t_float space;
    t_outlet* x_out;
    t_glist *glist;
    t_canvas *canvas;
    t_int *notes; // To store which notes should be played
}t_keyboard;

/* ------------------------- Keyboard Play ------------------------------*/

static void keyboard_play(t_keyboard* x){
    int i;
    for(i = 0 ; i < x->octaves * 12; i++){     // first, dispatch note off
        short key = i % 12;
        if(x->notes[i] < 0){ // stop play Keyb or mouse
            if( key != 1 && key != 3 && key !=6 && key != 8 && key != 10){
                if(x->first_c + i == 60) // Middle C
                    sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill #F0FFFF\n", x->canvas, x, i);
                else
                    sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill #FFFFFF\n", x->canvas, x, i);
                }
            else
                sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill #000000\n", x->canvas, x, i);
            t_atom a[2];
            SETFLOAT(a, ((int)x->first_c) + i);
            SETFLOAT(a+1, x->notes[i] = 0);
            outlet_list(x->x_out, &s_list, 2, a);
        }
    }
    // then dispatch note on
    for(i = 0 ; i < x->octaves * 12; i++){
        short key = i % 12;
        if(x->notes[i] > 0){ // play Keyb or mouse
            if( key != 1 && key != 3 && key !=6 && key != 8 && key != 10)
                sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill #9999FF\n", x->canvas, x, i);
            else
                sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill #6666FF\n", x->canvas, x, i);
            t_atom a[2];
            SETFLOAT(a, ((int)x->first_c) + i);
            SETFLOAT(a+1, x->notes[i]);
            outlet_list(x->x_out, &s_list, 2, a);
        }
    }
}

/* -------------------- MOUSE Events ----------------------------------*/

//Map mouse event position
static int keyboard_mapclick(t_keyboard* x, t_float xpix, t_float ypix, t_int event){
    short i, wcounter, bcounter;
    wcounter = bcounter = 0;
    for(i = 0 ; i < x->octaves * 12 ; i++){
        short key = i % 12;
        if(key == 4 || key == 11){
            bcounter++;
            }
        if( key == 1 || key == 3 || key ==6 || key == 8 || key == 10){ // Play the blacks
            if(xpix > x->x_obj.te_xpix + ((bcounter + 1) * (int)x->space) - ((int)(0.3f * x->space))
                 && xpix < x->x_obj.te_xpix + ((bcounter + 1) * (int)x->space) + ((int)(0.3f * x->space))
//                 && ypix > x->x_obj.te_ypix
//                 && ypix < x->x_obj.te_ypix + 2 * x->height / 3
                ){
                x->notes[i] = event;
                return i; // Avoid to play the white below
                }
            bcounter++;
            continue;
        }
        else{ // play white keys
            if(xpix > x->x_obj.te_xpix + wcounter * (int)x->space
                 && xpix < x->x_obj.te_xpix + (wcounter + 1) * (int)x->space
//                 && ypix > x->x_obj.te_ypix
//                 && ypix < x->x_obj.te_ypix + x->height
                 ){
                x->notes[i] = event;
                return i;
            }
            wcounter++;
        }
    }
    return -1;
}

// Mouse press
static void keyboard_mousepress(t_keyboard* x, t_float xpix, t_float ypix, t_float id){
    if((int)x != (int)id) // Check if it's the right instance to receive this message
        return;
    if(x->glist->gl_edit) // If edit mode, give up!
        return;
    keyboard_mapclick(x, xpix, ypix, MOUSE_PRESS);
    keyboard_play(x);
}

// Mouse release
static void keyboard_mouserelease(t_keyboard* x, t_float xpix, t_float ypix, t_float id){
    if((int)x != (int)id) // Check if it's the right instance to receive this message
        return;
    if (x->glist->gl_edit) // If edit mode, give up!
        return;
    int i, play;
    play = 0;
    for(i = 0 ; i < x->octaves * 12; i++){
        if(x->notes[i] == MOUSE_PRESS){
            x->notes[i] = MOUSE_RELEASE;
            play = 1;
            }
    }
    if(play == 1)
        keyboard_play(x);
}

// Mouse Drag event
static void keyboard_mousemotion(t_keyboard* x, t_float xpix, t_float ypix, t_float id){
    if((int)x != (int)id) // Check if it's the right instance to receive this message
        return;
    if (x->glist->gl_edit) // If edit mode, give up!
        return;
    if((int)xpix < x->x_obj.te_xpix
       || (int)xpix > x->x_obj.te_xpix + x->width
//       || (int)ypix < x->x_obj.te_ypix
//       || (int)ypix > x->x_obj.te_ypix + x->height
       )
            return;
    int i,actual = 0, new_drag = 0;
    for(i = 0 ; i < x->octaves * 12; i++){ // find actual key playing;
        if(x->notes[i] == MOUSE_PRESS){
            actual = i;
            break;
        }
    }
    new_drag = keyboard_mapclick(x, (int)xpix, (int)ypix, MOUSE_PRESS);
    if(new_drag != -1 && actual != new_drag){
        x->notes[actual] = MOUSE_RELEASE;
        keyboard_play(x);
    }
}

/* ------------------------ GUI Definitions ---------------------------*/

// THE BOUNDING RECTANGLE
static void keyboard_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_keyboard *x = (t_keyboard *)z;
     *xp1 = x->x_obj.te_xpix;
     *yp1 = x->x_obj.te_ypix;
     *xp2 = x->x_obj.te_xpix + x->width;
     *yp2 = x->x_obj.te_ypix + x->height;
}

// Erase the GUI
static void keyboard_erase(t_keyboard *x){
    sys_vgui(".x%lx.c delete %xrr\n", x->canvas, x);
    for(t_int i = 0 ; i < x->octaves * 12 ; i++)
        sys_vgui(".x%lx.c delete %xrrk%d\n", x->canvas, x, i);
}

// Draw the GUI
static void keyboard_draw(t_keyboard *x){
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %xrr -fill #FFFFFF\n", // Main rectangle
        x->canvas,
        x->x_obj.te_xpix,
        x->x_obj.te_ypix,
        x->x_obj.te_xpix + x->width,
        x->x_obj.te_ypix + x->height,
        x);
    int i, wcounter, bcounter;
    wcounter = bcounter = 0;
    for(i = 0 ; i < x->octaves * 12 ; i++){ // white keys 1st (blacks overlay it)
        short key = i % 12;
        if(key != 1 && key != 3 && key != 6 && key != 8 && key != 10){
            if(x->first_c + i == 60){ // Middle C
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags {%xrrk%d %xrr} -fill #F0FFFF\n",
                         x->canvas,
                         x->x_obj.te_xpix + wcounter * (int)x->space,
                         x->x_obj.te_ypix,
                         x->x_obj.te_xpix + (wcounter + 1) * (int)x->space,
                         x->x_obj.te_ypix + x->height,
                         x,
                         i,
                         x);
                wcounter++;
            }
            else{ // other keys
                sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags {%xrrk%d %xrr} -fill #FFFFFF\n",
                x->canvas,
                x->x_obj.te_xpix + wcounter * (int)x->space,
                x->x_obj.te_ypix,
                x->x_obj.te_xpix + (wcounter + 1) * (int)x->space,
                x->x_obj.te_ypix + x->height,
                x,
                i,
                x);
                wcounter++;
            }
        }
    }
    for(i = 0 ; i < x->octaves * 12 ; i++){ // Black keys
        short key = i % 12;
        if(key == 4 || key == 11){
            bcounter++;
            continue;
        }
        if(key == 1 || key == 3 || key ==6 || key == 8 || key == 10){
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags {%xrrk%d %xrr} -fill #000000\n",
            x->canvas,
            x->x_obj.te_xpix + ((bcounter + 1) * (int)x->space) - ((int)(0.3f * x->space)) ,
            x->x_obj.te_ypix,
            x->x_obj.te_xpix + ((bcounter + 1) * (int)x->space) + ((int)(0.3f * x->space)) ,
            x->x_obj.te_ypix + 2 * x->height / 3,
            x,
            i,
            x);
            bcounter++;
        }
    }
    canvas_fixlinesfor(x->glist, (t_text *)x);
}

// MAKE VISIBLE OR INVISIBLE
static void keyboard_vis(t_gobj *z, t_glist *glist, int vis){
    t_keyboard *x = (t_keyboard *)z;
    x->glist = glist;
    x->canvas = glist_getcanvas(glist);
    if(vis == 1){
        keyboard_draw(x);
        sys_vgui(".x%lx.c bind %xrr <ButtonPress-1> {\n keyboard_mousepress \"%d\" %%x %%y %%b\n}\n", x->canvas, x, x);
        sys_vgui(".x%lx.c bind %xrr <ButtonRelease-1> {\n keyboard_mouserelease \"%d\" %%x %%y %%b\n}\n", x->canvas, x, x);
        sys_vgui(".x%lx.c bind %xrr <B1-Motion> {\n keyboard_mousemotion \"%d\" %%x %%y\n}\n", x->canvas, x, x);
        sys_vgui(".x%lx.c bind %xrr <KeyPress> {\n keyboard_keydown \"%d\" %%N\n}\n", x->canvas, x, x);
        sys_vgui(".x%lx.c bind %xrr <KeyRelease> {\n keyboard_keyup \"%d\" %%N\n}\n", x->canvas, x, x);
    }
    else
        keyboard_erase(x);
}

// DISPLACE IT
void keyboard_displace(t_gobj *z, t_glist *glist,int dx, int dy){
    t_canvas *canvas = glist_getcanvas(glist);
    t_keyboard *x = (t_keyboard *)z;
    x->x_obj.te_xpix += dx; // x movement
    x->x_obj.te_ypix += dy; // y movement
    sys_vgui(".x%lx.c coords %xSEL %d %d %d %d \n", // MOVE THE BLUE ONE
        canvas, x,
        x->x_obj.te_xpix,
        x->x_obj.te_ypix,
        x->x_obj.te_xpix + x->width,
        x->x_obj.te_ypix + x->height
        );
    sys_vgui(".x%lx.c coords %xrr %d %d %d %d\n", // MOVE the main rectangle
        canvas, x,
        x->x_obj.te_xpix,
        x->x_obj.te_ypix,
        x->x_obj.te_xpix + x->width,
        x->x_obj.te_ypix + x->height
        );
    int i, wcounter, bcounter;
    wcounter = bcounter = 0;
    for(i = 0 ; i < x->octaves * 12 ; i++){ // MOVE THE KEYS
        short key = i % 12;
        if(key == 4 || key == 11) // Increment black keys counter
            bcounter++;
        if( key == 0 || key == 2 || key ==4 || key == 5 || key == 7 || key == 9 || key == 11){
        // Draw the white keys
            sys_vgui(".x%lx.c coords %xrrk%d %d %d %d %d \n",
                canvas, x, i,
                x->x_obj.te_xpix + wcounter * (int)x->space,
                x->x_obj.te_ypix,
                x->x_obj.te_xpix + (wcounter + 1) * (int)x->space,
                x->x_obj.te_ypix + x->height
                );
            wcounter++;
        }
        else{
            sys_vgui(".x%lx.c coords %xrrk%d %d %d %d %d \n",
                canvas, x, i,
                x->x_obj.te_xpix + ((bcounter + 1) * (int)x->space) - ((int)(0.3f * x->space)) ,
                x->x_obj.te_ypix,
                x->x_obj.te_xpix + ((bcounter + 1) * (int)x->space) + ((int)(0.3f * x->space)) ,
                x->x_obj.te_ypix + 2 * x->height / 3);
            bcounter++;
        }
    }
    canvas_fixlinesfor(glist, (t_text *)x);
}

// WHAT TO DO IF SELECTED?
static void keyboard_select(t_gobj *z, t_glist *glist, int state){
     t_keyboard *x = (t_keyboard *)z;
     t_canvas * canvas = glist_getcanvas(glist);
     if(state){
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %xSEL -outline blue\n",
        canvas,
        x->x_obj.te_xpix,
        x->x_obj.te_ypix,
        x->x_obj.te_xpix + x->width,
        x->x_obj.te_ypix + x->height,
        x);
    }
    else
        sys_vgui(".x%lx.c delete %xSEL\n",canvas, x);
}

// Delete the GUI
static void keyboard_delete(t_gobj *z, t_glist *glist){
    t_text *x = (t_text *)z;
    canvas_deletelinesfor(glist_getcanvas(glist), x);
} 

/* ------------------------ GUI Behaviour -----------------------------*/

// Set Properties
static void keyboard_set_properties(t_keyboard *x, t_floatarg space,
            t_floatarg height,t_floatarg octaves, t_floatarg low_c){
    x->height = (height < 10) ? 10 : height;
// clip octaves from 1 to 10
    if(octaves < 1)
        octaves = 1;
    if(octaves > 10)
        octaves = 10;
    x->octaves = octaves;
// clip from C0 to C8
    if(low_c < 0)
        low_c = 0;
    if(low_c > 8)
        low_c = 8;
    x->low_c = low_c;
// space/width
    x->space = (space < 7) ? 7 : space; // key width
    x->width = ((int)(x->space)) * 7 * (int)x->octaves;
// get first MIDI note
    x->first_c = ((int)(x->low_c * 12)) + 12;
// get and clear notes
    x->notes = getbytes(sizeof(t_int) * 12 * x->octaves);
    int i;
    for(i = 0; i < 12 * x->octaves; i++)
        x->notes[i] = 0;
}

// Keyboard Properties
void keyboard_properties(t_gobj *z, t_glist *owner){
    t_keyboard *x = (t_keyboard *)z;
    char cmdbuf[256];
    sprintf(cmdbuf, "keyboard_properties %%s %d %d %d %d\n",
        (int)x->space,
        (int)x->height,
        (int)x->octaves,
        (int)x->low_c);
    gfxstub_new(&x->x_obj.ob_pd, x, cmdbuf);
}

// Save Properties
static void keyboard_save(t_gobj *z, t_binbuf *b){
    t_keyboard *x = (t_keyboard *)z;
    binbuf_addv(b,
                "ssiisiiii",
                gensym("#X"),
                gensym("obj"),
                (t_int)x->x_obj.te_xpix,
                (t_int)x->x_obj.te_ypix,
                gensym("keyboard"),
                (t_int)x->space,
                (t_int)x->height,
                (t_int)x->octaves,
                (t_int)x->low_c);
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
static void keyboard_apply(t_keyboard *x, t_floatarg space, t_floatarg height,
        t_floatarg octaves, t_floatarg low_c){
    keyboard_erase(x);
    keyboard_set_properties(x, space, height, octaves, low_c);
    keyboard_draw(x);
}

/* ------------------------- Methods ------------------------------*/

void keyboard_float(t_keyboard *x, t_floatarg note){
    t_atom a[2];
    SETFLOAT(a, note);
    SETFLOAT(a+1, x->velocity_input);
    outlet_list(x->x_out, &s_list, 2, a);
    if(note > x->first_c && note < x->first_c + (x->octaves * 12)){
        x->notes[(int)(note - x->first_c)] = (x->velocity_input > 0) ? x->velocity_input : MOUSE_RELEASE;
        int i;
        for(i = 0 ; i < x->octaves * 12; i++){ // first dispatch note off
            short key = i % 12;
            if(x->notes[i] < 0){ // stop play Keyb or mouse
                if( key != 1 && key != 3 && key !=6 && key != 8 && key != 10){
                    if(x->first_c + i == 60) // Middle C
                        sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill #F0FFFF\n", x->canvas, x, i);
                    else
                        sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill #FFFFFF\n", x->canvas, x, i);
                }
                else
                    sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill #000000\n", x->canvas, x, i);
            }
        }
        for(i = 0 ; i < x->octaves * 12; i++){ // then dispatch note on
            short key = i % 12;
            if(x->notes[i] > 0){ // play Keyb or mouse
                if( key != 1 && key != 3 && key !=6 && key != 8 && key != 10)
                    sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill #9999FF\n", x->canvas, x, i);
                else
                    sys_vgui(".x%lx.c itemconfigure %xrrk%d -fill #6666FF\n", x->canvas, x, i);
            }
        }
    }
}

static void keyboard_oct(t_keyboard *x, t_floatarg f){
    f = (int)(f);
    if(f != 0){
        float target;
        if(x->low_c + f < 0)
            target = 0;
        else if(x->low_c + f > 8)
            target = 8;
        else
            target = x->low_c + f;
        if(x->low_c != target){
            keyboard_erase(x);
            keyboard_set_properties(x, x->space, x->height, x->octaves, target);
            keyboard_draw(x);
        }
    }
}

static void keyboard_height(t_keyboard *x, t_floatarg f){
    f = (int)(f);
    if(f < 10)
        f = 10;
    if(x->height != f){
        keyboard_erase(x);
        keyboard_set_properties(x, x->space, f, x->octaves, x->low_c);
        keyboard_draw(x);
    }
}

static void keyboard_width(t_keyboard *x, t_floatarg f){
    f = (int)(f);
    if(f < 7)
        f = 7;
    if(x->space != f){
        keyboard_erase(x);
        keyboard_set_properties(x, f, x->height, x->octaves, x->low_c);
        keyboard_draw(x);
    }
}

static void keyboard_8ves(t_keyboard *x, t_floatarg f){
    f = (int)(f);
    if(f > 10)
        f = 10;
    if(f < 1)
        f = 1;
    if(x->octaves != f){
        keyboard_erase(x);
        keyboard_set_properties(x, x->space, x->height, f, x->low_c);
        keyboard_draw(x);
    }
}

static void keyboard_low_c(t_keyboard *x, t_floatarg f){
    f = (int)(f);
    if(f > 8)
        f = 8;
    if(f < 0)
        f = 0;
    if(x->low_c != f){
        keyboard_erase(x);
        keyboard_set_properties(x, x->space, x->height, x->octaves, f);
        keyboard_draw(x);
    }
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
    t_float init_space = 17;
    t_float init_height = 80;
    t_float init_8ves = 4;
    t_float init_low_c = 3;
    if(ac) // 1st ARGUMENT: WIDTH
        init_space = atom_getfloat(av++), ac--;
    if(ac) // 2nd ARGUMENT: HEIGHT
        init_height = atom_getfloat(av++), ac--;
    if(ac) // 3rd ARGUMENT: Octaves
        init_8ves = atom_getfloat(av++), ac--;
    if(ac) // 4th ARGUMENT: Lowest C ("First C")
        init_low_c = atom_getfloat(av++), ac--;
    x->x_out = outlet_new(&x->x_obj, &s_list);
    floatinlet_new(&x->x_obj, &x->velocity_input);
// Set Parameters
    keyboard_set_properties(x, init_space, init_height, init_8ves, init_low_c);
    pd_bind(&x->x_obj.ob_pd, gensym("keyboard"));
    return(void *) x;
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
// Methods to receive TCL/TK events
    class_addmethod(keyboard_class, (t_method)keyboard_mousepress,gensym("_mousepress"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_mouserelease,gensym("_mouserelease"), A_FLOAT, A_FLOAT, A_FLOAT, 0); 
    class_addmethod(keyboard_class, (t_method)keyboard_mousemotion,gensym("_mousemotion"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_apply, gensym("apply"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
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
sys_vgui("\n");
sys_vgui("    global $var_width\n");
sys_vgui("    global $var_height\n");
sys_vgui("    global $var_octaves\n");
sys_vgui("    global $var_low_c\n");
sys_vgui("\n");
sys_vgui("    set cmd [concat $id apply \\\n");
sys_vgui("        [eval concat $$var_width] \\\n");
sys_vgui("        [eval concat $$var_height] \\\n");
sys_vgui("        [eval concat $$var_octaves] \\\n");
sys_vgui("        [eval concat $$var_low_c] \\;]\n");
sys_vgui("    pd $cmd\n");
sys_vgui("}\n");

sys_vgui("proc keyboard_properties {id width height octaves low_c} {\n");
sys_vgui("    set vid [string trimleft $id .]\n");
sys_vgui("    set var_width [concat var_width_$vid]\n");
sys_vgui("    set var_height [concat var_height_$vid]\n");
sys_vgui("    set var_octaves [concat var_octaves_$vid]\n");
sys_vgui("    set var_low_c [concat var_low_c_$vid]\n");
sys_vgui("\n");
sys_vgui("    global $var_width\n");
sys_vgui("    global $var_height\n");
sys_vgui("    global $var_octaves\n");
sys_vgui("    global $var_low_c\n");
sys_vgui("\n");
sys_vgui("    set $var_width $width\n");
sys_vgui("    set $var_height $height\n");
sys_vgui("    set $var_octaves $octaves\n");
sys_vgui("    set $var_low_c $low_c\n");
sys_vgui("\n");
sys_vgui("    toplevel $id\n");
sys_vgui("    wm title $id {Keyboard}\n");
sys_vgui("    wm protocol $id WM_DELETE_WINDOW [concat keyboard_cancel $id]\n");
sys_vgui("\n");
sys_vgui("    label $id.label -text {Keyboard}\n");
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
