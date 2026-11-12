// Prototype By Flávio Schiavoni, developed by Alexandre Porres

#include <m_pd.h>
#include <g_canvas.h>

#define BLACK_ON    "#FF0000"
#define BLACK_OFF   "#000000"
#define WHITE_ON    "#C40000"
#define WHITE_OFF   "#FFFFFF"
#define MIDDLE_C    "#7ADEFF" // color of Middle C when off

static t_class *keyboard_class, *edit_proxy_class;
static t_widgetbehavior keyboard_widgetbehavior;

typedef struct _edit_proxy{
    t_object    p_obj;
    t_symbol   *p_sym;
    t_clock    *p_clock;
    struct      _keyboard *p_cnv;
}t_edit_proxy;

typedef struct _keyboard{
    t_object       x_obj;
    t_glist       *x_glist;
    t_edit_proxy  *x_proxy;
    int           *x_tgl_notes;    // to store which notes should be played
    int            x_velocity;     // to store velocity
    int            x_last_note;    // to store last note
    float          x_vel_in;       // to store the second inlet values
    float          x_space;
    int            x_width;
    int            x_height;
    int            x_octaves;
    int            x_first_c;
    int            x_low_c;
    int            x_toggle_mode;
    int            x_norm;
    int            x_zoom;
    int            x_shift;
    int            x_xpos;
    int            x_ypos;
    int            x_snd_set;
    int            x_rcv_set;
    int            x_flag;
    int            x_s_flag;
    int            x_r_flag;
    int            x_edit;
    int            x_select;
    t_symbol      *x_receive;
    t_symbol      *x_rcv_raw;
    t_symbol      *x_send;
    t_symbol      *x_snd_raw;
    t_symbol      *x_bindsym;
    t_outlet      *x_out;
    char           x_tag_all[32];
    char           x_tag_main[32];
    char           x_tag_outline[32];
    char           x_tag_sel[32];
    char           x_tag_in1[32];
    char           x_tag_in2[32];
    char           x_tag_out[32];
    char           x_tag_IO[32];
}t_keyboard;

// forward decl (called from keyboard_draw)
static void keyboard_select(t_gobj *z, t_glist *gl, int sel);

// ------------------------- GUI tag helpers ------------------------------

static void keyboard_key_tag(t_keyboard *x, int i, char *buf, size_t len){
    snprintf(buf, len, "%pKEY%d", x, i);
}

// Repaint a single key with the given colour
static void keyboard_repaint_key(t_keyboard *x, int tag_index, int key_mod_12,
    int note, int on)
{
    char key_tag[32];
    keyboard_key_tag(x, tag_index, key_tag, sizeof(key_tag));
    int black = (key_mod_12 == 1 || key_mod_12 == 3 || key_mod_12 == 6 ||
                 key_mod_12 == 8 || key_mod_12 == 10);
    const char *fill;
    if(black)
        fill = on ? BLACK_ON : BLACK_OFF;
    else
        fill = on ? WHITE_ON : (note == 60 ? MIDDLE_C : WHITE_OFF);
    pdgui_vmess(0, "crs rs",
        glist_getcanvas(x->x_glist), "itemconfigure", key_tag,
        "-fill", fill);
}

// ------------------------- Keyboard Play ------------------------------
static void keyboard_note_on(t_keyboard* x, int note){
    int i = note - x->x_first_c;
    keyboard_repaint_key(x, i, note % 12, note, 1);
    t_atom at[2];
    SETFLOAT(at, note);
    SETFLOAT(at+1, x->x_velocity);
    outlet_list(x->x_out, &s_list, 2, at);
    if(x->x_send != &s_ && x->x_send->s_thing)
        pd_list(x->x_send->s_thing, &s_list, 2, at);
}

static void keyboard_note_off(t_keyboard* x, int note){
    int i = note - x->x_first_c;
    if(x->x_tgl_notes[note] == 0)
        keyboard_repaint_key(x, i, note % 12, note, 0);
    t_atom at[2];
    SETFLOAT(at, note);
    SETFLOAT(at+1, 0);
    outlet_list(x->x_out, &s_list, 2, at);
    if(x->x_send != &s_ && x->x_send->s_thing)
        pd_list(x->x_send->s_thing, &s_list, 2, at);
}

static void keyboard_get_snd_rcv(t_keyboard* x){
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
                int arg_n = 7; // receive argument number
                if(n_args > arg_n){ // we have it, get it
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
                int arg_n = 8; // receive argument number
                if(n_args > arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 80);
                    x->x_rcv_raw = gensym(buf);
                }
            }
        }
    }
    if(x->x_rcv_raw == &s_)
        x->x_rcv_raw = gensym("empty");
}

