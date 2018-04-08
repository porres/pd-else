#include "m_pd.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static t_class *message_class;

typedef struct _message{
  t_object      x_obj;
  t_int         x_ac;
  t_atom       *x_av;
  t_symbol     *x_s;
}t_message;

static void message_send(t_message *x, t_symbol *s, int ac, t_atom *av){
    if(!ac){ // only a selector (but not ";" or ",")
        if(!strcmp(s->s_name, ";") || !strcmp(s->s_name, ",")){
        }  // ignore
        else // output selector
            outlet_anything(x->x_obj.ob_outlet, s, 0, 0);
    }
    else{ // Not only a Selector
        char separator = gensym(",")->s_name[0]; // split character
        char send = gensym(";")->s_name[0]; // send character
        t_int send_flag = 0;
        t_int last_send_flag = 0, send_this = 0;
        t_int last_comma_flag = 0, split_this = 0;
        t_int i = 0, j = 0, n = 0;
        t_symbol *send_sym = NULL;
        while(i < ac){
            if(last_send_flag == 1)
                send_flag = 1; // from now on, send all the shit
            send_this = last_send_flag;
            split_this = last_comma_flag;
            if(send_this && split_this)
                send_this = 0;
            for(j = i; j < ac; j++){ // <=====    FOR
                if((av+j)->a_type == A_SYMBOL && atom_getsymbol(av+j)->s_name[0] == send){
                    last_comma_flag = 0; // next break point is a send, so comma = 0
                    last_send_flag = 1; // update for next round cause next break point is a send
                    break;
                }
                else if((av+j)->a_type == A_SYMBOL && atom_getsymbol(av+j)->s_name[0] == separator){
                    last_send_flag = 0;  // next break is a comma, so fuck this
                    last_comma_flag = 1; // update for next round cause next break point is a comma
                    break;
                }
            } // <=====    END OF FOR
            n = j - i; // j = breakpoint / n = elements in a message - counting the comma/semicolon 1st element
            if(i == 0){ // <=====    FIRST MESSAGE!!!!
                if(n == 0){ // the message only contains a selector
                    if(!strcmp(s->s_name, "list")) // if selector is list, turn to bang
                        outlet_bang(x->x_obj.ob_outlet);
                    else if(!strcmp(s->s_name, ";") || !strcmp(s->s_name, ",")){
                    }    // ignore ";" or ","
                    else // output selector
                        outlet_anything(x->x_obj.ob_outlet, s, n, av-1);
                }
                else{ // it's not only the selector
                    if(!strcmp(s->s_name, ";")){ // IT'S A SEND!!!!!!
                        if((av)->a_type == A_FLOAT)
                            send_sym = gensym("float");
                        else if((av)->a_type == A_SYMBOL)
                            send_sym = atom_getsymbol(av);
                        if(send_sym->s_thing && n > 1){ // there's a receive and a message, send it!
                            if((av+1)->a_type == A_FLOAT)
                                typedmess(send_sym->s_thing, &s_list, n-1, av+1);
                            else if((av+1)->a_type == A_SYMBOL)
                                typedmess(send_sym->s_thing, atom_getsymbol(av+1), n-2, av+2);
                        }
                        last_send_flag = 1; // mark it as a flag
                    }
                    else if(!strcmp(s->s_name, ",")){ // Selector is a comma, so ignore it and output the rest
                        if((av)->a_type == A_FLOAT)
                            outlet_anything(x->x_obj.ob_outlet, &s_list, n, av);
                        else if((av)->a_type == A_SYMBOL)
                            outlet_anything(x->x_obj.ob_outlet, atom_getsymbol(av), n-1, av+1);
                    }
                    else // output the message (can be the whole message if there's no breakage)
                        outlet_anything(x->x_obj.ob_outlet, s, n, av);
                }
            }
            else{ // <=====    NEXT MESSAGES!!!!
                if(send_flag){ // we're sending stuff
                    if(send_this){ // it's a new send!
                        if((av+i)->a_type == A_FLOAT)
                            send_sym = gensym("float");
                        else if((av+i)->a_type == A_SYMBOL)
                            send_sym = atom_getsymbol(av+i);
                        if(send_sym->s_thing && n > 1){ // there's a receive and a message, send it!
                            if((av+i+1)->a_type == A_FLOAT)
                                typedmess(send_sym->s_thing, &s_list, n-1, av+i+1);
                            else if((av+i+1)->a_type == A_SYMBOL)
                                typedmess(send_sym->s_thing, atom_getsymbol(av+i+1), n-2, av+i+2);
                        }
                    }
                    else if(split_this){ // it's an old send
                        if(send_sym->s_thing){ // there's a receive and a message, send it!
                            if((av+i)->a_type == A_FLOAT)
                                typedmess(send_sym->s_thing, &s_list, n, av+i);
                            else if((av+i)->a_type == A_SYMBOL){
                                if((!strcmp(atom_getsymbol(av+i)->s_name, ",") ||
                                    !strcmp(atom_getsymbol(av+i)->s_name, ";")) && !n){
                                } // ignore if the message is only a comma or semicolon selector
                                else
                                    typedmess(send_sym->s_thing, atom_getsymbol(av+i), n-1, av+i+1);
                            }
                        }
                    }
                }
                else{ // we're *NOT* sending stuff
                    if((av+i)->a_type == A_SYMBOL){
                        if((!strcmp(atom_getsymbol(av+i)->s_name, ",") ||
                            !strcmp(atom_getsymbol(av+i)->s_name, ";")) && !n){
                        } // ignore if the message is only a comma or semicolon selector
                        else
                            outlet_anything(x->x_obj.ob_outlet, atom_getsymbol(av+i), n-1, av + i+1);
                    }
                    else if((av+i)->a_type == A_FLOAT)
                        outlet_anything(x->x_obj.ob_outlet, &s_list, n, av+i);
                }
            }
            i = j + 1; // next start point
        } // <=====    END OF WHILE
    }
}

