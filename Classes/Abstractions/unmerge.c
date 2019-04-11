
#include "m_pd.h"

typedef struct _unmerge{
    t_object    x_obj;
    int         x_numouts; //number of outlets not including extra outlet
    float         x_size;
    t_outlet  **x_outlets; //numouts + 1 for extra outlet
}t_unmerge;

static t_class *unmerge_class;

static void unmerge_list(t_unmerge *x, t_symbol *s, int argc, t_atom *argv){
    t_symbol *dummy = s;
    dummy = NULL;
    int size = x->x_size < 1 ? 1 : (int)x->x_size;
    int numouts = x->x_numouts;
    int numleft = argc;
    int i;
    for(i = 0; i < numouts; i++){
        if(numleft >= size){ // if only one... out float
            if(size == 1 && argv->a_type == A_FLOAT)
                outlet_float(x->x_outlets[i], argv->a_w.w_float);
            else if(argv->a_type == A_FLOAT) // if first is float... out list
                outlet_list(x->x_outlets[i],  &s_list, size, argv);
            else
                outlet_anything(x->x_outlets[i],  &s_ , size, argv);
            numleft -= size;
            argv += size;
        }
        else if ((numleft > 0) && (numleft < size)){
            if(numleft == 1 && argv->a_type == A_FLOAT)
                outlet_float(x->x_outlets[i], argv->a_w.w_float);
            else if(argv->a_type == A_FLOAT) // if first is float... out list
                outlet_list(x->x_outlets[i],  &s_list, numleft, argv);
            else
                outlet_anything(x->x_outlets[i], &s_, numleft, argv);
            numleft = 0;
            break;
        }
        else if(numleft <= 0) break;
    };
    if(numleft){ // extra outlet
        if(numleft == 1 && argv->a_type == A_FLOAT)
            outlet_float(x->x_outlets[i], argv->a_w.w_float);
        else if(argv->a_type == A_FLOAT) // if first is float... out list
            outlet_list(x->x_outlets[numouts],  &s_list, numleft, argv);
        else
            outlet_anything(x->x_outlets[numouts], &s_, numleft, argv);
    };
}

static void unmerge_anything(t_unmerge * x, t_symbol *s, int argc, t_atom * argv){
    if(s){
        int i;
        t_atom* newlist = t_getbytes((argc + 1) * sizeof(*newlist));
        SETSYMBOL(&newlist[0],s);
        for(i=0;i<argc;i++)
            newlist[i+1] = argv[i];
        unmerge_list(x, NULL, argc+1, newlist);
        t_freebytes(newlist, (argc + 1) * sizeof(*newlist));
    }
    else unmerge_list(x, NULL, argc, argv);
}

static void unmerge_float(t_unmerge *x, t_float f){
    outlet_float(x->x_outlets[0], f);
}

static void unmerge_symbol(t_unmerge *x, t_symbol *s){
    outlet_symbol(x->x_outlets[0], s);
}

static void unmerge_free(t_unmerge *x){
    if (x->x_outlets)
        freebytes(x->x_outlets, (x->x_numouts+1) * sizeof(*x->x_outlets));
}

static void *unmerge_new(t_floatarg f1, t_floatarg f2){
    t_unmerge *x = (t_unmerge *)pd_new(unmerge_class);
    int n = (int)f1;
    x->x_numouts = n < 2 ? 2 : n > 512 ? 512 : n;
    x->x_size = f2;
    x->x_outlets = (t_outlet **)getbytes((x->x_numouts+1) * sizeof(t_outlet *));
    floatinlet_new(&x->x_obj, &x->x_size);
    for(int i = 0; i <= x->x_numouts; i++)
        x->x_outlets[i] = outlet_new(&x->x_obj, &s_anything);
    return(x);
}

void unmerge_setup(void){
    unmerge_class = class_new(gensym("unmerge"), (t_newmethod)unmerge_new,
        (t_method)unmerge_free, sizeof(t_unmerge), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addfloat(unmerge_class, unmerge_float);
    class_addlist(unmerge_class, unmerge_list);
    class_addanything(unmerge_class, unmerge_anything);
    class_addsymbol(unmerge_class, unmerge_symbol);
}

