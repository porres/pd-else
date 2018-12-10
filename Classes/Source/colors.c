
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <m_pd.h>
#include <m_imp.h>
static t_class *colors_class;

typedef struct _colors{
    t_object    x_obj;
    t_int       x_hex;
    t_int       x_gui;
    t_int       x_rgb;
    t_int       x_ds;
    t_symbol   *x_id;
    char        x_color[MAXPDSTRING];
}t_colors;

static void colors_pick(t_colors *x){
    sys_vgui("after idle [list after 10 else_colors_pick %s %s]\n", x->x_id->s_name, x->x_color);
}

static void colors_bang(t_colors *x){
//    post("bang, hex = %d", x->x_hex);
    if(x->x_hex)
        outlet_symbol(x->x_obj.ob_outlet, gensym(x->x_color));
    else{ // RGB related
        unsigned int red, green, blue;
        sscanf(x->x_color, "#%02x%02x%02x", &red, &green, &blue);
        if(x->x_rgb){
            t_atom at[3];
            SETFLOAT(at, red);
            SETFLOAT(at+1, green);
            SETFLOAT(at+2, blue);
            outlet_list(x->x_obj.ob_outlet, &s_list, 3, at);
        }
        else if(x->x_gui)
            outlet_float(x->x_obj.ob_outlet, -(float)(red*65536 + green*256 + blue) - 1);
        else if(x->x_ds){
            t_float ds = 100*(int)((float)red*8/255 + 0.5)
                        + 10*(int)((float)green*8/255 + 0.5)
                        + (int)((float)blue*8/255 + 0.5);
            outlet_float(x->x_obj.ob_outlet, ds);
            
            red = (int)(ds/100);
            if(red > 8)
                red = 8;
            red = (int)((float)red * 255./8. + 0.5);
            
            green = (int)(fmod(ds, 100) / 10);
            if(green > 8)
                green = 8;
            green = (int)((float)green * 255./8. + 0.5);
            
            blue = (int)(fmod(ds, 10));
            if(blue > 8)
                blue = 8;
            blue = (int)((float)blue * 255./8. + 0.5);
            
            char hex[MAXPDSTRING];
            sprintf(hex, "#%02x%02x%02x", red, green, blue);
            strncpy(x->x_color, hex, 7);
        }
    }
}

static void colors_convert_to(t_colors *x, t_symbol *s){
    if(s == gensym("rgb")){
        x->x_ds = x->x_gui = x->x_hex = 0;
        x->x_rgb = 1;
    }
    else if(s == gensym("hex")){
        x->x_ds = x->x_gui = x->x_rgb = 0;
        x->x_hex = 1;
    }
    else if(s == gensym("ds")){
        x->x_hex = x->x_gui = x->x_rgb = 0;
        x->x_ds = 1;
    }
    else if(s == gensym("gui")){
        x->x_hex = x->x_ds = x->x_rgb = 0;
        x->x_gui = 1;
    }
    else
        post("[colors]: can't convert to %s", s);
}

static void colors_hex(t_colors *x, t_symbol *s){
    strncpy(x->x_color, s->s_name, 7);
    colors_bang(x);
}

static void colors_ds(t_colors *x, t_floatarg ds){
    t_float red = (int)(ds/100);
    if(red > 8)
        red = 8;
    unsigned int r = (int)(red * 255./8. + 0.5);
    
    t_float green = (int)(fmod(ds, 100) / 10);
    if(green > 8)
        green = 8;
    unsigned int g = (int)(green * 255./8. + 0.5);
    
    t_float blue = (int)(fmod(ds, 10));
    if(blue > 8)
        blue = 8;
    unsigned int b = (int)(blue * 255./8. + 0.5);
    
    char hex[MAXPDSTRING];
    sprintf(hex, "#%02x%02x%02x", r, g, b);
    strncpy(x->x_color, hex, 7);
    
    colors_bang(x);
}

static void colors_gui(t_colors *x, t_floatarg gui){
    gui = -(gui + 1);
    unsigned int red = (unsigned int)(gui / 65536);
    unsigned int green = (unsigned int)(fmod(gui, 65536) / 255);
    unsigned int blue = (unsigned int)(fmod(gui, 256));
    char hex[MAXPDSTRING];
    sprintf(hex, "#%02x%02x%02x", red, green, blue);
    strncpy(x->x_color, hex, 7);
    colors_bang(x);
}

static void colors_rgb(t_colors *x, t_floatarg r, t_floatarg g, t_floatarg b){
    unsigned int red = (unsigned int)(r > 255 ? 255 : r < 0 ? 0 : r);
    unsigned int green = (unsigned int)(g > 255 ? 255 : g < 0 ? 0 : g);
    unsigned int blue = (unsigned int)(b > 255 ? 255 : b < 0 ? 0 : b);
    char hex[MAXPDSTRING];
    sprintf(hex, "#%02x%02x%02x", red, green, blue);
    strncpy(x->x_color, hex, 7);
    colors_bang(x);
}

static void colors_picked_color(t_colors *x, t_symbol *color){
    if(color != &s_){
        strncpy(x->x_color, color->s_name, MAXPDSTRING);
        colors_bang(x);
    }
}

static void colors_free(t_colors *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_id);
}

static void *colors_new(t_symbol *s){
    t_colors *x = (t_colors *)pd_new(colors_class);
    x->x_hex = x->x_gui = x->x_ds = x->x_rgb = 0;
    char buf[MAXPDSTRING];
    sprintf(buf, "#%lx", (t_int)x);
    x->x_id = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_id);
    outlet_new(&x->x_obj, &s_list);
    strcpy(x->x_color,"#ffffff"); // initial color    
    if(s == gensym("-hex"))
        x->x_hex = 1;
    else if(s == gensym("-gui"))
        x->x_gui = 1;
    else if(s == gensym("-ds"))
        x->x_ds = 1;
    else
        x->x_rgb = 1;
    return(x);
}

void colors_setup(void){
    colors_class = class_new(gensym("colors"), (t_newmethod)colors_new,
                (t_method)colors_free, sizeof(t_colors), 0, A_DEFSYMBOL, 0);
    class_addbang(colors_class, (t_method)colors_bang);
    class_addmethod(colors_class, (t_method)colors_rgb, gensym("rgb"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(colors_class, (t_method)colors_pick, gensym("pick"), 0);
    class_addmethod(colors_class, (t_method)colors_convert_to, gensym("to"), A_DEFSYMBOL, 0);
    class_addmethod(colors_class, (t_method)colors_hex, gensym("hex"), A_DEFSYMBOL, 0);
    class_addmethod(colors_class, (t_method)colors_gui, gensym("gui"), A_DEFFLOAT, 0);
    class_addmethod(colors_class, (t_method)colors_ds, gensym("ds"), A_DEFFLOAT, 0);
    class_addmethod(colors_class, (t_method)colors_picked_color, gensym("picked_color"), A_DEFSYMBOL, 0);
    class_addmethod(colors_class, (t_method)colors_pick, gensym("click"), 0);
    sys_vgui("proc else_colors_pick {obj_id color} {\n");
    sys_vgui("set color [tk_chooseColor -initialcolor $color]\n");
    sys_vgui("  pdsend [concat $obj_id picked_color $color]\n");
    sys_vgui("}\n");
}
