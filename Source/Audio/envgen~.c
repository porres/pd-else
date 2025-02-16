// porres 2018-2025

#include <m_pd.h>
#include "else_alloca.h"
#include <stdlib.h>
#include <math.h>

#define MAX_SEGS  256 // maximum line segments

typedef struct _envgen{
    t_object        x_obj;
    int             x_ac;
    int             x_ac_rel;
    int             x_pause;
    int             x_status;
    int             x_legato;
    int             x_suspoint;
    int             x_nchans;
    int             x_nblock;
    int             x_exp;
    float           x_power;
    t_float         x_retrigger;
    t_float        *x_value;
    t_float        *x_inc;
    t_float        *x_delta;
    t_float        *x_target;
    t_float        *x_last_target;
    t_float        *x_gain;
    t_float        *x_lastin;
    int            *x_running;
    int            *x_n;
    int            *x_nleft;
    int            *x_line_n;
    int            *x_n_lines;
    int            *x_line_idx;
    int            *x_release;
    t_float         x_current_target[MAX_SEGS];
    t_float         x_current_ms[MAX_SEGS];
    t_symbol       *x_ignore;
    t_atom         *x_av;
    t_atom         *x_av_rel;
    t_atom          x_at_exp[MAX_SEGS];
    t_clock        *x_clock;
    t_outlet       *x_out2;
}t_envgen;

static t_class *envgen_class;

static void copy_atoms(t_atom *src, t_atom *dst, int n){
    while(n--)
        *dst++ = *src++;
}

static float envgen_get_step(t_envgen *x, int j){
    float step = (float)(x->x_n[j] - x->x_nleft[j])/(float)x->x_n[j];
    if(fabs(x->x_power) != 1){ // EXPONENTIAL
        if(x->x_power >= 0){ // positive exponential
            if((x->x_delta[j] > 0) == (x->x_gain[j] > 0))
                step = pow(step, x->x_power);
            else
                step = 1-pow(1-step, x->x_power);
        }
        else{ // negative exponential
            if((x->x_delta[j] > 0) == (x->x_gain[j] > 0))
                step = 1-pow(1-step, -x->x_power);
            else
                step = pow(step, -x->x_power);
        }
    }
    return(step);
}

static void envgen_retarget(t_envgen *x, int skip, int j){
    int idx = x->x_line_idx[j]; // line index
    x->x_target[j] = x->x_current_target[idx];
    if(skip)
        x->x_power = 1;
    else{
        x->x_power = x->x_at_exp[x->x_line_n[j]].a_w.w_float;
        x->x_line_n[j]++;
    }
    x->x_nleft[j] = (int)(x->x_current_ms[idx] * sys_getsr()*0.001 + 0.5);
    x->x_n_lines[j]--;  // Decrease the number of remaining line segments
    x->x_line_idx[j]++; // Move to the next segment in the current channel
    idx = x->x_line_idx[j]; // update idx
    if(x->x_nleft[j] == 0){ // stupid line's gonna be ignored
        x->x_value[j] = x->x_last_target[j] = x->x_target[j];
        x->x_delta[j] = x->x_inc[j] = 0;
        x->x_power = x->x_at_exp[x->x_line_n[j]].a_w.w_float;
        while(x->x_n_lines[j] > 0 &&
              !(int)(x->x_current_ms[idx] * sys_getsr() * 0.001 + 0.5)) {
            x->x_value[j] = x->x_target[j] = x->x_current_target[idx];
            x->x_n_lines[j]--;               // Decrease the number of remaining line segments
            x->x_line_idx[j]++;              // Move to the next segment in the current channel
            idx = x->x_line_idx[j];          // Update idx to the next segment
            x->x_line_n[j]++;
            x->x_power = x->x_at_exp[x->x_line_n[j]].a_w.w_float;
        }
    }
    else{
        x->x_delta[j] = (x->x_target[j] - (x->x_last_target[j] = x->x_value[j]));
        x->x_n[j] = x->x_nleft[j];
        x->x_nleft[j]--; // update it already
        x->x_inc[j] = x->x_delta[j] != 0 ? envgen_get_step(x, j) * x->x_delta[j] : 0;
    }
}

