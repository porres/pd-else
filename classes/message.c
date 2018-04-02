#include "m_pd.h"
#include <string.h>

static t_class *message_class;

typedef struct _message{
  t_object      x_obj;
  t_int         x_ac;
  t_atom       *x_av;
  t_symbol     *x_s;
}t_message;

static void message_output(t_message *x, t_symbol *s, int ac, t_atom *av){
    if(!ac && strcmp(s->s_name, ",")) // only a selector, but not a comma
        outlet_anything(x->x_obj.ob_outlet, s, 0, 0); // output selector
    else{
        char separator = gensym(",")->s_name[0]; // split character
        int i = 0, first = 1, n;
        while(i < ac){
            int j = i; // i = start point
            for(j; j < ac; j++){
                if((av+j)->a_type == A_SYMBOL && atom_getsymbol(av+j)->s_name[0] == separator)
                    break; // j = comma
            }
            n = j - i; // n = number of elements in a message (counting the comma element)
            
// begining of output messages
            if(first){
                if(n == 0){ // it's only the selector
                    if(!strcmp(s->s_name, "list")) // if selector is list, turn to bang
                        outlet_bang(x->x_obj.ob_outlet);
                    else if(!strcmp(s->s_name, ",")){
                        // do nothing
                    }
                    else
                        outlet_anything(x->x_obj.ob_outlet, s, n, av-1); // output selector
                }
                else{ // it's not only the selector (this can be the whole message if there's no comma)
                    if(!strcmp(s->s_name, ",")){
                        if((av)->a_type == A_FLOAT)
                            outlet_anything(x->x_obj.ob_outlet, &s_list, n, av);
                        else if((av)->a_type == A_SYMBOL){
                            t_symbol *sym = atom_getsymbol(av);
                            outlet_anything(x->x_obj.ob_outlet, sym, n-1, av+1);
                        }
                    }
                    else
                        outlet_anything(x->x_obj.ob_outlet, s, n, av);
                }
                first = 0;
            }
            else{ // messages after a comma
                if((av+i)->a_type == A_SYMBOL)
                    outlet_anything(x->x_obj.ob_outlet, atom_getsymbol(av+i), n-1, av + i+1);
                else if((av+i)->a_type == A_FLOAT)
                    outlet_anything(x->x_obj.ob_outlet, &s_list, n, av+i);
            }
// end of output messages
            i = j + 1; // next start point
        } // end of while
    }
}



