#include "m_pd.h"

static t_class *keycode_class;

typedef struct _keycode
{
    t_object x_obj;
    t_outlet *x_outlet1;
    t_outlet *x_outlet2;
} t_keycode;

static void *keycode_new(void)
{
    t_keycode *x = (t_keycode *)pd_new(keycode_class);
    x->x_outlet1 = outlet_new(&x->x_obj, &s_float);
    x->x_outlet2 = outlet_new(&x->x_obj, &s_symbol);
    pd_bind(&x->x_obj.ob_pd, gensym("#keycode"));
    return (x);
}

static void keycode_list(t_keycode *x, t_symbol *s, int ac, t_atom *av)
{
    outlet_float(x->x_outlet2, atom_getfloatarg(1, ac, av));
    outlet_float(x->x_outlet1, atom_getfloatarg(0, ac, av));
}

static void keycode_free(t_keycode *x)
{
    pd_unbind(&x->x_obj.ob_pd, gensym("#keycode"));
}

void keycode_setup(void)
{
    keycode_class = class_new(gensym("keycode"),
        (t_newmethod)keycode_new, (t_method)keycode_free,
        sizeof(t_keycode), CLASS_NOINLET, 0);
    class_addfloat(keycode_class, keycode_list);
    sys_vgui("bind all <KeyPress> {+ pdsend \"#keycode 1 %k\"}\n");
    sys_vgui("bind all <KeyRelease> {+ pdsend \"#keycode 0 %k\"}\n");
}
