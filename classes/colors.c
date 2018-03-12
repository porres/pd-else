// variant of hcs/colorspanel

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
    char        x_current_color[MAXPDSTRING];
}t_colors;

static void colors_bang(t_colors *x){
    sys_vgui("after idle [list after 10 else_colors_open %s %s]\n",
             x->x_s->s_name, x->x_current_color);
}

static void colors_symbol(t_colors *x, t_symbol *s){
    strncpy(x->x_current_color, s->s_name, MAXPDSTRING);
    colors_bang(x);
}

 static void colors_list(t_colors *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *tmp_symbol = s;
    int i;
    unsigned int tmp_int;
    char colors_buffer[3];
    char colors_string[MAXPDSTRING];
    strncpy(colors_string, "#", MAXPDSTRING);
    if(ac > 3) 
        logpost(x, 2, "[colors] warning more than three elements in list");
    for(i = 0; i < 3; i++){
        tmp_symbol = atom_getsymbolarg(i, ac, av);
        if(tmp_symbol == &s_){
            tmp_int = (unsigned int)(atom_getfloatarg(i, ac , av));
            snprintf(colors_buffer, 3, "%02x", (tmp_int > 255 ? 255 : tmp_int));
            strncat(colors_string, colors_buffer, 3);
        }
        else 
        {
            pd_error(x,"[colors] symbols are not allowed in the colors list");
            return;
        }
    }
    memcpy(x->x_current_color, colors_string, 7);
    colors_bang(x);
}

static void colors_callback(t_colors *x, t_symbol *colors){
    t_atom at[3];
    unsigned int red, green, blue;
    if(colors != &s_){
        strncpy(x->x_current_color, colors->s_name, MAXPDSTRING);
        sscanf(x->x_current_color, "#%02x%02x%02x", &red, &green, &blue);
        if(x->x_hex){
            outlet_symbol(x->x_obj.ob_outlet, gensym(x->x_current_color));
        }
        else{
            SETFLOAT(at, red);
            SETFLOAT(at+1, green);
            SETFLOAT(at+2, blue);
            outlet_list(x->x_obj.ob_outlet, &s_list, 3, at);
        }
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
    strcpy(x->x_current_color,"#ffffff");
    x->x_hex = f != 0;
    return(x);
}

void colors_setup(void){
    colors_class = class_new(gensym("colors"), (t_newmethod)colors_new,
                (t_method)colors_free, sizeof(t_colors), 0, A_DEFFLOAT, 0);
    class_addbang(colors_class, (t_method)colors_bang);
    class_addsymbol(colors_class, (t_method)colors_symbol);
    class_addlist(colors_class, (t_method)colors_list);
    class_addmethod(colors_class, (t_method)colors_callback, gensym("callback"), A_DEFSYMBOL, 0);
    class_addmethod(colors_class, (t_method)colors_bang, gensym("click"), 0);

    sys_vgui("eval [read [open {%s/%s.tcl}]]\n", colors_class->c_externdir->s_name,
             colors_class->c_name->s_name);
}