static void message_send(t_message *x, t_symbol *s, int ac, t_atom *av){
    if(!ac && strcmp(s->s_name, ";")) // only a selector (but not a semicolon
        outlet_anything(x->x_obj.ob_outlet, s, 0, 0); // output selector
    else{
        char send = gensym(";")->s_name[0]; // send character
        int i = 0, first = 1, n;
        while(i < ac){
            int j = i; // i = start point
            for(j; j < ac; j++){
                if((av+j)->a_type == A_SYMBOL && atom_getsymbol(av+j)->s_name[0] == send)
                    break; // j = semicolon
            }
            n = j - i; // n = number of elements in a message (counting the semicolon element)
            if(first){
                if(n == 0){ // it's only the selector
                    if(!strcmp(s->s_name, "list")) // if selector is list, turn to bang
                        outlet_bang(x->x_obj.ob_outlet);
                    else if(!strcmp(s->s_name, ";")){
                        // do nothing
                    }
                    else
                        outlet_anything(x->x_obj.ob_outlet, s, n, av-1); // output selector
                }
                else{ // it's not only the selector (this can be the whole message if there's no comma)
                    if(!strcmp(s->s_name, ";")){
                        if(n > 1){ // needs to have more than the send symbol
                            t_symbol *send_sym;
                            if((av)->a_type == A_FLOAT){
                                send_sym = gensym("float");
                                if(send_sym->s_thing){ // there's a receive!
                                    if((av+1)->a_type == A_FLOAT)
                                        typedmess(send_sym->s_thing, &s_list, n-1, av+1);
                                    else if((av+1)->a_type == A_SYMBOL)
                                        typedmess(send_sym->s_thing, atom_getsymbol(av+1), n-2, av+2);
                                }
                            }
                            else if((av)->a_type == A_SYMBOL){
                                send_sym = atom_getsymbol(av);
                                if(send_sym->s_thing){ // there's a receive!
                                    if((av+1)->a_type == A_FLOAT)
                                        typedmess(send_sym->s_thing, &s_list, n-1, av+1);
                                    else if((av+1)->a_type == A_SYMBOL)
                                        typedmess(send_sym->s_thing, atom_getsymbol(av+1), n-2, av+2);
                                }
                            }
                        }
                    }
                    else
                        outlet_anything(x->x_obj.ob_outlet, s, n, av);
                }
                first = 0;
            }
            else{ // following messages after a semicolon
                if(n > 1){ // needs to have more than the send symbol
                    t_symbol *send_sym;
                    t_symbol *sel;
                    if((av+i)->a_type == A_FLOAT){
                        send_sym = gensym("float");
                        if(send_sym->s_thing){ // there's a receive!
                            if((av+i+1)->a_type == A_FLOAT)
                                typedmess(send_sym->s_thing, &s_list, n-1, av + i+1);
                            else if((av+i+1)->a_type == A_SYMBOL)
                                typedmess(send_sym->s_thing, atom_getsymbol(av+i+1), n-2, av + i+2);
                        }
                    }
                    else if((av+i)->a_type == A_SYMBOL){
                        send_sym = atom_getsymbol(av+i);
                         if(send_sym->s_thing){ // there's a receive
                             if((av+i+1)->a_type == A_FLOAT)
                                 typedmess(send_sym->s_thing, &s_list, n-1, av + i+1);
                             else if((av+i+1)->a_type == A_SYMBOL)
                                 typedmess(send_sym->s_thing, atom_getsymbol(av+i+1), n-2, av + i+2);
                         }
                    }
                }
            }
            i = j + 1; // next start point
        } // end of while
    }
}

static void message_anything(t_message *x, t_symbol *s, int ac, t_atom *av){
    if(x->x_av)
        message_output(x, x->x_s, x->x_ac, x->x_av);
}

static void message_click(t_message *x){
    if(x->x_av)
        message_send(x, x->x_s, x->x_ac, x->x_av);
}

static void message_set(t_message *x, t_symbol *s, int ac, t_atom *av){
    if(!ac){
        x->x_ac = 0;
        x->x_av = NULL;
    }
    else{
        if(av->a_type == A_SYMBOL){
            x->x_s = atom_getsymbol(av++);
            ac--;
        }
        else
            x->x_s = &s_list;
        x->x_ac = ac;
        x->x_av = (t_atom *)getbytes(x->x_ac * sizeof(t_atom));
        t_int i;
        for(i = 0; i < ac; i++)
            x->x_av[i] = av[i];
    }
}

static void message_free(t_message *x){
  if(x->x_av)
    freebytes(x->x_av, x->x_ac * sizeof(t_atom));
}

static void *message_new(t_symbol *s, int ac, t_atom *av){
    t_message *x = (t_message *)pd_new(message_class);
    if(!ac){
        x->x_ac = 0;
        x->x_av = NULL;
    }
    else{
        if(av->a_type == A_SYMBOL){ // get selector
            x->x_s = atom_getsymbol(av++);
            ac--;
        }
        else
            x->x_s = &s_list; // list selector otherwise
        x->x_ac = ac;
        x->x_av = (t_atom *)getbytes(x->x_ac * sizeof(t_atom));
        int i;
        for(i = 0; i < ac; i++)
            x->x_av[i] = av[i];
    }
    outlet_new(&x->x_obj, &s_anything);
    return(x);
}

void message_setup(void){
  message_class = class_new(gensym("message"), (t_newmethod)message_new,
        (t_method)message_free, sizeof(t_message), 0, A_GIMME, 0);
  class_addanything(message_class, (t_method)message_anything);
  class_addmethod(message_class, (t_method)message_set, gensym("set"), A_GIMME, 0);
  class_addmethod(message_class, (t_method)message_click, gensym("click"), 0);
}
