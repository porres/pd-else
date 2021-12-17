// porres 2021

#include "m_pd.h"
#include <stdlib.h>
#include <string.h>

#define DEFNTICKS 960

typedef struct _metronome{
    t_object    x_obj;
    t_clock    *x_clock;
    t_symbol   *x_sig;
    t_int       x_pause;
    t_int       x_running;
    t_int       x_freeze;
    t_int       x_dir;
    t_int       x_sigchange;
    t_int       x_group;
    t_int       x_ticks;
    t_int       x_tickcount;
    t_int       x_subdiv;
    t_int       x_n_subdiv;
    t_int       x_barcount;
    t_int       x_tempocount;
    t_float     x_tempo;
    t_float     x_n_tempo;
    t_float     x_tempo_fig;
    t_float     x_tempo_div;
    t_outlet   *x_barout;
    t_outlet   *x_tempoout;
    t_outlet   *x_subdivout;
    t_outlet   *x_phaseout;
    t_outlet   *x_tickout;
}t_metronome;

static t_class *metronome_class;

static void metronome_stop(t_metronome *x);
    
static void metronome_div(t_metronome *x, t_atom *av){
    t_float f1, f2;
    if(av->a_type == A_SYMBOL){
        pd_error(x, "[metronome]: wrong time signature symbol");
        return;
    }
    else
        f1 = atom_getfloat(av++);
    if(av->a_type == A_SYMBOL){
        pd_error(x, "[metronome]: wrong time signature symbol");
        return;
    }
    else
        f2 = atom_getfloat(av);
    if(f1 <= 0 || f2 <= 0){
        pd_error(x, "[metronome]: wrong time signature symbol");
        return;
    }
    t_float div = f1 / f2;
//    post("metronome f1(%g) / f2(%g) ==> div = %g", f1, f2, div);
    if(!x->x_group){
//        post("no group, so subdiv = 1 and group = f1");
        x->x_group = x->x_n_tempo = (int)f1;
        x->x_n_subdiv = 1;
    }
    else{
        x->x_n_subdiv = (int)(f1/((t_float)x->x_group));
        x->x_n_tempo = x->x_group;
//        post("subdiv = [%d]",x->x_subdiv);
    }
//    post("div * 4 = %g", div);
    x->x_tempo_div = (float)(div * 4/(t_float)x->x_group);
//    post(" =====> tempo-divider = %g", x->x_tempo_div);
    if(!x->x_freeze)
        clock_setunit(x->x_clock, x->x_tempo * x->x_tempo_div / x->x_ticks, 0);
}

static void string2atom(t_atom *ap, char* ch, int clen){
    char *buf = getbytes(sizeof(char)*(clen+1));
    char *endptr[1];
    strncpy(buf, ch, clen);
    buf[clen] = 0;
    t_float ftest = strtod(buf, endptr);
    if(buf+clen == *endptr) // float
        SETFLOAT(ap, ftest);
    else
        SETSYMBOL(ap, gensym(buf));
    freebytes(buf, sizeof(char)*(clen+1));
}

static void metronome_symbol(t_metronome *x, t_symbol *s){
    char *ch = (char*)s->s_name;
    if(strstr(ch, "/")){
        t_atom *av = getbytes(2*sizeof(t_atom));
        char *d = strstr(ch, "/");
        if(d == ch || !strcmp(d, "/"))
            goto error;
        string2atom(av+0, ch, d-ch);
        ch = d+1;
        if(strstr(ch, "/"))
            goto error;
        string2atom(av+1, ch, strlen(ch));
        metronome_div(x, av);
    }
    else{
    error:
        pd_error(x, "[metronome]: wrong time signature symbol ");
    }
}