static void envgen_attack(t_envgen *x, int ac, t_atom *av, int j){
    x->x_line_n[j] = 0;
    int odd = ac % 2;
    int n_lines = ac/2;
    int skip1st = 0;
    if(odd){ // non legato mode, restart from first value
        if(x->x_legato) // actually legato
            av++, ac--; // skip 1st atom
        else{ // ok, non legato really
            n_lines++; // add extra segment
            skip1st = 1; // for release
        }
    }
    if(n_lines > MAX_SEGS)
        n_lines = MAX_SEGS, odd = 0;
    x->x_release[j] = 0; // init to no release
    if(x->x_suspoint){ // find sustain point
        if((n_lines - skip1st) >= x->x_suspoint){ // we have a release!
            x->x_release[j] = 1;
            n_lines = x->x_suspoint; // limit n_lines to suspoint
            int n = 2*n_lines + skip1st;
            x->x_ac_rel = (ac -= n); // release n
            x->x_av_rel = getbytes(x->x_ac_rel*sizeof(*(x->x_av_rel)));
            copy_atoms(av+n, x->x_av_rel, x->x_ac_rel); // copy release ramp
        }
    }
// attack
    x->x_n_lines[j] = n_lines; // define number of line segments
    int idx = 0;
    if(odd && !x->x_legato){ // initialize 1st segment
        x->x_current_ms[idx] = x->x_running[j] ? x->x_retrigger : 0;
        x->x_current_target[idx] = (av++)->a_w.w_float * x->x_gain[j];
        idx++; // Move to the next index
        n_lines--; // Decrease the number of remaining segments
    }
    while(n_lines--){
        x->x_current_ms[idx] = (av++)->a_w.w_float; // Set ms for the current segment
        if(x->x_current_ms[idx] < 0)
            x->x_current_ms[idx] = 0; // Ensure ms is not negative
        x->x_current_target[idx] = (av++)->a_w.w_float * x->x_gain[j];
        idx++; // Move to the next index
    }
    x->x_line_idx[j] = 0;
    envgen_retarget(x, skip1st, j);
    if(x->x_pause)
        x->x_pause = 0;
    x->x_running[j] = 1;
    if(!x->x_status) // turn on global status
        outlet_float(x->x_out2, x->x_status = 1);
}

static void envgen_release(t_envgen *x, int ac, t_atom *av, int j){
    if(ac < 2)
        return;
    int n_lines = ac/2;
    x->x_n_lines[j] = n_lines; // define number of line segments
    int idx = 0; // Initialize the index
    while(n_lines--){
        x->x_current_ms[idx] = av++->a_w.w_float;
        if(x->x_current_ms[idx] < 0)
            x->x_current_ms[idx] = 0;
        x->x_current_target[idx] = av++->a_w.w_float * x->x_gain[j];
        idx++; // Update the index for the next segment
    }
    x->x_target[j] = x->x_current_target[x->x_line_idx[j] = 0];
    x->x_release[j] = 0;
    envgen_retarget(x, 0, j);
    if(x->x_pause)
        x->x_pause = 0;
}

static void envgen_tick(t_envgen *x){
//    post("tick x->x_status = %d / !x->x_release = %d", x->x_status, !x->x_release);
    int done = 1;
    for(int i = 0; i < x->x_nchans; i++){
        if(x->x_running[i]){
            done = 0;
            break;
        }
    }
    if(done && x->x_status) // turn off global status
        outlet_float(x->x_out2, x->x_status = 0);
}

static void envgen_init(t_envgen *x, int ac, t_atom *av){
    int i;
    for(i = 0; i < ac; i++)
        if((av+i)->a_type != A_FLOAT){
            pd_error(x, "[envgen~]: list needs to only contain floats");
            return;
        }
    if(!x->x_exp)
        copy_atoms(av, x->x_av, x->x_ac = ac);
    else{
        if((ac % 3) == 2){
            pd_error(x, "[envgen~]: wrong number of elements for 'exp' message");
            return;
        }
        int exp = (ac % 3) + 1;
        int nlines = (int)(ac/3);
        t_atom* temp_at = ALLOCA(t_atom, ac-nlines);
        int j = 0, k = 0;
        for(i = 0; i < ac; i++){
            if(i % 3 == exp){
                SETFLOAT(x->x_at_exp+j, (av+i)->a_w.w_float);
                j++;
            }
            else{
                SETFLOAT(temp_at+k, (av+i)->a_w.w_float);
                k++;
            }
        }
        x->x_ac = k;
        copy_atoms(temp_at, x->x_av, x->x_ac);
        FREEA(temp_at, t_atom, (ac-nlines));
    }
}

