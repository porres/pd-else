// porres

#include <m_pd.h>
#include <g_canvas.h>

static t_class *sender_class;

typedef struct _sender{
    t_object    x_obj;
    t_symbol   *x_cname;
    t_atom      x_at[1];
    t_symbol   *x_sym1;
    t_symbol   *x_sym2;
    t_symbol   *x_raw;
    t_glist    *x_cv;
    t_int       x_depth_arg;
}t_sender;

static void sender_get_snd(t_sender *x, t_floatarg f){
    int idx = (int)f;
    if(idx == 1 && x->x_sym1 != &s_)
        x->x_raw = x->x_sym1;
    else if(idx == 2 && x->x_sym2 != &s_)
        x->x_raw = x->x_sym2;
    else{
        idx += x->x_depth_arg;
        t_binbuf *bb = x->x_obj.te_binbuf;
        if(binbuf_getnatom(bb) > idx){
            char buf[128];
            atom_string(binbuf_getvec(bb) + idx, buf, 128);
            x->x_raw = gensym(buf);
        }
    }
}

static void sender_ping(t_sender *x){
    if(x->x_cname->s_thing && x->x_raw != gensym("empty")){
        t_atom at[1];
        SETSYMBOL(at, x->x_raw);
        typedmess(x->x_cname->s_thing, x->x_cname, 1, at);
    }
}

static void sender_bang(t_sender *x){
    sender_get_snd(x, 1);
    sender_ping(x);
    t_symbol *send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(x->x_cname->s_thing)
        pd_symbol(x->x_cname->s_thing, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        pd_bang(send_name->s_thing);
    sender_get_snd(x, 2); // 2nd name
    send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        pd_bang(send_name->s_thing);
}

static void sender_float(t_sender *x, t_float f){
    sender_get_snd(x, 1);
    sender_ping(x);
    t_symbol *send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        pd_float(send_name->s_thing, f);
    sender_get_snd(x, 2); // 2nd name
    send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        pd_float(send_name->s_thing, f);
}

static void sender_symbol(t_sender *x, t_symbol *s){
    sender_get_snd(x, 1);
    sender_ping(x);
    t_symbol *send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        pd_symbol(send_name->s_thing, s);
    sender_get_snd(x, 2); // 2nd name
    send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        pd_symbol(send_name->s_thing, s);
}

static void sender_pointer(t_sender *x, t_gpointer *gp){
    sender_get_snd(x, 1);
    sender_ping(x);
    t_symbol *send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        pd_pointer(send_name->s_thing, gp);
    sender_get_snd(x, 2); // 2nd name
    send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        pd_pointer(send_name->s_thing, gp);
}

static void sender_list(t_sender *x, t_symbol *s, int ac, t_atom *av){
    sender_get_snd(x, 1);
    sender_ping(x);
    t_symbol *send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        pd_list(send_name->s_thing, s, ac, av);
    sender_get_snd(x, 2); // 2nd name
    send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        pd_list(send_name->s_thing, s, ac, av);
}

static void sender_anything(t_sender *x, t_symbol *s, int ac, t_atom *av){
    sender_get_snd(x, 1);
    sender_ping(x);
    t_symbol *send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        typedmess(send_name->s_thing, s, ac, av);
    sender_get_snd(x, 2); // 2nd name
    send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(send_name != &s_ && send_name->s_thing)
        typedmess(send_name->s_thing, s, ac, av);
}

static void get_cname(t_sender *x, t_floatarg depth){
    t_canvas *canvas = canvas_getrootfor(canvas_getcurrent());
    while(depth-- && canvas->gl_owner)
        canvas = canvas_getrootfor(canvas->gl_owner);
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING, ".x%lx-link", (long unsigned int)canvas);
    x->x_cname = gensym(buf);
}

static void *sender_new(t_symbol *s, int ac, t_atom *av){
    t_sender *x = (t_sender *)pd_new(sender_class);
    get_cname(x, 100);
    x->x_depth_arg = 0;
    int depth = 0;
    x->x_raw = s; // get rid of warning
    x->x_sym1 = x->x_sym2 = &s_;
    x->x_cv = canvas_getrootfor(canvas_getcurrent());
    symbolinlet_new(&x->x_obj, &x->x_sym1);
    symbolinlet_new(&x->x_obj, &x->x_sym2);
    if(ac && (av)->a_type == A_FLOAT){
        x->x_depth_arg = 1;
        depth = atom_getint(av) < 0 ? 0 : atom_getint(av);
        av++, ac--;
        while(depth-- && x->x_cv->gl_owner)
            x->x_cv = canvas_getrootfor(x->x_cv->gl_owner);
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
