// porres 2018-2025

#include <m_pd.h>
#include "else_alloca.h"
#include <stdlib.h>
#include <math.h>

#define MAX_SEGS  1024 // maximum line segments

enum { LINEAR, CURVE, POWER, LAG, SINE, HANN };

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
    int             x_samps;
    int             x_lintype;
    float           x_curve; // exponential power or curve parameter
    t_float         x_retrigger;
    t_float        *x_value;
    t_float        *x_inc;
    t_float        *x_a;
    t_float        *x_b;
    t_float        *x_delta;
    t_float        *x_target;
    t_float        *x_last_target;
    t_float        *x_gain;
    t_float        *x_lastin;
    int            *x_running;
    int            *x_n;
    int            *x_nleft;
    int            *x_line_idx;
    int            *x_release;
    int            *x_n_lines;  // number of lines
    int            *x_exp_idx;
    int             x_single_exp;
    t_float         x_current_target[MAX_SEGS];
    t_float         x_current_time[MAX_SEGS];
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
    float step = x->x_inc[j]/(float)x->x_n[j];
    if(x->x_curve > 0){ // positive power exponential
        if((x->x_delta[j] > 0) == (x->x_gain[j] > 0))
            step = pow(step, x->x_curve);
        else
            step = 1-pow(1-step, x->x_curve);
    }
    else{ // negative
        if((x->x_delta[j] > 0) == (x->x_gain[j] > 0))
            step = 1-pow(1-step, -x->x_curve);
        else
            step = pow(step, -x->x_curve);
    }
    x->x_inc[j]++;
    return(step * x->x_delta[j]);
}

static int envgen_get_nleft(t_envgen *x, float time){
    if(x->x_samps)
        return((int)(time));
    else
        return((int)(time * sys_getsr()*0.001 + 0.5));
}

static void envgen_set_line_type(t_envgen *x, int j){
    int n = x->x_single_exp ? 0 : x->x_exp_idx[j];
    x->x_exp_idx[j]++;
    if(x->x_at_exp[n].a_type == A_FLOAT){
        x->x_curve = x->x_at_exp[n].a_w.w_float;
        x->x_lintype = fabs(x->x_curve) > 0.001 ? CURVE : LINEAR;
    }
    else{
        char p[MAXPDSTRING];
        t_symbol *sym = x->x_at_exp[n].a_w.w_symbol;
        if(sym == gensym("lin")){
            x->x_lintype = LINEAR;
            return;
        }
        sprintf(p, "%s", sym->s_name);
        if(p[0] == '^'){
            x->x_curve = atof(p + 1); // convert after '^'
            x->x_lintype = POWER;
        }
        else{
            post("[envgen~]: unknown curve type, setting to linear");
            x->x_lintype = LINEAR;
            return;
        }
        if(x->x_curve == 0 || fabs(x->x_curve) == 1)
            x->x_lintype = LINEAR;
    }
}

static void envgen_retarget(t_envgen *x, int j){
    int idx = x->x_line_idx[j]; // current segment index
    x->x_target[j] = x->x_current_target[idx];
    x->x_nleft[j] = envgen_get_nleft(x, x->x_current_time[idx]);
    while(x->x_n_lines[j] > 0 && x->x_nleft[j] == 0){ // Skip 0-length lines
        x->x_value[j] = x->x_last_target[j] = x->x_current_target[idx];
        x->x_n_lines[j]--;
        x->x_line_idx[j]++;
        idx = x->x_line_idx[j];
    
        if(x->x_n_lines[j] == 0)
            return;
    
        x->x_target[j] = x->x_current_target[idx];
        x->x_delta[j] = x->x_target[j] - x->x_last_target[j];
        x->x_nleft[j] = envgen_get_nleft(x, x->x_current_time[idx]);
        x->x_n[j] = x->x_nleft[j];
        x->x_inc[j] = x->x_delta[j] != 0 ? x->x_delta[j] / x->x_n[j] : 0;

        x->x_b[j] = x->x_delta[j] / (1 - exp(x->x_curve));
        x->x_a[j] = x->x_value[j] + x->x_b[j];
    }
    if(x->x_nleft[j] == 0){
//        post("exit");
        return; // exit if after skipping 0-length segments we have nothing left
    }
    
    // Proceed with normal ramp setup
    
    envgen_set_line_type(x, j);
    
    x->x_n_lines[j]--;
    x->x_line_idx[j]++;
    idx = x->x_line_idx[j];

    x->x_delta[j] = x->x_target[j] - (x->x_last_target[j] = x->x_value[j]);
    x->x_n[j] = x->x_nleft[j];
    
    if(x->x_delta[j] == 0)
        x->x_inc[j] = 0;
    else if(x->x_lintype == LINEAR)
        x->x_inc[j] = x->x_delta[j] / x->x_n[j];
    else if(x->x_lintype == CURVE){ // CURVE
        x->x_b[j] = x->x_delta[j] / (1 - exp(x->x_curve));
        x->x_a[j] = x->x_value[j] + x->x_b[j];
        x->x_inc[j] = exp(x->x_curve / x->x_n[j]);
    }
    else // POWER
        x->x_inc[j] = 1; // sample count
}