static void envgen_expi(t_envgen *x, t_floatarg f1, t_floatarg f2){
    int i = f1 < 0 ? 0 : (int)f1;
    SETFLOAT(x->x_at_exp+i, f2);
}

static void envgen_expl(t_envgen *x, t_symbol *s, int ac, t_atom *av){
    if(!ac)
        return;
    x->x_ignore = s;
    for(int i = 0; i < ac; i++)
        SETFLOAT(x->x_at_exp+i, (av+i)->a_w.w_float);
}

static void envgen_exp(t_envgen *x, t_symbol *s, int ac, t_atom *av){
    if(ac < 3)
        return;
    x->x_ignore  = s;
    x->x_exp = 1;
    envgen_init(x, ac, av);
}

static void envgen_bang(t_envgen *x){
    envgen_attack(x, x->x_ac, x->x_av, 0);
}

static void envgen_float(t_envgen *x, t_floatarg f){
    if(f != 0){
        x->x_gain[0] = f;
        envgen_attack(x, x->x_ac, x->x_av, 0);
    }
    else{
        if(x->x_release[0])
            envgen_release(x, x->x_ac_rel, x->x_av_rel, 0);
    }
}

static void envgen_list(t_envgen *x,t_symbol* s, int ac, t_atom* av){
    x->x_ignore = s;
    if(!ac){
        envgen_bang(x);
        return;
    }
    if(ac == 1){
        if(av->a_type == A_FLOAT)
            envgen_float(x, atom_getfloat(av));
        return;
    }
    x->x_exp = 0;
    envgen_init(x, ac, av);
}

static void envgen_rel(t_envgen *x){
    if(x->x_release[0])
        envgen_release(x, x->x_ac_rel, x->x_av_rel, 0);
}

static void envgen_setgain(t_envgen *x, t_floatarg f){
    if(f != 0)
        x->x_gain[0] = f;
}

static void envgen_retrigger(t_envgen *x, t_floatarg f){
    x->x_retrigger = f < 0 ? 0 : f;
}

static void envgen_legato(t_envgen *x, t_floatarg f){
    x->x_legato = f != 0;
}

static void envgen_suspoint(t_envgen *x, t_floatarg f){
    x->x_suspoint = f < 0 ? 0 : (int)f;
}

