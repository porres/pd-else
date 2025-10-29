// porres

#include <m_pd.h>
#include <g_canvas.h>

static t_class *receiver_class, *receiver_proxy_class;

typedef struct _receiver_proxy{
    t_pd              p_pd;
    struct _receiver  *p_owner;
}t_receiver_proxy;

typedef struct _receiver{
    t_object          x_obj;
    t_receiver_proxy  x_proxy;
    t_symbol         *x_sym_1;
    t_symbol         *x_sym_2;
    t_symbol         *x_raw;
    t_glist          *x_cv;
    t_glist          *x_init_cv;
    int               x_bound;
    t_int             x_depth_arg;
    t_outlet         *x_learnout;
    t_symbol         *x_cname;
    int               x_learn;
    int               x_local;
}t_receiver;

static void receiver_proxy_init(t_receiver_proxy * p, t_receiver *x){
    p->p_pd = receiver_proxy_class;
    p->p_owner = x;
}

static void receiver_proxy_clear(t_receiver_proxy *p){
    t_receiver *x = p->p_owner;
    if(x->x_bound){
        if(x->x_sym_1 != &s_)
            pd_unbind(&x->x_obj.ob_pd, x->x_sym_1);
        if(x->x_sym_2 != &s_)
            pd_unbind(&x->x_obj.ob_pd, x->x_sym_2);
        x->x_sym_1 = x->x_sym_2 = &s_;
    }
    x->x_bound = 0;
}

static void receiver_proxy_learn(t_receiver_proxy *p){
    p->p_owner->x_learn = 1;
    pd_bind(&p->p_owner->x_obj.ob_pd, p->p_owner->x_cname);
}

static void receiver_proxy_deactivate(t_receiver_proxy *p){
    if(p->p_owner->x_learn){
        p->p_owner->x_learn = 0;
        pd_unbind(&p->p_owner->x_obj.ob_pd, p->p_owner->x_cname);
    }
}

static void receiver_proxy_forget(t_receiver_proxy *p){
    if(p->p_owner->x_learn){
        p->p_owner->x_learn = 0;
        pd_unbind(&p->p_owner->x_obj.ob_pd, p->p_owner->x_cname);
    }
    receiver_proxy_clear(p);
    outlet_symbol(p->p_owner->x_learnout, gensym("none"));
}

static void receiver_proxy_bang(t_receiver_proxy *p){
    t_receiver *x = p->p_owner;
    if(x->x_sym_1 != &s_ || x->x_sym_2 != &s_){
        if(x->x_sym_1 != &s_ && x->x_sym_2 == &s_)
            outlet_symbol(x->x_obj.ob_outlet, x->x_sym_1);
        else{
            t_atom at[2];
            SETSYMBOL(at, x->x_sym_1);
            SETSYMBOL(at+1, x->x_sym_2);
            outlet_list(x->x_obj.ob_outlet, &s_list, 2, at);
        }
    }
}

static void receiver_proxy_symbol(t_receiver_proxy *p, t_symbol* s){
    if(s != &s_){
        t_receiver *x = p->p_owner;
        if(x->x_bound){
            if(x->x_sym_1 != &s_)
                pd_unbind(&x->x_obj.ob_pd, x->x_sym_1);
            if(x->x_sym_2 != &s_){
                pd_unbind(&x->x_obj.ob_pd, x->x_sym_2);
                x->x_sym_2 = &s_;
            }
        }
        x->x_sym_1 = canvas_realizedollar(x->x_cv, s);
        if(x->x_local){
            char buf[MAXPDSTRING];
            snprintf(buf, MAXPDSTRING, ".x%lx-%s",
                (long unsigned int)x->x_init_cv, x->x_sym_1->s_name);
            x->x_sym_1 = gensym(buf);
        }
        pd_bind(&x->x_obj.ob_pd, x->x_sym_1);
        x->x_bound = 1;
    }
}

