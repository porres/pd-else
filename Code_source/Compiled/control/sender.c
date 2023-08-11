// porres

#include "m_pd.h"
#include "g_canvas.h"

static t_class *sender_class;

typedef struct _sender{
    t_object  x_obj;
    t_symbol *x_sym1;
    t_symbol *x_sym2;
    t_glist  *x_cv;
}t_sender;

static void sender_bang(t_sender *x){
    t_symbol *sym1 = canvas_realizedollar(x->x_cv, x->x_sym1);
    t_symbol *sym2 = canvas_realizedollar(x->x_cv, x->x_sym2);
    if(sym1 != &s_ && x->x_sym1->s_thing) pd_bang(sym1->s_thing);
    if(sym2 != &s_ && x->x_sym2->s_thing) pd_bang(sym2->s_thing);
}

static void sender_float(t_sender *x, t_float f){
    t_symbol *sym1 = canvas_realizedollar(x->x_cv, x->x_sym1);
    t_symbol *sym2 = canvas_realizedollar(x->x_cv, x->x_sym2);
    if(sym1 != &s_ && x->x_sym1->s_thing) pd_float(sym1->s_thing, f);
    if(sym2 != &s_ && x->x_sym2->s_thing) pd_float(sym2->s_thing, f);
}

static void sender_symbol(t_sender *x, t_symbol *s){
    t_symbol *sym1 = canvas_realizedollar(x->x_cv, x->x_sym1);
    t_symbol *sym2 = canvas_realizedollar(x->x_cv, x->x_sym2);
    if(sym1 != &s_ && x->x_sym1->s_thing) pd_symbol(sym1->s_thing, s);
    if(sym2 != &s_ && x->x_sym2->s_thing) pd_symbol(sym2->s_thing, s);
}

static void sender_pointer(t_sender *x, t_gpointer *gp){
    t_symbol *sym1 = canvas_realizedollar(x->x_cv, x->x_sym1);
    t_symbol *sym2 = canvas_realizedollar(x->x_cv, x->x_sym2);
    if(sym1 != &s_ && x->x_sym1->s_thing) pd_pointer(sym1->s_thing, gp);
    if(sym2 != &s_ && x->x_sym2->s_thing) pd_pointer(sym2->s_thing, gp);
}

static void sender_list(t_sender *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *sym1 = canvas_realizedollar(x->x_cv, x->x_sym1);
    t_symbol *sym2 = canvas_realizedollar(x->x_cv, x->x_sym2);
    if(sym1 != &s_ && x->x_sym1->s_thing) pd_list(sym1->s_thing, s, ac, av);
    if(sym2 != &s_ && x->x_sym2->s_thing) pd_list(sym2->s_thing, s, ac, av);
}

static void sender_anything(t_sender *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *sym1 = canvas_realizedollar(x->x_cv, x->x_sym1);
    t_symbol *sym2 = canvas_realizedollar(x->x_cv, x->x_sym2);
    if(sym1 != &s_ && x->x_sym1->s_thing) typedmess(sym1->s_thing, s, ac, av);
    if(sym2 != &s_ && x->x_sym2->s_thing) typedmess(sym2->s_thing, s, ac, av);
}

static void *sender_new(t_symbol *s, int ac, t_atom *av){
    t_sender *x = (t_sender *)pd_new(sender_class);
    int depth = 0;
    x->x_sym1 = x->x_sym2 = &s_;
    x->x_cv = canvas_getrootfor(canvas_getcurrent());
    symbolinlet_new(&x->x_obj, &x->x_sym1);
    if(ac && (av)->a_type == A_FLOAT){
        depth = atom_getint(av) < 0 ? 0 : atom_getint(av);
        av++, ac--;
        while(depth-- && x->x_cv->gl_owner)
            x->x_cv = canvas_getrootfor(x->x_cv->gl_owner);
    }
    if(ac && (av)->a_type == A_SYMBOL){
        s = atom_getsymbol(av);
        if(s != &s_)
            x->x_sym1 = s;
        av++, ac--;
    }
    if(ac && (av)->a_type == A_SYMBOL){
        s = atom_getsymbol(av);
        if(s != &s_){
            x->x_sym2 = s;
            symbolinlet_new(&x->x_obj, &x->x_sym2);
        }
    }
    return(x);
}

void sender_setup(void){
    sender_class = class_new(gensym("sender"), (t_newmethod)sender_new,
        0, sizeof(t_sender), 0, A_GIMME, 0);
    class_addbang(sender_class, sender_bang);
    class_addfloat(sender_class, sender_float);
    class_addsymbol(sender_class, sender_symbol);
    class_addpointer(sender_class, sender_pointer);
    class_addlist(sender_class, sender_list);
    class_addanything(sender_class, sender_anything);
}