static t_int *envgen_perform(t_int *w){
    t_envgen *x = (t_envgen *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    int ch2 = (int)(w[5]);
    int n = x->x_nblock, chs = x->x_nchans;
    float *lastin = x->x_lastin;
    for(int j = 0; j < chs; j++){
        for(int i = 0; i < n; i++){
            t_float f = in1[j*n + i];
            t_float retrig = ch2 == 1 ? in2[i] : in2[j*n + i];
            if(f != 0 && lastin[j] == 0){ // set attack ramp
                x->x_gain[j] = f;
                envgen_attack(x, x->x_ac, x->x_av, j);
            }
            else if(x->x_release[j] && f == 0 && lastin[j] != 0) // set release ramp
                envgen_release(x, x->x_ac_rel, x->x_av_rel, j);
            if(f != 0 && lastin[j] != 0){ // gate on
                if(retrig != 0){ // retrigger
                    x->x_gain[j] = f;
                    envgen_attack(x, x->x_ac, x->x_av, j);
                }
            }
            if(PD_BIGORSMALL(x->x_value[j]))
                x->x_value[j] = 0;
            out[j*n + i] = x->x_value[j] = x->x_last_target[j] + x->x_inc[j];
            if(!x->x_pause && x->x_running[j]){ // not paused and running
                if(x->x_nleft[j] > 0){ // decrease
                    x->x_nleft[j]--;
                    x->x_inc[j] = x->x_delta[j] != 0 ? envgen_get_step(x, j) * x->x_delta[j] : 0;
                }
                else if(x->x_nleft[j] == 0){ // reached target, update!
                    x->x_last_target[j] = x->x_target[j];
                    x->x_inc[j] = 0;
                    if(x->x_n_lines[j] > 0) // there's more, retarget to the next
                        envgen_retarget(x, 0, j);
                    else if(!x->x_release[j]){ // there's no release, we're done.
                        x->x_running[j] = 0;
                        clock_delay(x->x_clock, 0);
                    }
                }
            }
            lastin[j] = f;
        }
    }
    x->x_lastin = lastin;
    return(w+6);
}

static void envgen_dsp(t_envgen *x, t_signal **sp){
    x->x_nblock = sp[0]->s_n;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans;
    if(x->x_nchans != chs){
        x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_gain = (t_float *)resizebytes(x->x_gain,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_value = (t_float *)resizebytes(x->x_value,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_delta = (t_float *)resizebytes(x->x_delta,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_target = (t_float *)resizebytes(x->x_target,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_last_target = (t_float *)resizebytes(x->x_last_target,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_inc = (t_float *)resizebytes(x->x_inc,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_line_n = (int *)resizebytes(x->x_line_n,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_nleft = (int *)resizebytes(x->x_nleft,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_n = (int *)resizebytes(x->x_n,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_n_lines = (int *)resizebytes(x->x_n_lines,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_line_idx = (int *)resizebytes(x->x_line_idx,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_running = (int *)resizebytes(x->x_running,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_release = (int *)resizebytes(x->x_release,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[2], chs);
    if((ch2 > 1 && ch2 != chs)){
        dsp_add_zero(sp[2]->s_vec, chs*x->x_nblock);
        pd_error(x, "[envgen~]: channel sizes mismatch");
        return;
    }
    dsp_add(envgen_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, ch2);
}

static void envgen_free(t_envgen *x){
    if(x->x_clock)
        clock_free(x->x_clock);
    freebytes(x->x_lastin, x->x_nchans * sizeof(*x->x_lastin));
    freebytes(x->x_gain, x->x_nchans * sizeof(*x->x_gain));
    freebytes(x->x_value, x->x_nchans * sizeof(*x->x_value));
    freebytes(x->x_delta, x->x_nchans * sizeof(*x->x_delta));
    freebytes(x->x_target, x->x_nchans * sizeof(*x->x_target));
    freebytes(x->x_last_target, x->x_nchans * sizeof(*x->x_last_target));
    freebytes(x->x_inc, x->x_nchans * sizeof(*x->x_inc));
    freebytes(x->x_line_n, x->x_nchans * sizeof(*x->x_line_n));
    freebytes(x->x_nleft, x->x_nchans * sizeof(*x->x_nleft));
    freebytes(x->x_n, x->x_nchans * sizeof(*x->x_n));
    freebytes(x->x_n_lines, x->x_nchans * sizeof(*x->x_n_lines));
    freebytes(x->x_line_idx, x->x_nchans * sizeof(*x->x_line_idx));
    freebytes(x->x_running, x->x_nchans * sizeof(*x->x_running));
}

static void envgen_pause(t_envgen *x){
    x->x_pause = 1;
}

static void envgen_resume(t_envgen *x){
    x->x_pause = 0;
}

static void *envgen_new(t_symbol *s, int ac, t_atom *av){
    t_symbol *cursym = s; // avoid warning
    t_envgen *x = (t_envgen *)pd_new(envgen_class);
    x->x_power = 1;
    x->x_nchans = 1;
    x->x_gain = (t_float *)getbytes(sizeof(*x->x_gain));
    x->x_lastin = (t_float *)getbytes(sizeof(*x->x_lastin));
    x->x_value = (t_float *)getbytes(sizeof(*x->x_value));
    x->x_delta = (t_float *)getbytes(sizeof(*x->x_delta));
    x->x_target = (t_float *)getbytes(sizeof(*x->x_target));
    x->x_last_target = (t_float *)getbytes(sizeof(*x->x_last_target));
    x->x_inc = (t_float *)getbytes(sizeof(*x->x_inc));
    x->x_line_n = (int *)getbytes(sizeof(*x->x_line_n));
    x->x_n_lines = (int *)getbytes(sizeof(*x->x_n_lines));
    x->x_line_idx = (int *)getbytes(sizeof(*x->x_line_idx));
    x->x_running = (int *)getbytes(sizeof(*x->x_running));
    x->x_nleft = (int *)getbytes(sizeof(*x->x_nleft));
    x->x_n = (int *)getbytes(sizeof(*x->x_n));
    x->x_release = (int *)getbytes(sizeof(*x->x_release));
    x->x_lastin[0] = x->x_value[0] = x->x_delta[0] = 0.;
    x->x_gain[0] = 1;
    x->x_target[0] = x->x_last_target[0] = x->x_inc[0] = 0.;
    x->x_n[0] = x->x_line_n[0] = x->x_nleft[0] = x->x_release[0] = 0;
    x->x_n_lines[0] = x->x_line_idx[0] = x->x_running[0] = 0;
    x->x_retrigger = 0;
    x->x_pause = 0;
    x->x_suspoint = x->x_legato = 0;
    int i = 0;
    x->x_exp = 0;
    for(i = 0; i < MAX_SEGS; i++) // set exponential list to linear
        SETFLOAT(x->x_at_exp+i, 1);
    t_atom at[2];
    SETFLOAT(at, 0);
    SETFLOAT(at+1, 0);
    x->x_av = getbytes(MAX_SEGS*sizeof(*(x->x_av)));
    copy_atoms(at, x->x_av, x->x_ac = 2);
    while(ac > 0){
        if(av->a_type == A_FLOAT)
            break;
        else if(av->a_type == A_SYMBOL){
            cursym = atom_getsymbolarg(0, ac, av);
            if(cursym == gensym("-init")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_value[0] = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(cursym == gensym("-retrigger")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_retrigger = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(cursym == gensym("-exp")){
                if(ac >= 5 && (av+1)->a_type == A_FLOAT){
                    ac--, av++;
                    int z = 0;
                    while(((ac-z)>0) && ((av+z)->a_type == A_FLOAT))
                        z++;
                    if((z % 3) == 2)
                        goto errstate;
                    int exp = (z % 3) + 1;
                    int nlines = (int)(z/3);
                    t_atom* temp_at = ALLOCA(t_atom, z-nlines);
                    int j = 0, k = 0;
                    for(i = 0; i < ac; i++){
                        if(i % 3 == exp){
                            SETFLOAT(x->x_at_exp+j, (av+i)->a_w.w_float);
                            j++;
                        }
                        else{
                            SETFLOAT(temp_at+k, (av+i)->a_w.w_float);
                            k++;
                        }
                    }
                    x->x_exp = 1;
                    copy_atoms(temp_at, x->x_av, x->x_ac = k);
                    FREEA(temp_at, t_atom, (z-nlines));
                    ac-=z, av+=z;
                }
                else
                    goto errstate;
            }
            else if(cursym == gensym("-suspoint")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    int suspoint = (int)atom_getfloatarg(1, ac, av);
                    x->x_suspoint = suspoint < 0 ? 0 : suspoint;
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(cursym == gensym("-legato"))
                x->x_legato = 1, ac--, av++;
            else
                goto errstate;
        }
    }
    if(ac){
        if(ac == 1)
            x->x_value[0] = atom_getfloatarg(0, 1, av);
        else{
            if(!x->x_exp)
                copy_atoms(av, x->x_av, x->x_ac = ac);
            else
                goto errstate;
        }
    }
    x->x_last_target[0] = x->x_value[0];
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    x->x_out2 = outlet_new((t_object *)x, &s_float);
    x->x_clock = clock_new(x, (t_method)envgen_tick);
    return(x);
errstate:
    pd_error(x, "[envgen~]: improper args");
    return(NULL);
}

void envgen_tilde_setup(void){
    envgen_class = class_new(gensym("envgen~"), (t_newmethod)envgen_new,
        (t_method)envgen_free, sizeof(t_envgen), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(envgen_class, nullfn, gensym("signal"), 0);
    class_addmethod(envgen_class, (t_method)envgen_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(envgen_class, envgen_list);
    class_addmethod(envgen_class, (t_method)envgen_setgain, gensym("setgain"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_exp, gensym("exp"), A_GIMME, 0);
    class_addmethod(envgen_class, (t_method)envgen_expl, gensym("expl"), A_GIMME, 0);
    class_addmethod(envgen_class, (t_method)envgen_expi, gensym("expi"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_bang, gensym("attack"), 0);
    class_addmethod(envgen_class, (t_method)envgen_rel, gensym("release"), 0);
    class_addmethod(envgen_class, (t_method)envgen_pause, gensym("pause"), 0);
    class_addmethod(envgen_class, (t_method)envgen_resume, gensym("resume"), 0);
    class_addmethod(envgen_class, (t_method)envgen_legato, gensym("legato"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_suspoint, gensym("suspoint"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_retrigger, gensym("retrigger"), A_FLOAT, 0);
}
