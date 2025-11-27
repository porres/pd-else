// porres 2025

#include "m_pd.h"
#include "random.h"
#include <math.h>
#include <stdlib.h>


#define LEN 128

typedef struct _tempo{
    t_object        x_obj;
    t_random_state  x_rstate;
    t_clock        *x_clock;
    t_symbol       *x_ignore;
    int             x_id;
    int             x_mode;
    int             x_run;
    int             x_zero;
    int             x_sync;
    int             x_cycle;
    int             x_swing_index;
    float           x_swing_ratio;
    double          x_ms;
    double          x_tempo;
    double          x_mul;
    double          x_settime;
    double          x_remaining;
    float          *x_swing_array;
    float          *x_slack;
}t_tempo;

static t_class *tempo_class;

static void tempo_seed(t_tempo *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
}

static void tempo_setms(t_tempo *x){
    double tempo = x->x_tempo * x->x_mul;
    if(x->x_mode == 0)
        x->x_ms = 60000./tempo;
    else if(x->x_mode == 1)
        x->x_ms = tempo;
    if(x->x_mode == 2)
        x->x_ms = 1000./tempo;
}

static void tempo_set(t_tempo *x, t_floatarg f){
    if(f <= 0)
        return;
    x->x_tempo = f;
    tempo_setms(x);
}

static void tempo_mul(t_tempo *x, t_floatarg mul){
    if(mul <= 0)
        return;
    x->x_mul = mul;
    tempo_setms(x);
}

static void tempo_adaptive(t_tempo *x){
    x->x_remaining -= clock_gettimesince(x->x_settime);
    double old_ms = x->x_ms;
    tempo_setms(x);
    x->x_settime = clock_getlogicaltime();
    clock_delay(x->x_clock, x->x_remaining *= (x->x_ms/old_ms));
}

static void tempo_synced(t_tempo *x){
    tempo_setms(x);
    x->x_remaining = x->x_ms - clock_gettimesince(x->x_settime);
    if(x->x_remaining < 0)
        x->x_remaining = 0;
    clock_delay(x->x_clock, x->x_remaining);
}

static void tempo_sync(t_tempo *x, t_floatarg f){
    x->x_sync = f != 0;
}

static void tempo_swing(t_tempo *x, t_floatarg swing, t_floatarg cycle){
    if(swing < 0)
        swing = 0;
    x->x_swing_ratio = (swing/100.) + 1;
    x->x_cycle = cycle > 128 ? 128 : cycle;
}

/*static void tempo_cycle(t_tempo *x, t_floatarg f){
    x->x_cycle = f < 0 ? 0 : f > 128 ? 128 : (int)f;
}*/

static void tempo_tempo(t_tempo *x, t_floatarg tempo){
    int zero = tempo <= 0;
    if(zero != x->x_zero){
        x->x_zero = zero;
        if(!x->x_run)
            return;
        if(x->x_zero){ // stop
            x->x_remaining -= clock_gettimesince(x->x_settime);
            clock_unset(x->x_clock);
        }
        else{ // restart
            double old_ms = x->x_ms;
            x->x_tempo = tempo;
            tempo_setms(x);
            x->x_settime = clock_getlogicaltime();
            clock_delay(x->x_clock, x->x_remaining *= (x->x_ms/old_ms));
        }
    }
    else if(!x->x_zero){
        x->x_tempo = tempo;
        if(x->x_run){
            if(x->x_sync)
                tempo_synced(x);
            else
                tempo_adaptive(x);
        }
    }
}

static void tempo_set_swing_array(t_tempo *x){
    double sum = 0, total_slack = 0;
    double max = x->x_swing_ratio;
    double min = 1./max;
    int i;
    for(i = 0; i < x->x_cycle; i++){
        uint32_t *s1 = &x->x_rstate.s1;
        uint32_t *s2 = &x->x_rstate.s2;
        uint32_t *s3 = &x->x_rstate.s3;
        t_float noise = (t_float)(random_frand(s1, s2, s3));
        double deviation = exp(log(x->x_swing_ratio) * noise);
        x->x_swing_array[i] = deviation;
        sum += deviation;
    }
    double diff = x->x_cycle - sum;
    for(i = 0; i < x->x_cycle; i++){
        if(diff < 0)
            x->x_slack[i] = x->x_swing_array[i] - min;
        else
            x->x_slack[i] = max - x->x_swing_array[i];
        if(x->x_slack[i] < 0)
            x->x_slack[i] = 0;
        total_slack += x->x_slack[i];
    }
    for(i = 0; i < x->x_cycle; i++){
        x->x_swing_array[i] += (diff * x->x_slack[i] / total_slack);
        if(x->x_swing_array[i] < min)
            x->x_swing_array[i] = min;
        if(x->x_swing_array[i] > max)
            x->x_swing_array[i] = max;
    }
}

static void tempo_tick(t_tempo *x){
    outlet_bang(x->x_obj.ob_outlet);
    x->x_settime = clock_getlogicaltime();
    double ms;
    if(x->x_cycle > 0){
        if(x->x_cycle == 1)
            ms = x->x_ms;
        else{
            int idx = x->x_swing_index;
            if(idx == 0)
                tempo_set_swing_array(x);
            ms = x->x_ms * x->x_swing_array[idx];
            x->x_swing_index++;
            if(x->x_swing_index >= x->x_cycle)
                x->x_swing_index = 0;
        }
    }
    else{
        uint32_t *s1 = &x->x_rstate.s1;
        uint32_t *s2 = &x->x_rstate.s2;
        uint32_t *s3 = &x->x_rstate.s3;
        t_float noise = (t_float)(random_frand(s1, s2, s3));
        double deviation = exp(log(x->x_swing_ratio) * noise);
        ms = x->x_ms * deviation;
    }
    clock_delay(x->x_clock, x->x_remaining = ms);
}