static void keyboard_play_tgl(t_keyboard* x, int note){ // TOGGLE MODE
    int i = note - x->x_first_c;
    int on = x->x_tgl_notes[note] = x->x_tgl_notes[note] ? 0 : 1;
    keyboard_repaint_key(x, i, i % 12, note, on);
    t_atom at[2];
    SETFLOAT(at, note);
    SETFLOAT(at+1, on ? x->x_velocity : 0);
    outlet_list(x->x_out, &s_list, 2, at);
    if(x->x_send != &s_ && x->x_send->s_thing)
        pd_list(x->x_send->s_thing, &s_list, 2, at);
}

// -------------------- MOUSE Events ----------------------------------
static int find_note(t_keyboard* x, float xpos, float ypos){
    int sz = (int)x->x_space;
    int b_sz = (int)(x->x_space / 3.f);
    int white_key = (int)(xpos / x->x_space) % 7;
    int white_note = (white_key * 2) - (white_key > 2);
    int oct = (int)((xpos / x->x_space) / 7);
    int i = white_note + oct*12;
    if(ypos < x->x_height*2/3){ // find black keys
        x->x_velocity = x->x_norm > 0 ? x->x_norm : (int)((ypos / (x->x_height*2/3)) * 127);
        if(x->x_velocity <= 0)
            x->x_velocity = 1;
        if(white_key == 0 || white_key == 3){ // find sharp
            if(xpos > (white_key+1 + oct*7) * sz - b_sz)
                return(x->x_first_c+i+1);
        }
        else if(white_key == 2 || white_key == 6){ // find flat
            if(xpos < (white_key + oct*7) * sz + b_sz)
                return(x->x_first_c+i-1);
        }
        else if(white_key == 1 || white_key == 4 || white_key == 5){ // find both
            if(xpos < (white_key + oct*7) * sz + b_sz)
                return(x->x_first_c+i-1);
            else if(xpos > (white_key+1 + oct*7) * sz - b_sz)
                return(x->x_first_c+i+1);
        }
    }
    x->x_velocity = x->x_norm > 0 ? x->x_norm : (int)((ypos / x->x_height) * 127);
    if(x->x_velocity < 1)
        x->x_velocity = 1;
    if(x->x_velocity > 127)
        x->x_velocity = 127;
    return(x->x_first_c+i);
}

static void keyboard_motion(t_keyboard *x, t_floatarg dx, t_floatarg dy){
    if(x->x_toggle_mode || x->x_shift || x->x_glist->gl_edit) // ignore if toggle or edit mode!
        return;
    x->x_xpos = (x->x_xpos + dx);
    x->x_ypos = (x->x_ypos + dy);
    if(x->x_xpos < 0 || x->x_xpos >= x->x_width * x->x_zoom) // ignore if out of bounds
        return;
    int note = find_note(x, x->x_xpos / x->x_zoom, x->x_ypos / x->x_zoom);
    if(note != x->x_last_note){
        keyboard_note_off(x, x->x_last_note);
        keyboard_note_on(x, x->x_last_note = note);
    }
}

static int keyboard_click(t_keyboard *x, t_glist *gl, int xpos, int ypos, int shift, int alt, int dbl, int doit){
    alt = dbl = 0; // remove warning
    if(doit){
        x->x_xpos = (xpos - text_xpix(&x->x_obj, gl));
        x->x_ypos = (ypos - text_ypix(&x->x_obj, gl));
        glist_grab(gl, &x->x_obj.te_g, (t_glistmotionfn)keyboard_motion, 0, xpos, ypos);
        int note = find_note(x, x->x_xpos / x->x_zoom, x->x_ypos / x->x_zoom);
        x->x_toggle_mode || (x->x_shift = shift) ? keyboard_play_tgl(x, note) : keyboard_note_on(x, x->x_last_note = note);
    }
    return(1);
}

static void keyboard_mouserelease(t_keyboard* x){
    if(x->x_toggle_mode || x->x_shift || x->x_glist->gl_edit) // ignore if toggle/edit mode!
        return;
    keyboard_note_off(x, x->x_last_note);
}

// ------------------------ GUI SHIT-----------------------------
static void keyboard_erase(t_keyboard *x, t_glist *glist){
    pdgui_vmess(0, "crs",
        glist_getcanvas(glist), "delete", x->x_tag_all);
}