static void metronome_tick(t_metronome *x){
    outlet_float(x->x_tickout, x->x_tickcount);
    outlet_float(x->x_phaseout, (float)x->x_tickcount / (float)x->x_ticks);
    if(x->x_barcount < 0){
        metronome_stop(x);
        return;
    }
    if(x->x_tickcount == 0){
//        post("========> x->x_tickcount == 0");

        outlet_float(x->x_subdivout, x->x_subdiv = 1);
        
        t_int tempoout = x->x_tempocount + 1;
        t_int barout = x->x_barcount + 1;
        if(x->x_dir){
//            post("x_dir x->x_tempocount (%d)", x->x_tempocount);
            x->x_tempocount++;
//            post("x->x_tempocount++ (%d)", x->x_tempocount);
        }
        else
            x->x_tempocount--;
        if(x->x_tempocount == x->x_n_tempo){
            x->x_barcount++;
            x->x_tempocount = 0;
        }
        else if(x->x_tempocount < 0){
            x->x_tempocount = (x->x_n_tempo - 1);
            x->x_barcount--;
        }
        outlet_float(x->x_tempoout, tempoout);
//        post("~~~~~~~~~~ BEFORE BAR OUT ~~~~~~~~~~");
//        post("barout (%d)", barout);
        if(x->x_dir && tempoout == 1)
            outlet_float(x->x_barout, barout);
        else if(!x->x_dir && tempoout == x->x_n_tempo)
            outlet_float(x->x_barout, barout);
//        post("~~~~~~~~~~ AFTER BAR OUT ~~~~~~~~~~");
        outlet_bang(x->x_obj.ob_outlet);
        if(x->x_sigchange && tempoout == 1){
//            post("==x->x_sigchange");
            metronome_symbol(x, x->x_sig);
            x->x_sigchange = 0;
        }
    }
    else{
        t_int div = (int)(x->x_tickcount / (x->x_ticks / x->x_n_subdiv)) + 1;
        if(div != x->x_subdiv)
            outlet_float(x->x_subdivout, x->x_subdiv = div);
    }
    if(x->x_dir){
        x->x_tickcount++;
        if(x->x_tickcount == x->x_ticks)
            x->x_tickcount = 0;
    }
    else{
        x->x_tickcount--;
        if(x->x_tickcount < 0)
            x->x_tickcount = x->x_ticks - 1;
    }
    clock_delay(x->x_clock, 1);
}

static void metronome_float(t_metronome *x, t_float f){
    if(f != 0){
        x->x_pause = 0;
        x->x_running = 1;
        if(!x->x_freeze)
            metronome_tick(x);
    }
    else{
        x->x_pause = 1;
        x->x_running = 0;
        clock_unset(x->x_clock);
    }
}

/*static void metronome_stop(t_metronome *x){
    metronome_float(x, 0);
    x->x_pause = 0;
    x->x_running = 0;
    x->x_barcount = x->x_tempocount = x->x_tickcount = 0;
}*/

static void metronome_start(t_metronome *x){
//    post("--------start--------");
    x->x_barcount = x->x_tempocount = x->x_tickcount = 0;
//    if(!x->x_running){
//        x->x_running = 1;
        metronome_float(x, 1);
//    }
}

static void metronome_rewind(t_metronome *x){
    x->x_barcount = x->x_tempocount = x->x_tickcount = 0;
}

static void metronome_stop(t_metronome *x){
    metronome_float(x, 0);
    metronome_rewind(x);
}

/*static void metronome_pause(t_metronome *x){
    metronome_float(x, 0);
}

static void metronome_play(t_metronome *x){
    metronome_float(x, 1);
}*/

static void metronome_timesig(t_metronome *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac > 0){
        if(ac >= 2){
            if((av+1)->a_type == A_SYMBOL){
                pd_error(x, "[metronome]: invalid group number");
                x->x_group = 0;
            }
            else{
                t_int arg = (t_int)(atom_getfloat(av+1));
                if(arg > 0){
                    x->x_group = arg;
//                    post("group = %d", x->x_group);
                }
                else{
                    pd_error(x, "[metronome]: invalid group number [%d]", (int)arg);
                    x->x_group = 0;
                }
            }
        }
        else{
//            post("no group");
            x->x_group = 0;
        }
        if(av->a_type == A_SYMBOL){
            x->x_sig = atom_getsymbol(av);
            if(!x->x_group){
//                post("check composites");
                if(x->x_sig == gensym("6/8")){
//                    post("found 6/8 - group = 2");
                     x->x_group = 2;
                }
                else if(x->x_sig == gensym("9/8")){
//                    post("found 9/8 - group = 3");
                     x->x_group = 3;
                }
                else if(x->x_sig == gensym("12/8")){
//                    post("found 12/8 - group = 4");
                     x->x_group = 4;
                }
            }
            if(x->x_tickcount == 0 && x->x_tempocount == 0)
                metronome_symbol(x, x->x_sig);
            else
                x->x_sigchange = 1;
        }
        else
            pd_error(x, "[metronome]: wrong time signature symbol");
    }
    else
        pd_error(x, "[metronome]: wrong time signature symbol");
//    post("x->x_n_tempo = %d / x->x_n_subdiv = %d ", x->x_n_tempo, x->x_n_subdiv);
}