static void message_anything(t_message *x, t_symbol *s, int ac, t_atom *av){
    if(x->x_av){
        char temp[MAXPDSTRING];
        t_symbol *sel = x->x_s;
        t_atom *a = (t_atom *)getbytes((x->x_ac+1) * sizeof(t_atom));
        t_int extra = 0, j = 0, index = 0;
        strcpy(temp, x->x_s->s_name);
// selector
        if(!strncmp(temp, "$a", 2)){ // selector might need replacement
            memmove(temp, temp+2, strlen(temp));
            int isnumber = 1;
            for(j = 0; j < (int)strlen(temp); j++){
                if((int)(temp[j]) < 48 || (int)(temp[j]) > 57){
                    isnumber = 0;
                    break;
                }
            }
            if(isnumber)
                index = atoi(temp);
            if(index > 0){ // REPLACE
                if(index > ac){
                    pd_error(x, "[message]: argument $a%ld outta range", index);
                    extra = 1;
                    sel = &s_list;
//                    a = (t_atom *)getbytes((x->x_ac+extra) * sizeof(t_atom));
                    SETFLOAT(&a[0], 0);
                }
                else{
                    if(av[index-1].a_type == A_SYMBOL)
                        sel = atom_getsymbol(&av[index-1]);
                    else if(av[index-1].a_type == A_FLOAT){
                        extra = 1;
                        sel = &s_list;
//                        a = (t_atom *)getbytes((x->x_ac+extra) * sizeof(t_atom));
                        a[0] = av[index-1];
                    }
                }
            }
        }
// atom(s)
        for(int i = 0; i < x->x_ac; i++){
            if(x->x_av[i].a_type == A_SYMBOL){
                strcpy(temp, atom_getsymbol(&x->x_av[i])->s_name);
                if(!strncmp(temp, "$a", 2)){
                    memmove(temp, temp+2, strlen(temp));
                    int isnumber = 1;
                    for(j = 0; j < (int)strlen(temp); j++){
                        if((int)(temp[j]) < 48 || (int)(temp[j]) > 57){
                            isnumber = 0;
                            break;
                        }
                    }
                    if(isnumber){
                        index = atoi(temp);
                        if(index > 0){
                            if(index > ac){
                                pd_error(x, "[message]: argument $a%ld outta range", index);
                                SETFLOAT(&a[i+extra], 0);
                            }
                            else
                                a[i+extra] = av[index-1];
                        }
                        else
                            a[i+extra] = x->x_av[i];
                    }
                    else
                        a[i+extra] = x->x_av[i];
                }
                else
                    a[i+extra] = x->x_av[i];
            }
            else if(x->x_av[i].a_type == A_FLOAT)
                a[i+extra] = x->x_av[i];
        }
        message_send(x, sel, x->x_ac+extra, a);
//        outlet_anything(x->x_obj.ob_outlet, sel, x->x_ac+extra, a);
    }
}

static void message_click(t_message *x){
    if(x->x_av){
        message_anything(x, &s_bang, 0, 0);
//        message_send(x, x->x_s, x->x_ac, x->x_av);
    }
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
            x->x_s = gensym("list");
        x->x_ac = ac;
        x->x_av = (t_atom *)getbytes(x->x_ac * sizeof(t_atom));
        t_int i;
        for(i = 0; i < ac; i++)
            x->x_av[i] = av[i];
//        outlet_anything(x->x_obj.ob_outlet, x->x_s, x->x_ac, x->x_av);
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
        for(i = 0; i < ac; i++){
            x->x_av[i].a_type = av[i].a_type;
            if(av[i].a_type == A_FLOAT)
                x->x_av[i].a_w.w_float = av[i].a_w.w_float;
            else if(av[i].a_type == A_SYMBOL)
                x->x_av[i].a_w.w_symbol = av[i].a_w.w_symbol;
        }
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