static void keyboard_draw_io_let(t_keyboard *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
    if(x->x_edit && x->x_receive == &s_){
        char *tags1[] = {x->x_tag_in1, x->x_tag_IO, x->x_tag_all};
        pdgui_vmess(0, "crr iiii rk rk rS", cv, "create", "rectangle",
            xpos, ypos, xpos+(IOWIDTH*x->x_zoom), ypos+(IHEIGHT*x->x_zoom),
            "-fill", THISGUI->i_foregroundcolor,
            "-outline", THISGUI->i_foregroundcolor,
            "-tags", 3, tags1);
        char *tags2[] = {x->x_tag_in2, x->x_tag_IO, x->x_tag_all};
        pdgui_vmess(0, "crr iiii rk rk rS", cv, "create", "rectangle",
            xpos+x->x_width*x->x_zoom, ypos,
            xpos+x->x_width*x->x_zoom-(IOWIDTH*x->x_zoom), ypos+(IHEIGHT*x->x_zoom),
            "-fill", THISGUI->i_foregroundcolor,
            "-outline", THISGUI->i_foregroundcolor,
            "-tags", 3, tags2);
    }
    if(x->x_edit && x->x_send == &s_){
        char *tags3[] = {x->x_tag_out, x->x_tag_IO, x->x_tag_all};
        pdgui_vmess(0, "crr iiii rk rk rS", cv, "create", "rectangle",
            xpos, ypos+x->x_height*x->x_zoom,
            xpos+IOWIDTH*x->x_zoom, ypos+x->x_height*x->x_zoom-IHEIGHT*x->x_zoom,
            "-fill", THISGUI->i_foregroundcolor,
            "-outline", THISGUI->i_foregroundcolor,
            "-tags", 3, tags3);
    }
    keyboard_select((t_gobj *)x, x->x_glist, x->x_select);
}

static void keyboard_draw(t_keyboard *x, t_glist* glist){
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    t_canvas *cv = glist_getcanvas(x->x_glist);
    // Main rectangle: carries x_tag_outline (unique outer border) and
    // x_tag_main (shared with keys for the ButtonRelease binding).
    char *tags_main[] = {x->x_tag_main, x->x_tag_outline, x->x_tag_all};
    pdgui_vmess(0, "crr iiii rS", cv, "create", "rectangle",
        xpos, ypos, xpos + x->x_width*x->x_zoom, ypos + x->x_height*x->x_zoom,
        "-tags", 3, tags_main);
    int i, wcount, bcount;
    wcount = bcount = 0;
    for(i = 0 ; i < x->x_octaves * 12 ; i++){ // white keys 1st (blacks overlay it)
        short key = i % 12;
        if(key != 1 && key != 3 && key != 6 && key != 8 && key != 10){
            int on = x->x_tgl_notes[x->x_first_c+i];
            int c4 = x->x_first_c + i == 60;
            char key_tag[32];
            keyboard_key_tag(x, i, key_tag, sizeof(key_tag));
            char *tags[] = {key_tag, x->x_tag_main, x->x_tag_all};
            pdgui_vmess(0, "crr iiii rs rS", cv, "create", "rectangle",
                xpos + wcount * (int)x->x_space*x->x_zoom,
                ypos,
                xpos + (wcount + 1) * (int)x->x_space*x->x_zoom,
                ypos + x->x_height*x->x_zoom,
                "-fill", on ? WHITE_ON : c4 ? MIDDLE_C : WHITE_OFF,
                "-tags", 3, tags);
            wcount++;
        }
    }
    for(i = 0 ; i < x->x_octaves * 12 ; i++){ // Black keys
        short key = i % 12;
        if(key == 4 || key == 11){
            bcount++;
            continue;
        }
        if(key == 1 || key == 3 || key ==6 || key == 8 || key == 10){
            int on = x->x_tgl_notes[x->x_first_c+i];
            char key_tag[32];
            keyboard_key_tag(x, i, key_tag, sizeof(key_tag));
            char *tags[] = {key_tag, x->x_tag_main, x->x_tag_all};
            pdgui_vmess(0, "crr iiii rs rS", cv, "create", "rectangle",
                xpos + ((bcount + 1) * (int)x->x_space)*x->x_zoom - ((int)(x->x_space*x->x_zoom / 3.f)),
                ypos,
                xpos + ((bcount + 1) * (int)x->x_space)*x->x_zoom + ((int)(x->x_space*x->x_zoom / 3.f)),
                ypos + x->x_height*x->x_zoom * 2/3,
                "-fill", on ? BLACK_ON : BLACK_OFF,
                "-tags", 3, tags);
            bcount++;
        }
    }
    canvas_fixlinesfor(x->x_glist, (t_text *)x);
    keyboard_draw_io_let(x);
    // Selection overlay: drawn LAST so it sits on top of every key and IO
    // rect (otherwise the keys' own outlines cover the bottom border of the
    // main rect when the widget is selected). Created hidden; keyboard_select
    // toggles it visible with the select colour when the widget is selected.
    char *tags_sel[] = {x->x_tag_sel, x->x_tag_all};
    pdgui_vmess(0, "crr iiii rs ri rs rS", cv, "create", "rectangle",
        xpos, ypos, xpos + x->x_width*x->x_zoom, ypos + x->x_height*x->x_zoom,
        "-outline", "",
        "-width", 2,
        "-state", "hidden",
        "-tags", 2, tags_sel);
    // re-apply selection appearance to the freshly drawn overlay
    keyboard_select((t_gobj *)x, glist, x->x_select);
}

