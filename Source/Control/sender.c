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
    t_glist    *x_init_cv;
    t_int       x_depth_arg;
    t_int       x_local;
}t_sender;

/*static void sender_get_snd(t_sender *x, t_floatarg f){
    int idx = (int)f;
    if(idx == 1 && x->x_sym1 != &s_)
        x->x_raw = x->x_sym1;
    else if(idx == 2 && x->x_sym2 != &s_)
        x->x_raw = x->x_sym2;
    else{
        idx += (x->x_depth_arg + x->x_local);
        t_binbuf *bb = x->x_obj.te_binbuf;
        if(binbuf_getnatom(bb) > idx){
            char buf[128];
            atom_string(binbuf_getvec(bb) + idx, buf, 128);
            x->x_raw = gensym(buf);
        }
    }
}*/

/*static t_symbol *get_send_name(t_sender *x){
    t_symbol *send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(x->x_raw == &s_)
        return(send_name = &s_);
    send_name = canvas_realizedollar(x->x_cv, x->x_raw);
    if(x->x_local){
        char buf[MAXPDSTRING];
        snprintf(buf, MAXPDSTRING, ".x%lx-%s",
                 (long unsigned int)x->x_init_cv,
                 send_name->s_name);
        send_name = gensym(buf);
    }
    return(send_name);
}*/

static t_symbol *get_send_name(t_sender *x, t_symbol *sym){
    t_symbol *send_name = canvas_realizedollar(x->x_cv, sym);
    if(sym == &s_)
        return(send_name = &s_);
    send_name = canvas_realizedollar(x->x_cv, sym);
    if(x->x_local){
        char buf[MAXPDSTRING];
        snprintf(buf, MAXPDSTRING, ".x%lx-%s",
            (long unsigned int)x->x_init_cv,
            send_name->s_name);
        send_name = gensym(buf);
    }
    return(send_name);
}

static void sender_ping(t_sender *x){
    if(x->x_cname->s_thing && x->x_sym1 != gensym("empty")){
        t_atom at[1];
        SETSYMBOL(at, x->x_sym1);
        typedmess(x->x_cname->s_thing, x->x_cname, 1, at);
    }
}

static void sender_bang(t_sender *x){
    sender_ping(x);
    t_symbol *send_name = get_send_name(x, x->x_sym1);
    if(send_name != &s_ && send_name->s_thing)
        pd_bang(send_name->s_thing);
    send_name = get_send_name(x, x->x_sym2);
    if(send_name != &s_ && send_name->s_thing)
        pd_bang(send_name->s_thing);
}

static void sender_float(t_sender *x, t_float f){
    sender_ping(x);
    t_symbol *send_name = get_send_name(x, x->x_sym1);
    if(send_name != &s_ && send_name->s_thing)
        pd_float(send_name->s_thing, f);
    send_name = get_send_name(x, x->x_sym2);
    if(send_name != &s_ && send_name->s_thing)
        pd_float(send_name->s_thing, f);
}

static void sender_symbol(t_sender *x, t_symbol *s){
    sender_ping(x);
    t_symbol *send_name = get_send_name(x, x->x_sym1);
    if(send_name != &s_ && send_name->s_thing)
        pd_symbol(send_name->s_thing, s);
    send_name = get_send_name(x, x->x_sym2);
    if(send_name != &s_ && send_name->s_thing)
        pd_symbol(send_name->s_thing, s);
}

static void sender_pointer(t_sender *x, t_gpointer *gp){
    sender_ping(x);
    t_symbol *send_name = get_send_name(x, x->x_sym1);
    if(send_name != &s_ && send_name->s_thing)
        pd_pointer(send_name->s_thing, gp);
    send_name = get_send_name(x, x->x_sym2);
    if(send_name != &s_ && send_name->s_thing)
        pd_pointer(send_name->s_thing, gp);
}

static void sender_list(t_sender *x, t_symbol *s, int ac, t_atom *av){
    sender_ping(x);
    t_symbol *send_name = get_send_name(x, x->x_sym1);
    if(send_name != &s_ && send_name->s_thing)
        pd_list(send_name->s_thing, s, ac, av);
    send_name = get_send_name(x, x->x_sym2);
    if(send_name != &s_ && send_name->s_thing)
        pd_list(send_name->s_thing, s, ac, av);
}

static void sender_anything(t_sender *x, t_symbol *s, int ac, t_atom *av){
    sender_ping(x);
    t_symbol *send_name = get_send_name(x, x->x_sym1);
    if(send_name != &s_ && send_name->s_thing)
        typedmess(send_name->s_thing, s, ac, av);
    send_name = get_send_name(x, x->x_sym2);
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
    x->x_init_cv = canvas_getcurrent();
    x->x_cv = canvas_getrootfor(x->x_init_cv);
    if(ac && atom_getsymbol(av) == gensym("-local")){
        x->x_local = 1;
        av++, ac--;
    }
    if(ac && (av)->a_type == A_FLOAT){
        x->x_depth_arg = 1;
        depth = atom_getint(av) < 0 ? 0 : atom_getint(av);
        av++, ac--;
        while(depth-- && x->x_cv->gl_owner)
            x->x_cv = canvas_getrootfor(x->x_cv->gl_owner);
    }
    if(ac && (av)->a_type == A_SYMBOL){
        s = atom_getsymbol(av);
        if(s != &s_)
            x->x_sym1 = canvas_realizedollar(x->x_cv, s);
        av++, ac--;
    }
    if(ac && (av)->a_type == A_SYMBOL){
        s = atom_getsymbol(av);
        if(s != &s_)
            x->x_sym2 = canvas_realizedollar(x->x_cv, s);
    }
    symbolinlet_new(&x->x_obj, &x->x_sym1);
    symbolinlet_new(&x->x_obj, &x->x_sym2);
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
