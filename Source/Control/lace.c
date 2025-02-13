

#include <m_pd.h>
//#include "else_alloca.h"
#include <stdlib.h>

static t_class *lace_class, *lace_inlet_class;

struct _lace_inlet;

typedef struct _lace{
    t_object             x_obj;
    int                  x_numinlets;
    int                  x_length;     // total length of all atoms from lace_inlet
    int                  x_zero;
    struct _lace_inlet  *x_ins;
    t_symbol            *x_ignore;
}t_lace;

typedef struct _lace_inlet{
    t_class*    x_pd;
    t_atom*     x_atoms;
    int         x_numatoms;
    int         x_id;
    int         x_trig;
    t_lace     *x_owner;
    t_symbol   *x_ignore;
}t_lace_inlet;

static void atoms_copy(int ac, t_atom *from, t_atom *to){
    for(int i = 0; i < ac; i++)
        to[i] = from[i];
}

static void lace_output(t_lace *x){
    int i = 0;
    int maxn = 0;
    int totaln = 0;
    for(i = 0; i < x->x_numinlets; i++){
        int size = x->x_ins[i].x_numatoms;
        totaln += size;
        if(size > maxn)
            maxn = size;
    }
    if(x->x_zero) // interleave with zeros mode
        totaln = maxn *  x->x_numinlets;
    t_atom z[1];
    SETFLOAT(z, 0);
    t_atom *outatom = (t_atom *)getbytes(totaln * sizeof(t_atom));
    int n = 0;
    int offset = 0;
    while(n < maxn){
        for(i = 0; i < x->x_numinlets; i++){
            if(n < x->x_ins[i].x_numatoms)
                outatom[offset++] = x->x_ins[i].x_atoms[n];
            else if(x->x_zero)
                outatom[offset++] = z[0];
        }
        n++;
    }
        outlet_list(x->x_obj.ob_outlet, &s_list, totaln, outatom);
    freebytes(outatom, totaln * sizeof(t_atom));
}

static void lace_inlet_atoms(t_lace_inlet *x, int ac, t_atom * av){
    t_lace *owner = x->x_owner;
    freebytes(x->x_atoms, x->x_numatoms * sizeof(t_atom));
    owner->x_length -= x->x_numatoms;
    x->x_atoms = (t_atom *)getbytes(ac * sizeof(t_atom));
    owner->x_length += ac;
    x->x_numatoms = (t_int)ac;
    atoms_copy(ac, av, x->x_atoms);
}

static void lace_inlet_list(t_lace_inlet *x, t_symbol* s, int ac, t_atom* av){
    x->x_ignore = s;
    lace_inlet_atoms(x, ac, av);
    if(x->x_trig)
        lace_output(x->x_owner);
}

/*static void lace_inlet_anything(t_lace_inlet *x, t_symbol* s, int ac, t_atom* av){
    t_atom* at = ALLOCA(t_atom, ac + 1);
    SETSYMBOL(at, s);
    atoms_copy(ac, av, at+1);
    lace_inlet_list(x, 0, ac+1, at);
    FREEA(at, t_atom, ac + 1);
}*/

static void* lace_free(t_lace *x){
    for(int i = 0; i < x->x_numinlets; i++)
        freebytes(x->x_ins[i].x_atoms, x->x_ins[i].x_numatoms*sizeof(t_atom));
    freebytes(x->x_ins, x->x_numinlets * sizeof(t_lace_inlet));
    return(void *)free;
}

static void *lace_new(t_symbol *s, int ac, t_atom* av){
    t_lace *x = (t_lace *)pd_new(lace_class);
    x->x_ignore = s;
    int n = 2;
    x->x_zero = 0;
/////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            t_float aval = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    n = aval;
                    break;
                case 1:
                    goto errstate;
                    break;
                default:
                    break;
            };
            ac--, av++;
            argnum++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            if(atom_getsymbolarg(0, ac, av) == gensym("-z")){
                x->x_zero = 1;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
/////////////////////////////////////////////////////////////////////////////////////
    x->x_numinlets = n < 2 ? 2 : n > 512 ? 512 : n;
    x->x_ins = (t_lace_inlet *)getbytes(x->x_numinlets * sizeof(t_lace_inlet));
    x->x_length = x->x_numinlets;
    for(int i = 0; i < x->x_numinlets; ++i){
        x->x_ins[i].x_pd    = lace_inlet_class;
        x->x_ins[i].x_atoms = (t_atom *)getbytes(1 * sizeof(t_atom));
        SETFLOAT(x->x_ins[i].x_atoms, 0);
        x->x_ins[i].x_numatoms = 0;
        x->x_ins[i].x_owner = x;
        x->x_ins[i].x_id = i;
        x->x_ins[i].x_trig = i == 0;
        inlet_new((t_object *)x, &(x->x_ins[i].x_pd), 0, 0);
    };
    outlet_new(&x->x_obj, &s_list);
    return(x);
errstate:
    pd_error(x, "[lace]: improper args");
    return(NULL);
}

extern void lace_setup(void){
    t_class* c = NULL;
    c = class_new(gensym("lace-inlet"), 0, 0, sizeof(t_lace_inlet), CLASS_PD, 0);
    if(c){
        class_addlist(c, (t_method)lace_inlet_list);
//        class_addanything(c, (t_method)lace_inlet_anything);
    }
    lace_inlet_class = c;
    c = class_new(gensym("lace"), (t_newmethod)lace_new, (t_method)lace_free,
        sizeof(t_lace), CLASS_NOINLET, A_GIMME, 0);
    lace_class = c;
}