// ------------------------ widgetbehaviour-----------------------------
static void keyboard_getrect(t_gobj *z, t_glist *owner, int *xp1, int *yp1, int *xp2, int *yp2){
    t_keyboard *x = (t_keyboard *)z;
    *xp1 = text_xpix(&x->x_obj, owner);
    *yp1 = text_ypix(&x->x_obj, owner) ;
    *xp2 = text_xpix(&x->x_obj, owner) + x->x_width*x->x_zoom;
    *yp2 = text_ypix(&x->x_obj, owner) + x->x_height*x->x_zoom;
}

void keyboard_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_keyboard *x = (t_keyboard *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    pdgui_vmess(0, "crs ii",
        glist_getcanvas(glist), "move", x->x_tag_all, dx*x->x_zoom, dy*x->x_zoom);
    canvas_fixlinesfor(glist, (t_text *)x);
}

static void keyboard_select(t_gobj *z, t_glist *gl, int sel){
    t_keyboard *x = (t_keyboard *)z;
    t_canvas *cv = glist_getcanvas(gl);
    x->x_select = sel;
    pdgui_vmess(0, "crs rk",
        cv, "itemconfigure", x->x_tag_outline,
        "-outline", sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
    if(sel){
        pdgui_vmess(0, "crs rk ri rs",
            cv, "itemconfigure", x->x_tag_sel,
            "-outline", THISGUI->i_selectcolor,
            "-width", 2,
            "-state", "normal");
    } else {
        pdgui_vmess(0, "crs rs",
            cv, "itemconfigure", x->x_tag_sel,
            "-state", "hidden");
    }
    pdgui_vmess(0, "crs rk rk",
        cv, "itemconfigure", x->x_tag_IO,
        "-outline", sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor,
        "-fill",    sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
}

static void keyboard_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void keyboard_vis(t_gobj *z, t_glist *glist, int vis){
    t_keyboard *x = (t_keyboard *)z;
    if(vis){
        keyboard_draw(x, glist);
        char buf[512];
        snprintf(buf, sizeof(buf),
            ".x%lx.c bind %s <ButtonRelease> {pdsend [concat %s _mouserelease \\;]}",
            (unsigned long)glist_getcanvas(glist), x->x_tag_main, x->x_bindsym->s_name);
        pdgui_vmess(buf, NULL);
    }
    else
        keyboard_erase(x, glist);
}

// ------------------------- Methods ------------------------------
void keyboard_float(t_keyboard *x, t_floatarg f){
    int note = (int)f;
    if(note < 0 || note >= 255) return;
    if(x->x_vel_in < 0)
        x->x_vel_in = 0;
    if(x->x_vel_in > 127)
        x->x_vel_in = 127;
    int on = x->x_tgl_notes[note] = x->x_vel_in > 0;
    t_atom at[2];
    SETFLOAT(at, note);
    SETFLOAT(at+1, x->x_vel_in);
    outlet_list(x->x_out, &s_list, 2, at);
    if(x->x_send != &s_ && x->x_send->s_thing)
        pd_list(x->x_send->s_thing, &s_list, 2, at);
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
        if(note >= x->x_first_c && note < x->x_first_c + (x->x_octaves * 12)){
            int i = note - x->x_first_c;
            keyboard_repaint_key(x, i, i % 12, note, on);
        }
    }
}

static void keyboard_set(t_keyboard *x, t_floatarg f1, t_floatarg f2){
    int note = (int)f1;
    if(note < 0 || note >= 255) return;

    x->x_vel_in = f2 < 0 ? 0 : f2 > 127 ? 127 : (int)f2;
    int on = x->x_tgl_notes[note] = x->x_vel_in > 0;
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
        if(note >= x->x_first_c && note < x->x_first_c + (x->x_octaves * 12)){
            int i = note - x->x_first_c;
            keyboard_repaint_key(x, i, i % 12, note, on);
        }
    }
}