static void tempo_bang(t_tempo *x){
    if(x->x_run)
        tempo_tick(x);
    x->x_swing_index = 0;
}

static void tempo_float(t_tempo *x, t_float f){
    x->x_run = f != 0;
    if(x->x_run)
        tempo_tick(x);
    else{
        clock_unset(x->x_clock);
        x->x_swing_index = 0;
    }
}

static void tempo_bpm(t_tempo *x ){
    x->x_mode = 0;
    tempo_setms(x);
}

static void tempo_ms(t_tempo *x ){
    x->x_mode = 1;
    tempo_setms(x);
}

static void tempo_hz(t_tempo *x ){
    x->x_mode = 2;
    tempo_setms(x);
}

static void tempo_start(t_tempo *x ){
    tempo_float(x, 1);
}

static void tempo_stop(t_tempo *x ){
    tempo_float(x, 0);
}

static void tempo_free(t_tempo *x){
    clock_free(x->x_clock);
    free(x->x_swing_array);
    free(x->x_slack);
}

static void *tempo_new(t_symbol *s, int ac, t_atom *av){
    t_tempo *x = (t_tempo *)pd_new(tempo_class);
    x->x_id = random_get_id();
    tempo_seed(x, s, 0, NULL);
    x->x_cycle = 0;
    float tempo = 0, mul = 1, swing = 0;
    int flag_no_more = 0, on = 0;
    x->x_swing_array = (float*)malloc(LEN * sizeof(float));
    x->x_slack = (float*)malloc(LEN * sizeof(float));
    while(ac > 0){
        if(ac && av->a_type == A_FLOAT){
            flag_no_more = 1;
            tempo = atom_getfloat(av);
                ac--;
                av++;
            if(ac && av->a_type == A_FLOAT){
                swing = atom_getfloat(av);
                ac--;
                av++;
            }
            if(ac && av->a_type == A_FLOAT){
                float arg = atom_getint(av);
                x->x_cycle = arg < 0 ? 0 : arg > 128 ? 128 : arg;
                ac--;
                av++;
            }
            if(ac && av->a_type == A_FLOAT)
                goto errstate;
        }
        else if(av->a_type == A_SYMBOL){
            if(flag_no_more)
                goto errstate;
            t_symbol *curarg = atom_getsymbol(av);
            if(curarg == gensym("-on")){
                on = 1;
                ac--;
                av++;
            }
            else if(curarg == gensym("-ms")){
                x->x_mode = 1;
                ac--;
                av++;
            }
            else if(curarg == gensym("-hz")){
                x->x_mode = 2;
                ac--;
                av++;
            }
            else if(curarg == gensym("-sync")){
                x->x_sync = 1;
                ac--;
                av++;
            }
            else if(curarg == gensym("-mul")){
                if((av+1)->a_type == A_FLOAT){
                    mul = atom_getintarg(1, ac, av);
                    ac-=2;
                    av+=2;
                }
                else
                    goto errstate;
            }
            else if(curarg == gensym("-seed")){
                if((av+1)->a_type == A_FLOAT){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    tempo_seed(x, s, 1, at);
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
    }
    x->x_tempo = tempo <= 0 ? 60 : tempo;
    x->x_mul = mul <= 0 ? 1 : mul;
    if(swing < 0)
        swing = 0;
    x->x_swing_ratio = (swing/100.) + 1;
    tempo_setms(x);
    x->x_clock = clock_new(x, (t_method)tempo_tick);
    if(on){
        x->x_run = 1;
        clock_delay(x->x_clock, 0);
    }
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("tempo"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("swing"));
    outlet_new(&x->x_obj, gensym("bang"));
    return(x);
errstate:
    pd_error(x, "[tempo]: improper args");
    return(NULL);
}

void tempo_setup(void){
    tempo_class = class_new(gensym("tempo"), (t_newmethod)(void *)tempo_new,
        (t_method)tempo_free, sizeof(t_tempo), 0, A_GIMME, 0);
    class_addfloat(tempo_class, (t_method)tempo_float);
    class_addbang(tempo_class, (t_method)tempo_bang);
    class_addmethod(tempo_class, (t_method)tempo_mul, gensym("mul"), A_FLOAT, 0);
    class_addmethod(tempo_class, (t_method)tempo_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(tempo_class, (t_method)tempo_tempo, gensym("tempo"), A_FLOAT, 0);
    class_addmethod(tempo_class, (t_method)tempo_sync, gensym("sync"), A_FLOAT, 0);
    class_addmethod(tempo_class, (t_method)tempo_swing, gensym("swing"), A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(tempo_class, (t_method)tempo_start, gensym("start"), 0);
    class_addmethod(tempo_class, (t_method)tempo_stop, gensym("stop"), 0);
    class_addmethod(tempo_class, (t_method)tempo_ms, gensym("ms"), 0);
    class_addmethod(tempo_class, (t_method)tempo_bpm, gensym("bpm"), 0);
    class_addmethod(tempo_class, (t_method)tempo_hz, gensym("hz"), 0);
//    class_addmethod(tempo_class, (t_method)tempo_cycle, gensym("cycle"), A_FLOAT, 0);
    class_addmethod(tempo_class, (t_method)tempo_seed, gensym("seed"), A_GIMME, 0);
}