static void receiver_proxy_list(t_receiver_proxy *p, t_symbol* s, int ac, t_atom *av){
    t_receiver *x = p->p_owner;
    if(ac > 0){
        char buf[MAXPDSTRING];
        if(ac > 2){
            pd_error(x, "[receiver]: too many name arguments");
            return;
        }
        if((av)->a_type == A_FLOAT){
            pd_error(x, "[receiver]: can't take float as a name argument");
            return;
        }
        else{
            s = atom_getsymbol(av);
            if(s != &s_){
                if(x->x_bound){
                    if(x->x_sym_1 !=  &s_)
                        pd_unbind(&x->x_obj.ob_pd, x->x_sym_1);
                    if(x->x_sym_2 !=  &s_){
                        pd_unbind(&x->x_obj.ob_pd, x->x_sym_2);
                        x->x_sym_2 = &s_;
                    }
                }
                x->x_sym_1 = canvas_realizedollar(x->x_cv, s);
                if(x->x_local){
                    snprintf(buf, MAXPDSTRING, ".x%lx-%s",
                        (long unsigned int)x->x_init_cv, s->s_name);
                    x->x_sym_1 = gensym(buf);
                }
                pd_bind(&x->x_obj.ob_pd, x->x_sym_1);
                x->x_bound = 1;
            }
            else{
                pd_error(x, "[receiver]: invalid symbol name");
                return;
            }
        }
        if(ac == 2){
            if((av+1)->a_type == A_FLOAT){
                pd_error(x, "[receiver]: can't take float as a name argument");
                return;
            }
            else{
                s = atom_getsymbol(av+1);
                if(s != &s_){
                    x->x_sym_2 = canvas_realizedollar(x->x_cv, s);
                    if(x->x_local){
                        snprintf(buf, MAXPDSTRING, ".x%lx-%s",
                                 (long unsigned int)x->x_init_cv, s->s_name);
                        x->x_sym_2 = gensym(buf);
                    }
                    pd_bind(&x->x_obj.ob_pd, x->x_sym_2);
                }
                else{
                    pd_error(x, "[receiver]: invalid name symbol");
                    return;
                }
            }
        }
    }
}

static void receiver_bang(t_receiver *x){
    outlet_bang(x->x_obj.ob_outlet);
}

static void receiver_float(t_receiver *x, t_float f){
    outlet_float(x->x_obj.ob_outlet, f);
}

static void receiver_symbol(t_receiver *x, t_symbol *s){
    outlet_symbol(x->x_obj.ob_outlet, s);
}

static void receiver_pointer(t_receiver *x, t_gpointer *gp){
    outlet_pointer(x->x_obj.ob_outlet, gp);
}

static void receiver_list(t_receiver *x, t_symbol *s, int ac, t_atom *av){
    if(!ac)
        receiver_bang(x);
    else if(ac == 1){
        if((av)->a_type == A_SYMBOL)
            receiver_symbol(x, atom_getsymbol(av));
        else if((av)->a_type == A_FLOAT)
            receiver_float(x, atom_getfloat(av));
        else if((av)->a_type == A_POINTER)
            receiver_pointer(x, av->a_w.w_gpointer);
    }
    else
        outlet_list(x->x_obj.ob_outlet, s, ac, av);
}

static void receiver_anything(t_receiver *x, t_symbol *s, int ac, t_atom *av){
    if(x->x_learn && s == x->x_cname && ac == 1){
        t_symbol *raw = atom_getsymbol(av);
        receiver_proxy_symbol(&x->x_proxy, raw);
        outlet_symbol(x->x_learnout, raw);
        x->x_learn = 0;
        pd_unbind(&x->x_obj.ob_pd, x->x_cname);
    }
    else
        outlet_anything(x->x_obj.ob_outlet, s, ac, av);
}

static void get_cname(t_receiver *x, t_floatarg depth){
    t_canvas *canvas = canvas_getrootfor(canvas_getcurrent());
    while(depth-- && canvas->gl_owner)
        canvas = canvas_getrootfor(canvas->gl_owner);
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING, ".x%lx-link", (long unsigned int)canvas);
    x->x_cname = gensym(buf);
}

