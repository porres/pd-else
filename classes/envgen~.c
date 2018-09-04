// porres 2018

#include "m_pd.h"
#include <string.h>

#define ENVGEN_MAX_SIZE  512 // maximum line segments

typedef struct _envgenseg{
    float  s_target;
    float  s_delta;
}t_envgen_line;

typedef struct _envgen{
    t_object        x_obj;
    float           x_value;
    float           x_target;
    float           x_delta;
    float           x_inc;
    float           x_ksr;
    int             x_ac;
    t_atom         *x_av;
    int             x_ac_rel;
    t_atom         *x_av_rel;
    int             x_nleft;
    int             x_n_lines;
    int             x_pause;
    int             x_status;
    int             x_legato;
    int             x_release;
    int             x_suspoint;
    t_float         x_lastin;
    t_float         x_maxsustain;
    t_float         x_retrigger;
    t_float         x_gain;
    t_clock        *x_clock;
    t_clock        *x_sus_clock;
    t_outlet       *x_out2;
    t_envgen_line  *x_curseg;
    t_envgen_line  *x_lines;
    t_envgen_line   x_n_lineini[ENVGEN_MAX_SIZE];
}t_envgen;

static t_class *envgen_class;

static void copy_atoms(t_atom *src, t_atom *dst, int n){
    while(n--)
        *dst++ = *src++;
}

static void envgen_retarget(t_envgen *x){
    x->x_target = x->x_curseg->s_target;
    x->x_nleft = (t_int)(x->x_curseg->s_delta * x->x_ksr + 0.5);
    x->x_n_lines--;
    x->x_curseg++;
    if(x->x_nleft == 0){
        x->x_value = x->x_target;
        x->x_inc = 0;
        while(x->x_n_lines && // next delta also == 0 && lines
              !(t_int)(x->x_curseg->s_delta * x->x_ksr + 0.5)){
            x->x_value = x->x_target = x->x_curseg->s_target;
            x->x_n_lines--, x->x_curseg++;
        }
    }
    else{
        x->x_value += (x->x_inc = (x->x_target - x->x_value) / (t_float)x->x_nleft);
        x->x_nleft--;
    }
}

static void envgen_attack(t_envgen *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    t_int odd = ac % 2;
    t_int n_lines = ac / 2;
    t_int skip1st = 0;
    if(odd){
        if(x->x_legato){
            av++; // we have skipped the 1st atom
            ac--;
        }
        else{
            n_lines++; // add an extra segment
            skip1st = 1; // for release
        }
    }
    if(n_lines > ENVGEN_MAX_SIZE){
        n_lines = ENVGEN_MAX_SIZE;
        odd = 0;
    };
    if(x->x_suspoint){ // we have a sustain point
        if(n_lines - skip1st >= x->x_suspoint){ // limit # segs to suspoint and define release
            n_lines = x->x_suspoint;
            x->x_release = 1; // we have a release ramp!
            int n = 2*n_lines + skip1st;
            x->x_ac_rel = (ac -= n);
            x->x_av_rel = getbytes(x->x_ac_rel * sizeof(*(x->x_av_rel)));
            copy_atoms(av+n, x->x_av_rel, x->x_ac_rel);
            if(x->x_maxsustain > 0)
                clock_delay(x->x_sus_clock, x->x_maxsustain);
        }
        else
            x->x_release = 0;
    }
    else
        x->x_release = 0;
// attack
    x->x_n_lines = n_lines; // define number of line segments
    t_envgen_line *line_point = x->x_lines;
    if(odd && !x->x_legato){ // initialize 1st segment
        line_point->s_delta = x->x_status ? x->x_retrigger : 0;
        line_point->s_target = (av++)->a_w.w_float * x->x_gain;
        line_point++;
        n_lines--;
    }
    while(n_lines--){
        line_point->s_delta = (av++)->a_w.w_float;
        if(line_point->s_delta < 0)
            line_point->s_delta = 0;
        line_point->s_target = (av++)->a_w.w_float * x->x_gain;
        line_point++;
    }
    x->x_curseg = x->x_lines;
    envgen_retarget(x);
    if(x->x_pause)
        x->x_pause = 0;
    if(!x->x_status)
        outlet_float(x->x_out2, x->x_status = 1); // turn on status
}