static void envgen_attack(t_envgen *x, int ac, t_atom *av, int j){
    x->x_exp_idx[j] = 0;
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
        x->x_current_time[idx] = x->x_running[j] ? x->x_retrigger : 0;
        x->x_current_target[idx] = (av++)->a_w.w_float * x->x_gain[j];
        idx++; // Move to the next index
        n_lines--; // Decrease the number of remaining segments
    }
    while(n_lines--){
        x->x_current_time[idx] = (av++)->a_w.w_float; // Set ms for the current segment
        if(x->x_current_time[idx] < 0)
            x->x_current_time[idx] = 0; // Ensure ms is not negative
        x->x_current_target[idx] = (av++)->a_w.w_float * x->x_gain[j];
        idx++; // Move to the next index
    }
    x->x_line_idx[j] = 0;
    envgen_retarget(x, j);
    x->x_nleft[j]++; // add a start point
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
        x->x_current_time[idx] = av++->a_w.w_float;
        if(x->x_current_time[idx] < 0)
            x->x_current_time[idx] = 0;
        x->x_current_target[idx] = av++->a_w.w_float * x->x_gain[j];
        idx++; // Update the index for the next segment
    }
    x->x_target[j] = x->x_current_target[x->x_line_idx[j] = 0];
    x->x_release[j] = 0;
    envgen_retarget(x, j);
    x->x_nleft[j]++; // add a start point
    if(x->x_pause)
        x->x_pause = 0;
}

static void envgen_tick(t_envgen *x){
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
    for(i = 0; i < ac; i++){
        if((av+i)->a_type != A_FLOAT){
            pd_error(x, "[envgen~]: list needs to only contain floats");
            return;
        }
    }
    copy_atoms(av, x->x_av, x->x_ac = ac);
}