static void *receiver_new(t_symbol *s, int ac, t_atom *av){
    t_receiver *x = (t_receiver *)pd_new(receiver_class);
    get_cname(x, 100);
    char buf[MAXPDSTRING];
    x->x_sym_1 = x->x_sym_2 = &s_;
    x->x_bound = x->x_local = 0;
    int depth = 0;
    x->x_init_cv = canvas_getcurrent();
    x->x_cv = canvas_getrootfor(x->x_init_cv);
    if(ac && atom_getsymbol(av) == gensym("-local")){
        x->x_local = 1;
        av++, ac--;
    }
    if(ac && (av)->a_type == A_FLOAT){
        depth = atom_getint(av) < 0 ? 0 : atom_getint(av);
        av++, ac--;
        while(depth-- && x->x_cv->gl_owner)
            x->x_cv = canvas_getrootfor(x->x_cv->gl_owner);
    }
    if(ac && (av)->a_type == A_SYMBOL){
        s = atom_getsymbol(av);
        if(s != &s_){
            if(x->x_local){
                snprintf(buf, MAXPDSTRING, ".x%lx-%s",
                    (long unsigned int)x->x_init_cv, s->s_name);
                s = gensym(buf);
            }
            pd_bind(&x->x_obj.ob_pd, x->x_sym_1 = canvas_realizedollar(x->x_cv, s));
            x->x_bound = 1;
        }
        av++, ac--;
    }
    if(ac && (av)->a_type == A_SYMBOL){
        s = atom_getsymbol(av);
        if(s != &s_){
            if(x->x_local){
                snprintf(buf, MAXPDSTRING, ".x%lx-%s",
                    (long unsigned int)x->x_init_cv, s->s_name);
                s = gensym(buf);
            }
            pd_bind(&x->x_obj.ob_pd, x->x_sym_2 = canvas_realizedollar(x->x_cv, s));
            x->x_bound = 1;
        }
    }
    receiver_proxy_init(&x->x_proxy, x);
    inlet_new(&x->x_obj, &x->x_proxy.p_pd, 0, 0);
    outlet_new(&x->x_obj, 0);
    x->x_learnout = outlet_new((t_object *)x, &s_symbol);
    return(x);
}

static void receiver_free(t_receiver *x){
    if(x->x_learn)
        pd_unbind(&x->x_obj.ob_pd, x->x_cname);
    if(x->x_bound){
        if(x->x_sym_1 !=  &s_)
            pd_unbind(&x->x_obj.ob_pd, x->x_sym_1);
        if(x->x_sym_2 !=  &s_)
            pd_unbind(&x->x_obj.ob_pd, x->x_sym_2);
    }
}

void receiver_setup(void){
    receiver_class = class_new(gensym("receiver"), (t_newmethod)receiver_new,
        (t_method)receiver_free, sizeof(t_receiver), CLASS_NOINLET, A_GIMME, 0);
    class_addbang(receiver_class, receiver_bang);
    class_addfloat(receiver_class, (t_method)receiver_float);
    class_addsymbol(receiver_class, receiver_symbol);
    class_addpointer(receiver_class, receiver_pointer);
    class_addlist(receiver_class, receiver_list);
    class_addanything(receiver_class, receiver_anything);
    receiver_proxy_class = (t_class *)class_new(gensym("receiver proxy"),
        0, 0, sizeof(t_receiver_proxy), 0, 0);
    class_addsymbol(receiver_proxy_class, receiver_proxy_symbol);
    class_addbang(receiver_proxy_class, receiver_proxy_bang);
    class_addlist(receiver_proxy_class, receiver_proxy_list);
    class_addmethod(receiver_proxy_class, (t_method)receiver_proxy_clear, gensym("clear"), 0);
    class_addmethod(receiver_proxy_class, (t_method)receiver_proxy_learn, gensym("learn"), 0);
    class_addmethod(receiver_proxy_class, (t_method)receiver_proxy_forget, gensym("forget"), 0);
    class_addmethod(receiver_proxy_class, (t_method)receiver_proxy_deactivate, gensym("deactivate"), 0);
}