static void keyboard_show(t_keyboard *x,  t_symbol *s, int ac, t_atom *av){
    s = NULL;
    for(int note = 0; note < 128; note++){
        if(x->x_tgl_notes[note] > 0){
            x->x_tgl_notes[note] = 0;
            int i = note - x->x_first_c;
            if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
                keyboard_repaint_key(x, i, i % 12, note, 0);
        }
    }
    for(int i = 0; i < ac; i++){
        int note = atom_getint(av++);
        int on = x->x_tgl_notes[note] = 1;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
            if(note >= x->x_first_c && note < x->x_first_c + (x->x_octaves * 12)){
                int j = note - x->x_first_c;
                keyboard_repaint_key(x, j, i % 12, note, on);
            }
        }
    }
}

static void keyboard_flush(t_keyboard* x){
    t_atom at[2];
    for(int note = 0; note < 128; note++){
        if(x->x_tgl_notes[note] > 0){
            int i = note - x->x_first_c;
            if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
                keyboard_repaint_key(x, i, i % 12, note, 0);
            SETFLOAT(at, note);
            SETFLOAT(at+1, x->x_tgl_notes[note] = 0);
            outlet_list(x->x_out, &s_list, 2, at);
            if(x->x_send != &s_ && x->x_send->s_thing)
                pd_list(x->x_send->s_thing, &s_list, 2, at);
        }
    }
}

static void keyboard_on(t_keyboard *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    while(ac > 0){
        x->x_vel_in = 127;
        keyboard_float(x, atom_getfloatarg(0, ac--, av++));
    }
}

static void keyboard_off(t_keyboard *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    while(ac > 0){
        x->x_vel_in = 0;
        keyboard_float(x, atom_getfloatarg(0, ac--, av++));
    }
}

static void keyboard_play(t_keyboard *x, t_symbol *s, int ac, t_atom *av){
    if(!ac)
        return;
    keyboard_flush(x);
    keyboard_on(x, s, ac, av);
}

static void keyboard_height(t_keyboard *x, t_floatarg f){
    f =  f < 10 ? 10 : (int)(f);
    if(x->x_height != f){
        x->x_height = f;
        keyboard_erase(x, x->x_glist);
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
            keyboard_draw(x, x->x_glist);
            canvas_fixlinesfor(x->x_glist, (t_text *)x);
        }
    }
}

static void keyboard_width(t_keyboard *x, t_floatarg f){
    f = f < 7 ? 7 : (int)(f);
    if(x->x_space != f){
        x->x_space = f;
        keyboard_erase(x, x->x_glist);
        x->x_width = ((int)(x->x_space)) * 7 * (int)x->x_octaves;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            keyboard_draw(x, x->x_glist);
    }
}

static void keyboard_8ves(t_keyboard *x, t_floatarg f){
    f = f > 10 ? 10 : f < 1 ? 1 : (int)(f);
    if(x->x_octaves != f){
        x->x_octaves = f;
        x->x_width = x->x_space * 7 * x->x_octaves;
        keyboard_erase(x, x->x_glist);
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            keyboard_draw(x, x->x_glist);
    }
}

static void keyboard_low_c(t_keyboard *x, t_floatarg f){
    f = f > 8 ? 8 : f < 0 ? 0 : (int)(f);
    if(x->x_low_c != f){
        x->x_first_c = ((int)((x->x_low_c = f) * 12)) + 12;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            keyboard_erase(x, x->x_glist), keyboard_draw(x, x->x_glist);
    }
}

static void keyboard_oct(t_keyboard *x, t_floatarg f){
    keyboard_low_c(x, x->x_low_c + (int)(f));
}

static void keyboard_toggle(t_keyboard *x, t_floatarg f){
    int tgl = f != 0;
    if(tgl != x->x_toggle_mode)
        x->x_toggle_mode = tgl;
}

static void keyboard_norm(t_keyboard *x, t_floatarg f){
    int norm = f < 0 ? 0 : f > 127 ? 127 : (int)f;
    if(norm != x->x_norm)
        x->x_norm = norm;
}