static void metronome_tempo(t_metronome *x, t_floatarg tempo){
    if(tempo < 0) // avoid negative tempo for now
        tempo = 0;
    if(tempo != 0){
        if(x->x_freeze){
            x->x_freeze = 0;
            if(!x->x_pause)
                metronome_float(x, 1);
        }
        tempo = 60000./tempo;
        int dir = (tempo > 0);
        if(tempo < 0)
            tempo *= -1;
        x->x_tempo = tempo;
        if(dir != x->x_dir){
            x->x_dir = dir;
            if(x->x_dir){
                x->x_tempocount+=2;
                if(x->x_tempocount >= x->x_n_tempo){
                    x->x_tempocount-= x->x_n_tempo;
                    x->x_barcount++;
                }
            }
            else{
                x->x_tempocount-=2;
                if(x->x_tempocount < 0){
                    x->x_tempocount += x->x_n_tempo;
                    x->x_barcount--;
                }
            }
        }
        clock_setunit(x->x_clock, x->x_tempo * x->x_tempo_div / x->x_ticks, 0);
    }
    else{
        clock_unset(x->x_clock);
        x->x_freeze = 1;
    }
}

static void metronome_free(t_metronome *x){
    clock_free(x->x_clock);
}

static void *metronome_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_metronome *x = (t_metronome *)pd_new(metronome_class);
    x->x_clock = clock_new(x, (t_method)metronome_tick);
    x->x_sig = NULL;
    x->x_dir = 1;
    x->x_ticks = DEFNTICKS;
    x->x_tickcount = 0;
    x->x_tempo_div = 1;
    x->x_group = 0;
    x->x_running = 0;
    x->x_sigchange = 0;
    x->x_n_tempo = 4;
    x->x_tempo_fig = 4;
    x->x_n_subdiv = 1;
    t_float tempo = 0;
    if(ac > 0){
        if(av->a_type == A_FLOAT){
            tempo = atom_getfloatarg(0, ac, av);
            ac--, av++;
        }
        else
            goto next;
    next:
        if(ac > 0){
            if(ac == 1){
                t_atom at[1];
                if(av->a_type == A_SYMBOL){
                    SETSYMBOL(at, atom_getsymbolarg(0, ac, av));
                    ac--, av++;
                    metronome_timesig(x, gensym("timesig"), 1, at);
                }
                else
                    goto errstate;
            }
            else{
                t_atom at[2];
                if(av->a_type == A_SYMBOL){
                    SETSYMBOL(at, atom_getsymbolarg(0, ac, av));
                    ac--, av++;
                    if(av->a_type == A_FLOAT){
                        SETFLOAT(at+1, atom_getfloatarg(0, ac, av));
                        ac--, av++;
                    }
                    else
                        goto errstate;
                    metronome_timesig(x, gensym("timesig"), 2, at);
                }
                else
                    goto errstate;
            }
        }
    }
    metronome_tempo(x, tempo);
    metronome_stop(x);
    outlet_new(&x->x_obj, gensym("bang"));
    x->x_barout = outlet_new((t_object *)x, gensym("float"));
    x->x_tempoout = outlet_new((t_object *)x, gensym("float"));
    x->x_subdivout = outlet_new((t_object *)x, gensym("float"));
    x->x_phaseout = outlet_new((t_object *)x, gensym("float"));
    x->x_tickout = outlet_new((t_object *)x, gensym("float"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("tempo"));
    return(x);
    errstate:
        pd_error(x, "[metronome]: improper args");
        return(NULL);
}

void metronome_setup(void){
    metronome_class = class_new(gensym("metronome"), (t_newmethod)metronome_new, (t_method)metronome_free, sizeof(t_metronome), 0, A_GIMME, 0);
    class_addfloat(metronome_class, (t_method)metronome_float);
    class_addbang(metronome_class, (t_method)metronome_start);
    class_addmethod(metronome_class, (t_method)metronome_timesig, gensym("timesig"), A_GIMME, 0);
    class_addmethod(metronome_class, (t_method)metronome_tempo, gensym("tempo"), A_FLOAT, 0);
//    class_addmethod(metronome_class, (t_method)metronome_pause, gensym("pause"), 0);
//    class_addmethod(metronome_class, (t_method)metronome_play, gensym("play"), 0);
    class_addmethod(metronome_class, (t_method)metronome_stop, gensym("stop"), 0);
    class_addmethod(metronome_class, (t_method)metronome_rewind, gensym("rewind"), 0);
    class_addmethod(metronome_class, (t_method)metronome_start, gensym("start"), 0);
}