static void envgen_release(t_envgen *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    if(ac < 2)
        return;
    t_int n_lines = ac / 2;
// release
    x->x_n_lines = n_lines; // define number of line segments
    t_envgen_line *line_point = x->x_lines;
    while(n_lines--){
        line_point->s_delta = av++->a_w.w_float;
        if(line_point->s_delta < 0)
            line_point->s_delta = 0;
        line_point->s_target = av++->a_w.w_float * x->x_gain;
        line_point++;
    }
    x->x_target = x->x_lines->s_target;
    x->x_curseg = x->x_lines;
    x->x_release = 0;
    envgen_retarget(x);
    if(x->x_pause)
        x->x_pause = 0;
}

static void envgen_tick(t_envgen *x){
    if(x->x_status && !x->x_release)
        outlet_float(x->x_out2, x->x_status = 0);
}

static void envgen_sus_tick(t_envgen *x){
    if(x->x_release)
        envgen_release(x, &s_list, x->x_ac_rel, x->x_av_rel);
}

static void envgen_set(t_envgen *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    t_atom *a;
    t_int n;
    for(n = 0, a = av; n < ac; n++, a++)
        if(a->a_type != A_FLOAT){
            pd_error(x, "envgen~: set needs to only contain floats");
            return;
        }
    if(!ac)
        return;
    x->x_ac = ac;
    x->x_av = getbytes(x->x_ac * sizeof(*(x->x_av)));
    copy_atoms(av, x->x_av, x->x_ac);
}

static void envgen_list(t_envgen *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    t_atom *a;
    t_int n;
    for(n = 0, a = av; n < ac; n++, a++)
        if(a->a_type != A_FLOAT){
            pd_error(x, "envgen~: list needs to only contain floats");
            return;
        }
    if(!ac)
        return;
    x->x_ac = ac;
    x->x_av = getbytes(x->x_ac * sizeof(*(x->x_av)));
    copy_atoms(av, x->x_av, x->x_ac);
    envgen_attack(x, &s_list, x->x_ac, x->x_av);
}

static void envgen_bang(t_envgen *x){
    envgen_attack(x, &s_list, x->x_ac, x->x_av);
}

static void envgen_rel(t_envgen *x){
    if(x->x_release)
        envgen_release(x, &s_list, x->x_ac_rel, x->x_av_rel);
}

static void envgen_float(t_envgen *x, t_float f){
    if(f != 0){
        x->x_gain = f;
        envgen_attack(x, &s_list, x->x_ac, x->x_av);
    }
    else{
        if(x->x_release)
            envgen_release(x, &s_list, x->x_ac_rel, x->x_av_rel);
    }
}

static void envgen_setgain(t_envgen *x, t_float f){
    if(f != 0)
        x->x_gain = f;
}

static void envgen_retrigger(t_envgen *x, t_float f){
    x->x_retrigger = f < 0 ? 0 : f;
}

static void envgen_legato(t_envgen *x, t_float f){
    x->x_legato = f != 0;
}

static void envgen_maxsustain(t_envgen *x, t_float f){
    x->x_maxsustain = f;
}

static void envgen_suspoint(t_envgen *x, t_float f){
    t_int suspoint = f < 0 ? 0 : (int)f;
    x->x_suspoint = suspoint;
}