static void keyboard_send(t_keyboard *x, t_symbol *s){
    if(s != gensym("")){
        t_symbol *snd = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
        if(snd != x->x_send){
            x->x_snd_raw = s;
            x->x_send = snd;
            x->x_snd_set = 1;
            if(x->x_edit && glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
                if(x->x_send == &s_)
                    keyboard_draw_io_let(x);
                else
                    pdgui_vmess(0, "crs",
                        glist_getcanvas(x->x_glist), "delete", x->x_tag_out);
            }
        }
    }
}

static void keyboard_receive(t_keyboard *x, t_symbol *s){
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
                    keyboard_draw_io_let(x);
            }
            else{
                pd_bind(&x->x_obj.ob_pd, x->x_receive);
                if(x->x_edit && glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
                    t_canvas *cv = glist_getcanvas(x->x_glist);
                    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_in1);
                    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_in2);
                }
            }
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
            if(edit)
                keyboard_draw_io_let(p->p_cnv);
            else{
                t_canvas *cv = glist_getcanvas(p->p_cnv->x_glist);
                pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_in1);
                pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_in2);
                pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_out);
            }
        }
    }
}

static void keyboard_zoom(t_keyboard *x, t_floatarg zoom){
    x->x_zoom = (int)zoom;
}

// ------------------------ GUI Behaviour -----------------------------
void keyboard_properties(t_gobj *z, t_glist *owner){
    (void)owner;
    t_keyboard *x = (t_keyboard *)z;
    keyboard_get_snd_rcv(x);
    pdgui_stub_vnew(&x->x_obj.ob_pd, "keyboard_dialog", x,
        "iiiiii ss",
        (int)x->x_space,
        (int)x->x_height,
        (int)x->x_octaves,
        (int)x->x_low_c,
        (int)x->x_norm,
        (int)x->x_toggle_mode,
        x->x_snd_raw->s_name,
        x->x_rcv_raw->s_name);
}

static void keyboard_save(t_gobj *z, t_binbuf *b){
    t_keyboard *x = (t_keyboard *)z;
    keyboard_get_snd_rcv(x);
    binbuf_addv(b,
        "ssiisiiiiiiss",
        gensym("#X"),
        gensym("obj"),
        (int)x->x_obj.te_xpix,
        (int)x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)),
        (int)x->x_space,
        (int)x->x_height,
        (int)x->x_octaves,
        (int)x->x_low_c,
        (int)x->x_toggle_mode,
        (int)x->x_norm,
        x->x_snd_raw,
        x->x_rcv_raw);
        binbuf_addv(b, ";");
}

static void keyboard_apply(t_keyboard *x, t_symbol *s, int ac, t_atom *av){ // Apply property windows
    (void)s;
    t_atom undo[8];
    SETFLOAT(undo+0, (int)x->x_space);
    SETFLOAT(undo+1, (int)x->x_height);
    SETFLOAT(undo+2, x->x_octaves);
    SETFLOAT(undo+3, x->x_low_c);
    SETFLOAT(undo+4, x->x_norm);
    SETFLOAT(undo+5, x->x_toggle_mode);
    SETSYMBOL(undo+6, x->x_snd_raw);
    SETSYMBOL(undo+7, x->x_rcv_raw);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dialog"), 8, undo, ac, av);
    float w = atom_getfloatarg(0, ac, av);
    float h = atom_getfloatarg(1, ac, av);
    float oct = atom_getfloatarg(2, ac, av);
    float low_c = atom_getfloatarg(3, ac, av);
    float norm = atom_getfloatarg(4, ac, av);
    float tgl = (atom_getfloatarg(5, ac, av) != 0);
    t_symbol *snd = atom_getsymbolarg(6, ac, av);
    t_symbol *rcv = atom_getsymbolarg(7, ac, av);
    int changed = 0;
    if(w < 7)
        w = 7;
    if(x->x_space != w){
        changed = 1;
        x->x_space = w;
    }
    if(h < 10)
        h = 10;
    if(x->x_height != h){
        changed = 1;
        x->x_height = h;
    }
    oct = oct < 1 ? 1 : oct > 10 ? 10 : oct;
    if(x->x_octaves != oct){
        changed = 1;
        x->x_octaves = oct;
    }
    low_c = low_c < 0 ? 0 : low_c > 8 ? 8 : low_c;
    if(x->x_low_c != low_c){
        changed = 1;
        x->x_low_c = low_c;
    }
    norm = norm < 0 ? 0 : norm > 127 ? 127 : norm;
    if(x->x_norm != norm){
        changed = 1;
        x->x_norm = norm;
    }
    if(x->x_toggle_mode != tgl){
        changed = 1;
        x->x_toggle_mode = tgl;
    }
    if(x->x_snd_raw != snd)
        changed = 1;
    if(x->x_rcv_raw != rcv)
        changed = 1;
    if(changed){
        x->x_width = ((int)(x->x_space)) * 7 * (int)x->x_octaves;
        x->x_first_c = ((int)(x->x_low_c * 12)) + 12;
        keyboard_erase(x, x->x_glist), keyboard_draw(x, x->x_glist);
        keyboard_send(x, snd);
        keyboard_receive(x, rcv);
        canvas_dirty(x->x_glist, 1);
    }
}