static void envgen_curve(t_envgen *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        return;
    x->x_single_exp = (ac == 1);
    for(int i = 0; i < ac; i++){
        if((av+i)->a_type == A_FLOAT)
            SETFLOAT(x->x_at_exp+i, (av+i)->a_w.w_float * -4);
        else
            SETSYMBOL(x->x_at_exp+i, (av+i)->a_w.w_symbol);
    }
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

static void envgen_samps(t_envgen *x, t_floatarg f){
    x->x_samps = f != 0;
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
            float output;
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
            output = x->x_value[j];
            if(!x->x_pause && x->x_running[j]){ // not paused and running
                if(x->x_nleft[j] > 0){ // decrease
                    if(x->x_lintype == CURVE){ // if log curve
                        output = x->x_value[j];
                        x->x_b[j] *= x->x_inc[j]; // inc
                        x->x_value[j] = x->x_a[j] - x->x_b[j];
                    }
                    else if(x->x_lintype == POWER){ // if power exponential
                        output = x->x_value[j];
                        x->x_value[j] = x->x_last_target[j] + envgen_get_step(x, j);
                    }
                    else{ // if linear
                        output = x->x_value[j];
                        x->x_value[j] += x->x_inc[j];
                    }
                    x->x_nleft[j]--;
                }
                if(x->x_nleft[j] == 0){ // reached target, update!
                    output = x->x_value[j] = x->x_last_target[j] = x->x_target[j];
                    x->x_inc[j] = 0;
                    if(x->x_n_lines[j] > 0){ // there's more, retarget to the next
                        envgen_retarget(x, j);
                        output = x->x_value[j] = x->x_last_target[j];
                        if(x->x_lintype == CURVE){
                            x->x_b[j] *= x->x_inc[j];
                            x->x_value[j] = x->x_a[j] - x->x_b[j];
                        }
                        else if(x->x_lintype == POWER){
                            x->x_inc[j] = 1; // sample count
                            x->x_value[j] = x->x_last_target[j] + envgen_get_step(x, j);
                        }
                        else
                            x->x_value[j] += x->x_inc[j];
                    }
                    else if(!x->x_release[j]){ // there's no release, we're done.
                        x->x_running[j] = 0;
                        clock_delay(x->x_clock, 0);
                    }
                }
            }
            out[j*n + i] = output;
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
        x->x_a = (t_float *)resizebytes(x->x_a,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_b = (t_float *)resizebytes(x->x_b,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_exp_idx = (int *)resizebytes(x->x_exp_idx,
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
    freebytes(x->x_a, x->x_nchans * sizeof(*x->x_a));
    freebytes(x->x_b, x->x_nchans * sizeof(*x->x_b));
    freebytes(x->x_exp_idx, x->x_nchans * sizeof(*x->x_exp_idx));
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
    x->x_nchans = 1;
    x->x_gain = (t_float *)getbytes(sizeof(*x->x_gain));
    x->x_lastin = (t_float *)getbytes(sizeof(*x->x_lastin));
    x->x_value = (t_float *)getbytes(sizeof(*x->x_value));
    x->x_delta = (t_float *)getbytes(sizeof(*x->x_delta));
    x->x_target = (t_float *)getbytes(sizeof(*x->x_target));
    x->x_last_target = (t_float *)getbytes(sizeof(*x->x_last_target));
    x->x_inc = (t_float *)getbytes(sizeof(*x->x_inc));
    x->x_a = (t_float *)getbytes(sizeof(*x->x_a));
    x->x_b = (t_float *)getbytes(sizeof(*x->x_b));
    x->x_exp_idx = (int *)getbytes(sizeof(*x->x_exp_idx));
    x->x_n_lines = (int *)getbytes(sizeof(*x->x_n_lines));
    x->x_line_idx = (int *)getbytes(sizeof(*x->x_line_idx));
    x->x_running = (int *)getbytes(sizeof(*x->x_running));
    x->x_nleft = (int *)getbytes(sizeof(*x->x_nleft));
    x->x_n = (int *)getbytes(sizeof(*x->x_n));
    x->x_release = (int *)getbytes(sizeof(*x->x_release));
    x->x_lastin[0] = x->x_value[0] = x->x_delta[0] = 0.;
    x->x_gain[0] = 1;
    x->x_target[0] = x->x_last_target[0] = x->x_inc[0] = 0.;
    x->x_a[0] = x->x_b[0] = 0.;
    x->x_n[0] = x->x_exp_idx[0] = x->x_nleft[0] = x->x_release[0] = 0;
    x->x_n_lines[0] = x->x_line_idx[0] = x->x_running[0] = 0;
    x->x_retrigger = 0;
    x->x_pause = 0;
    x->x_suspoint = x->x_legato = 0;
    int i = 0;
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
            else if(cursym == gensym("-curve")){
                if(ac >= 2){
                    ac--, av++;
                    envgen_curve(x, NULL, 1, av);
                    ac--, av++;
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
            else if(cursym == gensym("-samps"))
                x->x_samps = 1, ac--, av++;
            else
                goto errstate;
        }
    }
    if(ac){
        if(ac == 1)
            x->x_value[0] = atom_getfloatarg(0, 1, av);
        else
            copy_atoms(av, x->x_av, x->x_ac = ac);
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
    class_addmethod(envgen_class, (t_method)envgen_curve, gensym("curve"), A_GIMME, 0);
    class_addmethod(envgen_class, (t_method)envgen_bang, gensym("attack"), 0);
    class_addmethod(envgen_class, (t_method)envgen_rel, gensym("release"), 0);
    class_addmethod(envgen_class, (t_method)envgen_pause, gensym("pause"), 0);
    class_addmethod(envgen_class, (t_method)envgen_resume, gensym("resume"), 0);
    class_addmethod(envgen_class, (t_method)envgen_legato, gensym("legato"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_samps, gensym("samps"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_suspoint, gensym("suspoint"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_retrigger, gensym("retrigger"), A_FLOAT, 0);
}
