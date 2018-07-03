// Porres 2016
 
#include "m_pd.h"
#include <math.h>
#include <string.h>

static t_class *rescale_class;

typedef struct _rescale{
    t_object    x_obj;
    t_outlet   *x_outlet;
    t_atom     *x_at;
    t_int       x_bytes;
    t_float     x_f;
    t_float     x_minin;
    t_float     x_maxin;
    t_float     x_minout;
    t_float     x_maxout;
    t_float     x_exp;
    t_int       x_clip;
}t_rescale;

void rescale_clip(t_rescale *x, t_floatarg f){
    x->x_clip = f != 0;
}

static t_float convert(t_rescale *x, t_float f){
    t_float minin = x->x_minin;
    t_float minout = x->x_minout;
    t_float maxout = x->x_maxout;
    t_float rangein = x->x_maxin - minin;
    t_float rangeout = x->x_maxout - minout;
    t_float exp = x->x_exp;
    t_float r = (f-minin) == 0 ? minout :
        (f-minin)/rangein > 0 ?
        minout + rangeout * pow((f-minin)/rangein, exp) :
        minout + rangeout * -pow((minin-f)/rangein, exp);
    if(x->x_clip){
        if(r < minout)
            r = minout;
        if(r > maxout)
            r = maxout;
    }
    return r;
}

static void rescale_bang(t_rescale *x){
    outlet_float(x->x_outlet, convert(x, x->x_f));
}

static void rescale_float(t_rescale *x, t_floatarg f){
    outlet_float(x->x_outlet, convert(x, x->x_f = f));
}

static void rescale_list(t_rescale *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL; // avoid warning
    int old_bytes = x->x_bytes, i = 0;
    x->x_bytes = ac*sizeof(t_atom);
    x->x_at = (t_atom *)t_resizebytes(x->x_at, old_bytes, x->x_bytes);
    for(i = 0; i < ac; i++)
        SETFLOAT(x->x_at+i, convert(x, atom_getfloatarg(i, ac, av)));
    outlet_list(x->x_outlet, 0, ac, x->x_at);
}

static void rescale_set(t_rescale *x, t_float f){
    x->x_f = f;
}

static void rescale_exp(t_rescale *x, t_floatarg f){
    x->x_exp = f < 0. ? 0. : f;
}

static void rescale_free(t_rescale *x){
    t_freebytes(x->x_at, x->x_bytes);
}

static void *rescale_new(t_symbol *s, int ac, t_atom *av){
    t_rescale *x = (t_rescale *)pd_new(rescale_class);
    t_symbol *sym = s;
    x->x_minin = 0;
    x->x_maxin = 127;
    x->x_minout = 0;
    x->x_maxout = 1;
    x->x_exp = 1.f;
    t_int numargs = 0;
    if(ac > 0){
        for(int i = 0; i < ac; i++){
            if((av+i)->a_type == A_FLOAT)
                numargs++;
        }
        t_int argnum = 0;
        if(numargs <= 3){
            while(ac){
                if(av->a_type == A_FLOAT){
                    t_float argval = atom_getfloatarg(0, ac, av);
                    switch(argnum){
                        case 0:
                            x->x_minout = argval;
                            break;
                        case 1:
                            x->x_maxout = argval;
                            break;
                        case 2:
                            x->x_exp = argval < 0 ? 0 : argval;
                            break;
                        default:
                            break;
                    };
                }
                else if(av->a_type == A_SYMBOL){
                    sym = atom_getsymbolarg(0, ac, av);
                    if(!strcmp(sym->s_name, "-clip"))
                        x->x_clip = 1;
                    else
                        goto errstate;
                }
                else
                    goto errstate;
                argnum++;
                ac--;
                av++;
            }
        }
        else{ // numargs = 4 || 5
            while(ac){
                if(av->a_type == A_FLOAT){
                    t_float argval = atom_getfloatarg(0, ac, av);
                    switch(argnum){
                        case 0:
                            x->x_minin = argval;
                            break;
                        case 1:
                            x->x_maxin = argval;
                            break;
                        case 2:
                            x->x_minout = argval;
                            break;
                        case 3:
                            x->x_maxout = argval;
                            break;
                        case 4:
                            x->x_exp = argval < 0 ? 0 : argval;
                            break;
                        default:
                            break;
                    };
                }
                else if(av->a_type == A_SYMBOL){
                    sym = atom_getsymbolarg(0, ac, av);
                    if(!strcmp(sym->s_name, "-clip"))
                        x->x_clip = 1;
                    else
                        goto errstate;
                }
                else
                    goto errstate;
                argnum++;
                ac--;
                av++;
            }
        }
    }
    x->x_bytes = sizeof(t_atom);
    x->x_at = (t_atom *)getbytes(x->x_bytes);
    x->x_outlet = outlet_new(&x->x_obj, 0);
    if(numargs <= 3){
        floatinlet_new(&x->x_obj, &x->x_minout);
        floatinlet_new(&x->x_obj, &x->x_maxout);
    }
    else{
        floatinlet_new(&x->x_obj, &x->x_minin);
        floatinlet_new(&x->x_obj, &x->x_maxin);
        floatinlet_new(&x->x_obj, &x->x_minout);
        floatinlet_new(&x->x_obj, &x->x_maxout);
    }
    return (x);
errstate:
    post("[rescale]: improper args");
    return NULL;
}

void rescale_setup(void){
    rescale_class = class_new(gensym("rescale"), (t_newmethod)rescale_new,
            (t_method)rescale_free, sizeof(t_rescale), 0, A_GIMME, 0);
    class_addfloat(rescale_class, (t_method)rescale_float);
    class_addlist(rescale_class, (t_method)rescale_list);
    class_addbang(rescale_class, (t_method)rescale_bang);
    class_addmethod(rescale_class, (t_method)rescale_set, gensym("set"), A_DEFFLOAT,0 );
    class_addmethod(rescale_class, (t_method)rescale_exp, gensym("exp"), A_DEFFLOAT, 0);
    class_addmethod(rescale_class, (t_method)rescale_clip, gensym("clip"), A_FLOAT, 0);
}