// ------------------------ Free / New / Setup ------------------------------
static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy * edit_proxy_new(t_keyboard *x, t_symbol *s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

void keyboard_free(t_keyboard *x){
    if(x->x_receive != &s_)
        pd_unbind(&x->x_obj.ob_pd, x->x_receive);
    pd_unbind(&x->x_obj.ob_pd, x->x_bindsym);
    x->x_proxy->p_cnv = NULL;
    clock_delay(x->x_proxy->p_clock, 0);
    pdgui_stub_deleteforkey(x);
}

void * keyboard_new(t_symbol *s, int ac, t_atom* av){
    s = NULL;
    t_keyboard *x = (t_keyboard *) pd_new(keyboard_class);
    t_canvas *cv = canvas_getcurrent();
    x->x_glist = (t_glist*)cv;
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)cv);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindsym = gensym(buf));
    x->x_edit = cv->gl_edit;
    x->x_select = 0;
    x->x_zoom = x->x_glist->gl_zoom;
    x->x_last_note = -1;
    x->x_velocity = 0;
    x->x_send = x->x_snd_raw = x->x_receive = x->x_rcv_raw = &s_;
    int tgl = 0;
    int vel = 0;
    float init_space = 17;
    float init_height = 80;
    float init_8ves = 4;
    float init_low_c = 2;
    t_symbol *snd = &s_;
    t_symbol *rcv = &s_;
    if(ac && av->a_type == A_FLOAT){ // 1st Width
        init_space = av->a_w.w_float;
        ac--; av++;
        if(ac && av->a_type == A_FLOAT){ // 2nd Height
            init_height = av->a_w.w_float;
            ac--, av++;
            if(ac && av->a_type == A_FLOAT){ // 3rd Octaves
                init_8ves = av->a_w.w_float;
                ac--; av++;
                if(ac && av->a_type == A_FLOAT){ // 4th Low C
                    init_low_c = av->a_w.w_float;
                    ac--; av++;
                    if(ac && av->a_type == A_FLOAT){ // 5th Toggle
                        tgl = (int)(av->a_w.w_float != 0);
                        ac--; av++;
                        if(ac && av->a_type == A_FLOAT){ // 6th Norm
                            vel = (int)(av->a_w.w_float);
                            ac--; av++;
                            if(ac && av->a_type == A_SYMBOL){ // 7th send
                                if(av->a_w.w_symbol == gensym("empty")) // ignore empty symbol
                                    ac--, av++;
                                else{
                                    snd = av->a_w.w_symbol;
                                    ac--, av++;
                                }
                                if(ac && av->a_type == A_SYMBOL){ // 8th receive
                                    if(av->a_w.w_symbol == gensym("empty")) // ignore empty symbol
                                        ac--, av++;
                                    else{
                                        rcv = av->a_w.w_symbol;
                                        ac--, av++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    t_symbol *cursym = NULL, *sym = NULL;
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            cursym = atom_getsymbolarg(0, ac, av);
            if(cursym == gensym("-width")){
                x->x_flag = 1;
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    init_space = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(cursym == gensym("-height")){
                x->x_flag = 1;
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    init_height = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(cursym == gensym("-oct")){
                x->x_flag = 1;
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    init_8ves = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            
            else if(cursym == gensym("-lowc")){
                x->x_flag = 1;
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    init_low_c = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(cursym == gensym("-tgl")) {
                x->x_flag = tgl = 1;
                ac-=1, av+=1;
            }
            else if(cursym == gensym("-norm")){
                x->x_flag = 1;
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    vel = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(cursym == gensym("-send")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    sym = atom_getsymbolarg(1, ac, av);
                    x->x_flag = x->x_s_flag = 1;
                    if(sym != gensym("empty"))
                        snd = sym;
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(cursym == gensym("-receive")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    sym = atom_getsymbolarg(1, ac, av);
                    x->x_flag = x->x_r_flag = 1;
                    if(sym != gensym("empty"))
                        rcv = sym;
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else goto errstate;
        }
        else goto errstate;
    };
    x->x_snd_raw = snd;
    x->x_send = canvas_realizedollar(x->x_glist, x->x_snd_raw);
    x->x_rcv_raw = rcv;
    x->x_receive = canvas_realizedollar(x->x_glist, x->x_rcv_raw);
    if(x->x_receive != &s_)
        pd_bind(&x->x_obj.ob_pd, x->x_receive);
    x->x_space = (init_space < 7) ? 7 : init_space; // key width
    x->x_height = (init_height < 10) ? 10 : init_height;
    x->x_octaves = init_8ves < 1 ? 1 : init_8ves > 10 ? 10 : init_8ves;
    x->x_low_c = init_low_c < 0 ? 0 : init_low_c > 8 ? 8 : init_low_c;
    x->x_norm = vel < 0 ? 0 : vel > 127 ? 127 : vel;
    x->x_toggle_mode = (tgl != 0);
    x->x_width = ((int)(x->x_space)) * 7 * (int)x->x_octaves;
    x->x_first_c = ((int)(x->x_low_c * 12)) + 12;
    // tags
    snprintf(x->x_tag_all,     sizeof(x->x_tag_all),     "%pALL",     x);
    snprintf(x->x_tag_main,    sizeof(x->x_tag_main),    "%pRR",      x);
    snprintf(x->x_tag_outline, sizeof(x->x_tag_outline), "%pOUTLINE", x);
    snprintf(x->x_tag_sel,     sizeof(x->x_tag_sel),     "%pSEL",     x);
    snprintf(x->x_tag_in1,     sizeof(x->x_tag_in1),     "%pIN1",     x);
    snprintf(x->x_tag_in2,     sizeof(x->x_tag_in2),     "%pIN2",     x);
    snprintf(x->x_tag_out,     sizeof(x->x_tag_out),     "%pOUT",     x);
    snprintf(x->x_tag_IO,      sizeof(x->x_tag_IO),      "%pIO",      x);
    x->x_tgl_notes = getbytes(sizeof(int) * 256);
    for(int i = 0; i < 256; i++)
        x->x_tgl_notes[i] = 0;
    x->x_out = outlet_new(&x->x_obj, &s_list);
    floatinlet_new(&x->x_obj, &x->x_vel_in);
    return(void *)x;
    errstate:
        pd_error(x, "[keyboard]: improper args");
        return(NULL);
}

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
    class_addmethod(keyboard_class, (t_method)keyboard_norm, gensym("norm"), A_DEFFLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_flush, gensym("flush"), 0);
    class_addmethod(keyboard_class, (t_method)keyboard_send, gensym("send"), A_DEFSYMBOL, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_receive, gensym("receive"), A_DEFSYMBOL, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_on, gensym("on"), A_GIMME, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_set, gensym("set"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_show, gensym("show"), A_GIMME, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_play, gensym("play"), A_GIMME, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_off, gensym("off"), A_GIMME, 0);
    class_addmethod(keyboard_class, (t_method)keyboard_zoom, gensym("zoom"), A_CANT, 0);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
// Methods to receive TCL/TK events
    class_addmethod(keyboard_class, (t_method)keyboard_mouserelease, gensym("_mouserelease"), 0);
    class_addmethod(keyboard_class, (t_method)keyboard_apply, gensym("dialog"), A_GIMME, 0);
// GUI
    class_setwidget(keyboard_class, &keyboard_widgetbehavior);
    keyboard_widgetbehavior.w_getrectfn  = keyboard_getrect;
    keyboard_widgetbehavior.w_displacefn = keyboard_displace;
    keyboard_widgetbehavior.w_selectfn   = keyboard_select;
    keyboard_widgetbehavior.w_deletefn   = keyboard_delete;
    keyboard_widgetbehavior.w_visfn      = keyboard_vis;
    keyboard_widgetbehavior.w_clickfn    = (t_clickfn)keyboard_click;
    class_setsavefn(keyboard_class, keyboard_save);
    class_setpropertiesfn(keyboard_class, keyboard_properties);
    #include "../Extra/keyboard_dialog.h"
    pdgui_vmess(keyboard_dialog_tcl, NULL);
}