static t_int *envgen_perform(t_int *w){
    t_envgen *x = (t_envgen *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    t_int n = (int)(w[4]);
    float lastin = x->x_lastin;
    while(n--){
        t_float f = *in++;
        if(f != 0 && lastin == 0){
            x->x_gain = f;
            envgen_attack(x, &s_list, x->x_ac, x->x_av);
        }
        else if(x->x_release && f == 0 && lastin != 0)
            envgen_release(x, &s_list, x->x_ac_rel, x->x_av_rel);
        if(PD_BIGORSMALL(x->x_value))
            x->x_value = 0;
        *out++ = x->x_value;
        if(!x->x_pause && x->x_status){ // update
            if(x->x_nleft > 0){ // inc
                x->x_value += x->x_inc;
                x->x_nleft--;
            }
            else if(x->x_nleft == 0){ // target
                x->x_value = x->x_target;
                if(x->x_n_lines > 0){ // next
//                    while(x->x_nleft == 0){
                        envgen_retarget(x);
//                    }
                }
                else if(!x->x_release) // off
                    clock_delay(x->x_clock, 0); // over!!!!!
            }
        }
        lastin = f;
    }
    x->x_lastin = lastin;
    return(w + 5);
}

static void envgen_dsp(t_envgen *x, t_signal **sp){
    x->x_ksr = sp[0]->s_sr * 0.001;
    dsp_add(envgen_perform, 4, x, sp[0]->s_vec, sp[0]->s_vec, sp[0]->s_n);
}

static void envgen_free(t_envgen *x){
    if(x->x_lines != x->x_n_lineini)
        freebytes(x->x_lines, ENVGEN_MAX_SIZE * sizeof(*x->x_lines));
    if(x->x_clock)
        clock_free(x->x_clock);
    if(x->x_sus_clock)
        clock_free(x->x_sus_clock);
}

static void envgen_pause(t_envgen *x){
    x->x_pause = 1;
}

static void envgen_resume(t_envgen *x){
    x->x_pause = 0;
}

static void *envgen_new(t_symbol *s, int ac, t_atom *av){
    t_symbol *cursym = s;
    t_envgen *x = (t_envgen *)pd_new(envgen_class);
    x->x_gain = 1;
    x->x_nleft = 0;
    x->x_n_lines = 0;
    x->x_pause = 0;
    x->x_lines = x->x_n_lineini;
    x->x_curseg = 0;
    x->x_release = 0;
    x->x_lastin = 0;
// init
    x->x_maxsustain = 0;
    x->x_legato = 0;
    x->x_retrigger = 0;
    x->x_suspoint = 0;
    t_atom at[2];
    SETFLOAT(at, 0);
    SETFLOAT(at+1, 0);
    x->x_ac = 2;
    x->x_av = getbytes(2 * sizeof(*(x->x_av)));
    copy_atoms(at, x->x_av, x->x_ac);
/////////////////////////////////////////////////////////////////////////////////////
    int symarg = 0;
    int n = 0;
    while(ac > 0){
        if((av+n)->a_type == A_FLOAT && !symarg){
            n++;
            ac--;
        }
        else if((av+n)->a_type == A_SYMBOL){
            if(!symarg){
                symarg = 1;
                if(n == 1 && av->a_type == A_FLOAT){
                    x->x_value = atom_getfloatarg(0, ac, av);
                }
                else if(n > 1){
                    x->x_ac = n;
                    x->x_av = getbytes(n * sizeof(*(x->x_av)));
                    copy_atoms(av, x->x_av, x->x_ac);
                }
                av += n;
                n = 0;
            }
            cursym = atom_getsymbolarg(0, ac, av);
            if(!strcmp(cursym->s_name, "-init")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_value = atom_getfloatarg(1, ac, av);
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-retrigger")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_retrigger = atom_getfloatarg(1, ac, av);
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-suspoint")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    t_int suspoint = (t_int)atom_getfloatarg(1, ac, av);
                    x->x_suspoint = suspoint < 0 ? 0 : suspoint;
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-maxsustain")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_maxsustain = (t_int)atom_getfloatarg(1, ac, av);
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-legato")){
                x->x_legato = 1;
                ac--;
                av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    if(!symarg && n > 0){
        if(n == 1)
            x->x_value = atom_getfloatarg(0, n, av);
        else{
            x->x_ac = n;
            x->x_av = getbytes(n * sizeof(*(x->x_av)));
            copy_atoms(av, x->x_av, x->x_ac);
        }
    }
/////////////////////////////////////////////////////////////////////////////////////
    outlet_new((t_object *)x, &s_signal);
    x->x_out2 = outlet_new((t_object *)x, &s_float);
    x->x_clock = clock_new(x, (t_method)envgen_tick);
    x->x_sus_clock = clock_new(x, (t_method)envgen_sus_tick);
    return(x);
errstate:
    pd_error(x, "[envgen~]: improper args");
    return NULL;
}

void envgen_tilde_setup(void){
    envgen_class = class_new(gensym("envgen~"), (t_newmethod)envgen_new,
            (t_method)envgen_free, sizeof(t_envgen), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(envgen_class, nullfn, gensym("signal"), 0);
    class_addmethod(envgen_class, (t_method)envgen_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(envgen_class, envgen_float);
    class_addlist(envgen_class, envgen_list);
    class_addbang(envgen_class, envgen_bang);
    class_addmethod(envgen_class, (t_method)envgen_setgain, gensym("setgain"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_set, gensym("set"), A_GIMME, 0);
    class_addmethod(envgen_class, (t_method)envgen_bang, gensym("attack"), 0);
    class_addmethod(envgen_class, (t_method)envgen_rel, gensym("release"), 0);
    class_addmethod(envgen_class, (t_method)envgen_pause, gensym("pause"), 0);
    class_addmethod(envgen_class, (t_method)envgen_resume, gensym("resume"), 0);
    class_addmethod(envgen_class, (t_method)envgen_legato, gensym("legato"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_suspoint, gensym("suspoint"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_maxsustain, gensym("maxsustain"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_retrigger, gensym("retrigger"), A_FLOAT, 0);
}
