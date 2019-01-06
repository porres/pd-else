// adapted from cyclone's fromsymbol

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "m_pd.h"

typedef struct _separate{
    t_object   x_ob;
    t_symbol  *x_separator;
}t_separate;

static t_class *separate_class;

char *mtok(char *input, char *delimiter){
    // adapted from stack overflow (designed to work like strtok)
    static char *string;
    if (input != NULL)     //if not passed null, use static var
        string = input;
    if (string == NULL)    //if reached the end, just return the static var (so it seems)
        return string;
    //return pointer of first occurence of delim
    //added, keep going until first non delim
    char *end = strstr(string, delimiter);
    while(end == string){
        *end = '\0';
        string = end + strlen(delimiter);
        end = strstr(string, delimiter);
    };
    if(end == NULL){    //if not found, just return the string
        char *temp = string;
        string = NULL;
        return temp;
    }
    char *temp = string;
    // set thing pointed to at end as null char, advance pointer to after delim
    *end = '\0';
    string = end + strlen(delimiter);
    return temp;
}

static void separate_separator(t_separate *x, t_symbol *s, int argc, t_atom * argv){
    t_symbol *dummy = s;
    dummy = NULL;
    if(!argc)
        x->x_separator = gensym(" ");
    else if(argc == 1 && argv->a_type == A_SYMBOL)
//        x->x_separator = atom_getsymbol(argv)->s_name[0];
        x->x_separator = atom_getsymbolarg(0, argc, argv);
    else
        pd_error(x, "separator message needs to contain only one symbol");
}

static void separate_symbol(t_separate *x, t_symbol *s){
    long unsigned int seplen = strlen(x->x_separator->s_name);
    seplen++;
    char *sep = t_getbytes(seplen * sizeof(*sep));
    memset(sep, '\0', seplen);
    strcpy(sep, x->x_separator->s_name);
    if(s){
        long unsigned int iptlen = strlen(s->s_name); // length of input string
// allocate t_atom[] on length of string (hacky way of making sure there's enough space)
        t_atom* out = t_getbytes(iptlen * sizeof(*out));
        iptlen++;
        char* newstr = t_getbytes(iptlen * sizeof(*newstr));
        memset(newstr, '\0', iptlen);
        strcpy(newstr,s->s_name);
        int atompos = 0; // position in atom
        //parsing by token
        char * ret = mtok(newstr, sep);
        while(ret != NULL){
            if(strlen(ret) > 0){
                    t_symbol * cursym = gensym(ret);
                    SETSYMBOL(&out[atompos], cursym);
                atompos++; //increment position in atom
            };
            ret = mtok(NULL,sep);
        };
        outlet_anything(((t_object *)x)->ob_outlet, out->a_w.w_symbol, atompos-1, out+1);
        t_freebytes(out, iptlen * sizeof(*out));
        t_freebytes(newstr, iptlen * sizeof(*newstr));
    };
    t_freebytes(sep, seplen * sizeof(*sep));
}

static void *separate_new(t_symbol * s, int argc, t_atom * argv){
    t_symbol *dummy = s;
    dummy = NULL;
    t_separate *x = (t_separate *)pd_new(separate_class);
    outlet_new((t_object *)x, &s_anything);
    if(argc == 0 || (argc == 1 && argv->a_type == A_SYMBOL)){
            separate_separator(x, 0, argc, argv);
    }
    else
        goto
            errstate;
    return(x);
	errstate:
		pd_error(x, "[separate]: improper args");
		return NULL;
}

void separate_setup(void){
    separate_class = class_new(gensym("separate"), (t_newmethod)separate_new, 0,
				 sizeof(t_separate), 0, A_GIMME, 0);
    class_addsymbol(separate_class, separate_symbol);
    class_addmethod(separate_class, (t_method)separate_separator,
                    gensym("separator"), A_GIMME, 0);
}
