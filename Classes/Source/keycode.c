#include "m_pd.h"

static t_class *keycode_class;

typedef struct _keycode
{
    t_object x_obj;
    t_outlet *x_outlet1;
    t_outlet *x_outlet2;
} t_keycode;

/* keep our own list of objects */
typedef struct _listelem
{
    t_pd *e_who;
    struct _listelem *e_next;
} t_listelem;

typedef struct _objectlist
{
    t_pd b_pd;
    t_listelem *b_list;
} t_objectlist;

static t_objectlist *object_list;

static void object_list_bind(t_pd *x) {
    t_listelem *e = (t_listelem *)getbytes(sizeof(t_listelem));
    e->e_next = object_list->b_list;
    e->e_who = x;
    object_list->b_list = e;
}

static void object_list_unbind(t_pd *x) {
    t_listelem *e, *e2; 

    if ((e = object_list->b_list)->e_who == x)
    {
        object_list->b_list = e->e_next;
        freebytes(e, sizeof(t_listelem));
        return;
    }
    for (;(e2 = e->e_next); e = e2)
        if (e2->e_who == x)
    {
        e->e_next = e2->e_next;
        freebytes(e2, sizeof(t_listelem));
        return;
    }
}

static void *keycode_new(void)
{
    t_keycode *x = (t_keycode *)pd_new(keycode_class);
    x->x_outlet1 = outlet_new(&x->x_obj, &s_float);
    x->x_outlet2 = outlet_new(&x->x_obj, &s_symbol);
    object_list_bind(&x->x_obj.ob_pd);
    return (x);
}

static void keycode_list(t_keycode *x, t_symbol *s, int ac, t_atom *av)
{
    outlet_float(x->x_outlet2, atom_getfloatarg(1, ac, av));
    outlet_float(x->x_outlet1, atom_getfloatarg(0, ac, av));
}

static void object_list_iterate(t_objectlist *x, t_symbol *s, int argc, t_atom *argv) {
    #ifdef __APPLE__
    #elif defined _WIN32
    #endif
    t_listelem *e;
    for (e = x->b_list; e; e = e->e_next)
        pd_list(e->e_who, s, argc, argv);
}

static void keycode_free(t_keycode *x)
{
    object_list_unbind(&x->x_obj.ob_pd);
}

void keycode_setup(void)
{
    /* since it's a singleton, I won't keep track of the class */
    t_class *objectlist_class;
    keycode_class = class_new(gensym("keycode"),
        (t_newmethod)keycode_new, (t_method)keycode_free,
        sizeof(t_keycode), CLASS_NOINLET, 0);
    class_addlist(keycode_class, keycode_list);
    objectlist_class = class_new(NULL, 0, 0, sizeof(t_objectlist), CLASS_PD, 0);
    class_addlist(objectlist_class, object_list_iterate);
    object_list = (t_objectlist *)pd_new(objectlist_class);
    object_list->b_list = NULL;
    pd_bind(&object_list->b_pd, gensym("#keycode"));
    /* Tk stores the actual code in the high byte on MacOs, hopefully doesn't change
     * based on compiler or something */
    #ifdef __APPLE__
    sys_vgui("bind all <KeyPress> {+ pdsend \"#keycode 1 [expr %k >> 24]\"}\n");
    sys_vgui("bind all <KeyRelease> {+ pdsend \"#keycode 0 [expr %k >> 24]\"}\n");
    #else /* __APPLE__ */
    sys_vgui("bind all <KeyPress> {+ pdsend \"#keycode 1 %k\"}\n");
    sys_vgui("bind all <KeyPress> {+ pdsend \"#keycode 0 %k\"}\n");
    #endif /* NOT __APPLE__ */
}
