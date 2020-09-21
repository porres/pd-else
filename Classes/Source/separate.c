// based on zexy/symbol2list

#include "m_pd.h"
#include <stdlib.h>
#include <string.h>

typedef struct _separate{
    t_object    x_obj;
    t_symbol   *separator;
    t_atom     *av;
    int         ac;
    int         arg_n; // reserved atoms (might be > ac)
}t_separate;

static t_class *separate_class;

static void set_separator(t_separate *x, t_symbol *s){
    if(s == gensym("empty")){
        x->separator = NULL;
        return;
    }
    if(strlen(s->s_name) > 1){
        pd_error(x, "[separate]: separator symbol must contain not more than one character");
        return;
    }
    else
        x->separator = s;
}

int ishex(const char *s){
    s++;
    if(*s == 'x' || *s == 'X')
        return(1);
    else
        return(0);
}

static void string2atom(t_atom *ap, char* cp, int clen){
    char *buf = getbytes(sizeof(char)*(clen+1));
    char *endptr[1];
    t_float ftest;
    strncpy(buf, cp, clen);
    buf[clen] = 0;
    ftest = strtod(buf, endptr);
    if(buf+clen != *endptr) // strtof() failed, we have a symbol
        SETSYMBOL(ap, gensym(buf));
    else{ // probably a number
        if(ishex(buf)) // test for hexadecimal (inf/nan are still floats)
            SETSYMBOL(ap, gensym(buf));
        else
            SETFLOAT(ap, ftest);
    }
    freebytes(buf, sizeof(char)*(clen+1));
}

static void separate_process(t_separate *x, t_symbol *s){
    char *cc;
    char *deli;
    int   dell;
    char *cp, *d;
    int i = 1;
    if(s == NULL){
        x->ac = 0;
        return;
    }
    cc = (char*)s->s_name;
    cp = cc;
    if(x->separator == NULL || x->separator == gensym("")){
        i = strlen(cc);
        if(x->arg_n < i){ // resize if necessary
            freebytes(x->av, x->arg_n *sizeof(t_atom));
            x->arg_n = i+10;
            x->av = getbytes(x->arg_n *sizeof(t_atom));
        }
        x->ac = i;
        while(i--)
            string2atom(x->av+i, cc+i, 1);
        if(x->ac)
            outlet_list(x->x_obj.ob_outlet, &s_list, x->ac, x->av);
        return;
    }
    deli = (char*)x->separator->s_name;
    dell = strlen(deli);
    while((d = strstr(cp, deli))){ // get the number of items
        if(d != NULL && d != cp)
            i++;
        cp = d+dell;
    }
    if(x->arg_n < i){ // resize if necessary
        freebytes(x->av, x->arg_n *sizeof(t_atom));
        x->arg_n = i+10;
        x->av = getbytes(x->arg_n *sizeof(t_atom));
    }
    x->ac = i;
    // parse the items into the list-buffer
    i = 0;
    // find the first item
    cp = cc;
    while(cp == (d = strstr(cp,deli)))
        cp += dell;
    while((d = strstr(cp, deli))){
        if(d != cp){
            string2atom(x->av+i, cp, d-cp);
            i++;
        }
        cp = d+dell;
    }
    if(cp)
        string2atom(x->av+i, cp, strlen(cp));
    if(x->ac)
        outlet_list(x->x_obj.ob_outlet, &s_list, x->ac, x->av);
}

static void separate_symbol(t_separate *x, t_symbol *s){
    if(!s || s == gensym(""))
        outlet_bang(x->x_obj.ob_outlet);
    else
       separate_process(x, s);
}

static void *separate_new(t_symbol *s, int ac, t_atom *av){
    t_separate *x = (t_separate *)pd_new(separate_class);
    s = NULL;
    x->ac = 0;
    x->arg_n = 16;
    x->av = getbytes(x->arg_n *sizeof(t_atom));
    x->separator =  gensym(" "); //
    if(ac){
        if(av->a_type == A_SYMBOL)
            set_separator(x, atom_getsymbol(av));
        else if(av->a_type == A_FLOAT)
            pd_error(x, "[separate]: separator needs be a symbol");
    }
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym(""));
    outlet_new(&x->x_obj, 0);
    return(x);
}

void separate_setup(void){
    separate_class = class_new(gensym("separate"), (t_newmethod)separate_new,
        0, sizeof(t_separate), 0, A_GIMME, 0);
    class_addsymbol(separate_class, separate_symbol);
    class_addmethod(separate_class, (t_method)set_separator, gensym(""), A_SYMBOL, 0);}
