// based on cyclone/grab

#include "m_pd.h"
#include "m_imp.h"

struct _outlet{
    t_object        *o_owner;
    struct _outlet  *o_next;
    t_outconnect    *o_connections;
    t_symbol        *o_sym;
};

t_outconnect *outlet_connections(t_outlet *o){ // obj_starttraverseoutlet() replacement
    return(o ? o->o_connections : 0); // magic
}

t_outconnect *outlet_nextconnection(t_outconnect *last, t_object **destp, int *innop){
    t_inlet *dummy;
    return(obj_nexttraverseoutlet(last, destp, &dummy, innop));
}

static t_class *bindlist_class = 0;

typedef struct _bindelem{
    t_pd                *e_who;
    struct _bindelem    *e_next;
}t_bindelem;

typedef struct _bindlist{
    t_pd b_pd;
    t_bindelem *b_list;
}t_bindlist;

typedef struct _take{
    t_object        x_obj;
    t_symbol       *x_target;           // bound symbol
    t_outconnect  **x_grabcons;         // taken connections
// traversal helpers:
    t_object       *x_taken;            // currently taken object
    t_outconnect   *x_taken_connection; // a connection to taken object
    t_bindelem     *x_bindelem;
}t_take;

static t_class *take_class;

static void take_start(t_take *x){
    x->x_taken_connection = 0;
    x->x_bindelem = 0;
    if(x->x_target){
        t_pd *proxy = x->x_target->s_thing;
        t_object *obj;
        if(proxy && bindlist_class){
            if(*proxy == bindlist_class){
                x->x_bindelem = ((t_bindlist *)proxy)->b_list;
                while(x->x_bindelem){
                    obj = pd_checkobject(x->x_bindelem->e_who);
                    if(obj){
                        x->x_taken_connection = outlet_connections(obj->ob_outlet);
                        return;
                    }
                    x->x_bindelem = x->x_bindelem->e_next;
                }
            }
            else if((obj = pd_checkobject(proxy)))
                x->x_taken_connection = outlet_connections(obj->ob_outlet);
        }
    }
}

static t_pd *take_next(t_take *x){
nextremote:
    if(x->x_taken_connection){
        int inno;
        x->x_taken_connection = outlet_nextconnection(x->x_taken_connection, &x->x_taken, &inno);
        if(x->x_taken){
            if(inno){
                if(x->x_target)
                    pd_error(x, "[take]: right outlet must feed leftmost inlet");
                else
                    pd_error(x, "[take]: remote proxy must feed leftmost inlet");
            }
            else{
                t_outlet *op;
                t_outlet *goutp;
                int goutno = 1;
                while(goutno--){
                    x->x_grabcons[goutno] = obj_starttraverseoutlet(x->x_taken, &goutp, goutno);
                    goutp->o_connections = obj_starttraverseoutlet((t_object *)x, &op, goutno);
                }
                return((t_pd *)x->x_taken);
            }
        }
    }
    if(x->x_bindelem)
        while((x->x_bindelem = x->x_bindelem->e_next)){
            t_object *obj = pd_checkobject(x->x_bindelem->e_who);
            if(obj){
                x->x_taken_connection = outlet_connections(obj->ob_outlet);
                goto
                    nextremote;
            }
        }
    return(0);
}

static void take_restore(t_take *x){
    t_outlet *goutp;
    int goutno = 1;
    while(goutno--){
        obj_starttraverseoutlet(x->x_taken, &goutp, goutno);
        goutp->o_connections = x->x_grabcons[goutno];
    }
}

static void take_bang(t_take *x){
    t_pd *taken;
    take_start(x);
    while((taken = take_next(x))){
        pd_bang(taken);
        take_restore(x);
    }
}

static void take_set(t_take *x, t_symbol *s){
    if(s && s != &s_)
        x->x_target = s;
}

static void *take_new(t_symbol *s){
    t_take *x = (t_take *)pd_new(take_class);
    t_outconnect **grabcons = getbytes(sizeof(*grabcons));
    if(!grabcons)
        return(0);
    x->x_grabcons = grabcons;
    outlet_new((t_object *)x, &s_anything);
    x->x_target = (s && s != &s_) ? s : 0;
    return(x);
}

static void take_free(t_take *x){
    if(x->x_grabcons)
        freebytes(x->x_grabcons, sizeof(*x->x_grabcons));
}

void take_setup(void){
    t_symbol *s = gensym("take");
    take_class = class_new(s, (t_newmethod)take_new,
        (t_method)take_free, sizeof(t_take), 0, A_DEFSYMBOL, 0);
    class_addbang(take_class, take_bang);
    class_addmethod(take_class, (t_method)take_set, gensym("set"), A_SYMBOL, 0);
    if(!bindlist_class){
        t_class *c = take_class;
        pd_bind(&take_class, s);
        pd_bind(&c, s);
        bindlist_class = *s->s_thing;
        if(!s->s_thing || !bindlist_class || bindlist_class->c_name != gensym("bindlist"))
            error("[take]: failure to initialize remote taking feature");
        pd_unbind(&c, s);
        pd_unbind(&take_class, s);
    }
}
