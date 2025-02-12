// porres

#include <m_pd.h>
#include <math.h>

typedef struct _delace{
    t_object    x_obj;
    int         x_nouts; //number of outlets
    int         x_trim;
    int         x_zero;
    t_symbol   *x_ignore;
    t_outlet  **x_outlets;
}t_delace;

static t_class *delace_class;

static void delace_list(t_delace *x, t_symbol *s, int ac, t_atom *av){
    if(!ac){
        pd_error(x , "delace: no method for 'bang'");
        return;
    }
    x->x_ignore = s;
    int i, j, k;
    int size = (int)ceil((float)ac / (float)x->x_nouts);
    int step = (int)ceil((float)ac / (float)size);
    t_atom at[size];
    for(i = 0; i < size; i++)
        SETFLOAT(at+i, 0);
    for(i = (x->x_nouts - 1); i >= 0; i--){
        for(j = 0, k = i; k < ac; j++, k += step){
            if((av+k)->a_type == A_FLOAT)
                SETFLOAT(at+j, (av+k)->a_w.w_float);
            else
                SETSYMBOL(at+j, (av+k)->a_w.w_symbol);
        }
        if(x->x_zero)
            outlet_list(x->x_outlets[i],  &s_list, size, at);
        else if(j > 0)
            outlet_list(x->x_outlets[i],  &s_list, j, at);
    }
}

/*static void delace_anything(t_delace * x, t_symbol *s, int ac, t_atom * av){
    t_atom *newlist = t_getbytes((ac+1) * sizeof(*newlist));
    SETSYMBOL(&newlist[0], s);
    for(int i = 0; i < ac; i++)
        newlist[i+1] = av[i];
    delace_list(x, NULL, ac+1, newlist);
    t_freebytes(newlist, (ac+1) * sizeof(*newlist));
}*/

static void delace_free(t_delace *x){
    if(x->x_outlets)
        freebytes(x->x_outlets, (x->x_nouts) * sizeof(*x->x_outlets));
}

static void *delace_new(t_symbol *s, int ac, t_atom* av){
    t_delace *x = (t_delace *)pd_new(delace_class);
    x->x_ignore = s;
    int n = 2;
    x->x_zero = 0;
    if(av->a_type == A_SYMBOL){
        if(atom_getsymbol(av) == gensym("-z")){
            x->x_zero = 1;
            ac--, av++;
        }
        else
            goto errstate;
    }
    if(ac)
        n = atom_getint(av);
/////////////////////////////////////////////////////////////////////////////////////
    x->x_nouts = n < 2 ? 2 : n > 512 ? 512 : n;
    x->x_outlets = (t_outlet **)getbytes((x->x_nouts) * sizeof(t_outlet *));
    for(int i = 0; i < x->x_nouts; i++)
        x->x_outlets[i] = outlet_new(&x->x_obj, &s_anything);
    return(x);
    errstate:
        pd_error(x, "[delace]: improper args");
        return(NULL);
}

void delace_setup(void){
    delace_class = class_new(gensym("delace"), (t_newmethod)delace_new,
        (t_method)delace_free, sizeof(t_delace), 0, A_GIMME, 0);
    class_addlist(delace_class, delace_list);
//    class_addanything(delace_class, delace_anything);
}
