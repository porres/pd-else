
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <m_pd.h>
#include <m_imp.h>

static t_class *colors_class;

typedef struct _colors{
    t_object    x_obj;
    t_int       x_hex;
    t_symbol   *x_s;
    char        x_color[MAXPDSTRING];
}t_colors;

static void colors_bang(t_colors *x){
    sys_vgui("after idle [list after 10 else_colors_open %s %s]\n", x->x_s->s_name, x->x_color);
}

/*static void colors_symbol(t_colors *x, t_symbol *s){
    strncpy(x->x_color, s->s_name, 7);
    colors_bang(x);
}*/

static void colors_set(t_colors *x, t_floatarg r, t_floatarg g, t_floatarg b){
    unsigned int red = (unsigned int)(r > 255 ? 255 : r < 0 ? 0 : r);
    unsigned int green = (unsigned int)(g > 255 ? 255 : g < 0 ? 0 : g);
    unsigned int blue = (unsigned int)(b > 255 ? 255 : b < 0 ? 0 : b);
    char hex[MAXPDSTRING];
    sprintf(hex, "#%02x%02x%02x", red, green, blue);
    strncpy(x->x_color, hex, 7);
}

static void colors_callback(t_colors *x, t_symbol *colors){
    if(colors != &s_){
        if(x->x_hex)
            outlet_symbol(x->x_obj.ob_outlet, colors);
        else{ // RGB
            unsigned int red, green, blue;
            sscanf(colors->s_name, "#%02x%02x%02x", &red, &green, &blue);
            t_atom at[3];
            SETFLOAT(at, red);
            SETFLOAT(at+1, green);
            SETFLOAT(at+2, blue);
            outlet_list(x->x_obj.ob_outlet, &s_list, 3, at);
        }
        strncpy(x->x_color, colors->s_name, MAXPDSTRING);
    }
}

static void colors_free(t_colors *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_s);
}

static void *colors_new(t_floatarg f){
    char buf[MAXPDSTRING];
    t_colors *x = (t_colors *)pd_new(colors_class);
    sprintf(buf, "#%lx", (t_int)x);
    x->x_s = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_s);
    outlet_new(&x->x_obj, &s_list);
    strcpy(x->x_color,"#ffffff");
//    x->x_hex = f != 0;
    x->x_hex = 0;
    return(x);
}

void colors_setup(void){
    colors_class = class_new(gensym("colors"), (t_newmethod)colors_new,
                (t_method)colors_free, sizeof(t_colors), 0, A_DEFFLOAT, 0);
    class_addbang(colors_class, (t_method)colors_bang);
//    class_addsymbol(colors_class, (t_method)colors_symbol);
    class_addmethod(colors_class, (t_method)colors_set, gensym("set"), A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(colors_class, (t_method)colors_callback, gensym("callback"), A_DEFSYMBOL, 0);
    class_addmethod(colors_class, (t_method)colors_bang, gensym("click"), 0);
    sys_vgui("proc else_colors_open {obj_id color} {\n");
    sys_vgui("set color [tk_chooseColor -initialcolor $color]\n");
    sys_vgui("  pdsend [concat $obj_id callback $color]\n");
    sys_vgui("}\n");
}
