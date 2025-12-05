// Porres 2017-2025

#include "m_pd.h"
#include "magic.h"
#include "random.h"
#include <math.h>
#include <stdlib.h>

#define LEN 128
#define MAXLEN 1024

typedef struct _tempo{
    t_object       x_obj;
    t_random_state x_rstate;
    t_inlet       *x_inlet_tempo;
    t_inlet       *x_inlet_swing;
    t_inlet       *x_inlet_sync;
    double        *x_phase;
    int           *x_count;
    double        *x_ms;
    int           *x_swing_index;
    t_float       *x_deviation;
    t_float       *x_last_gate;
    t_float       *x_last_sync;
    t_float        x_gate;
    int            x_run;
    t_float        x_sr;
    t_float        x_mul;
    t_float        x_current_mul;
    t_int          x_mode;
    t_int          x_synced;
    int            x_id;
    int            x_cycle;
    float         *x_swing_array;
    float         *x_slack;
    double         x_swing_ratio;
    t_float        x_set_tempo;
    t_glist       *x_glist;
    t_float       *x_sigscalar1;
    t_float       *x_sigscalar2;
    t_int          x_ch1;
    t_int          x_ch2;
    t_int          x_ch3;
    t_int          x_ch4;
    t_int          x_sig2;
    t_int          x_sig3;
    int            x_nchs;
    t_int          x_n;
    float         *x_tempo_list;
    float         *x_set_tempo_list;
    float         *x_swing_list;
    t_int          x_tempo_list_size;
    t_int          x_swing_list_size;
    t_symbol      *x_ignore;
}t_tempo;

static t_class *tempo_class;

static float tempo_get_noise(t_tempo *x){
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    return((t_float)(random_frand(s1, s2, s3)));
}
    
static void tempo_set_swing_array(t_tempo *x, int j){
    int offset = j*LEN;
    double sum = 0, total_slack = 0;
    double max = x->x_swing_ratio;
    double min = 1./max;
    int i;
    for(i = 0; i < x->x_cycle; i++){
        double deviation = exp(log(x->x_swing_ratio) * tempo_get_noise(x));
        x->x_swing_array[offset+i] = deviation;
        sum += deviation;
    }
    double diff = x->x_cycle - sum;
    for(i = 0; i < x->x_cycle; i++){
        if(diff < 0)
            x->x_slack[i] = x->x_swing_array[offset+i] - min;
        else
            x->x_slack[i] = max - x->x_swing_array[offset+i];
        if(x->x_slack[i] < 0)
            x->x_slack[i] = 0;
        total_slack += x->x_slack[i];
    }
    for(i = 0; i < x->x_cycle; i++){
        x->x_swing_array[offset+i] += (diff * x->x_slack[i] / total_slack);
        if(x->x_swing_array[offset+i] < min)
            x->x_swing_array[offset+i] = min;
        if(x->x_swing_array[offset+i] > max)
            x->x_swing_array[offset+i] = max;
    }
}

static void tempo_update_deviation(t_tempo *x, t_floatarg swing, int j){
    float actual_swing = x->x_sig3 ? swing : x->x_swing_list[j];
    if(actual_swing < 0)
        actual_swing = 0;
    x->x_swing_ratio = (actual_swing/100.) + 1;
    if(x->x_cycle > 0){
        if(x->x_cycle == 1)
            x->x_deviation[j] = 1;
        else{
            int idx = x->x_swing_index[j];
            if(idx == 0)
                tempo_set_swing_array(x, j);
            x->x_deviation[j] = x->x_swing_array[idx+j*LEN];
            x->x_swing_index[j]++;
            if(x->x_swing_index[j] >= x->x_cycle)
                x->x_swing_index[j] = 0;
        }
    }
    else
        x->x_deviation[j] = exp(log(x->x_swing_ratio) * tempo_get_noise(x));
}

static t_int *tempo_perform(t_int *w){
    t_tempo *x = (t_tempo *)(w[1]);
    t_float *in1 = (t_float *)(w[2]); // gate
    t_float *in2 = (t_float *)(w[3]); // tempo
    t_float *in3 = (t_float *)(w[4]); // swing
    t_float *in4 = (t_float *)(w[5]); // sync
    t_float *out = (t_float *)(w[6]);
    t_float *phaseout = (t_float *)(w[7]);
    float *last_gate = x->x_last_gate; // last gate state
    float *last_sync = x->x_last_sync;
    float *deviation = x->x_deviation;
    double *phase = x->x_phase;
    int *count = x->x_count;
    double sr = x->x_sr;
    int n = x->x_n;
    if(!x->x_sig2){
        t_float *scalar = x->x_sigscalar1;
        if(!else_magic_isnan(*x->x_sigscalar1)){
            float tempo = *scalar;
            for(int i = 0; i < x->x_nchs; i++)
                x->x_tempo_list[i] = x->x_set_tempo_list[i] = tempo;
            else_magic_setnan(x->x_sigscalar1);
        }
    }
    if(!x->x_sig3){
        t_float *scalar = x->x_sigscalar2;
        if(!else_magic_isnan(*x->x_sigscalar2)){
            float swing = *scalar;
            for(int i = 0; i < x->x_nchs; i++)
                x->x_swing_list[i] = swing;
            else_magic_setnan(x->x_sigscalar2);
        }
    }
    for(int j = 0; j < x->x_nchs; j++){
        for(int i = 0; i < n; i++){
            float gate = x->x_ch1 == 1 ? in1[i] : in1[j*n + i];
            double tempo;
            if(x->x_ch2 == 1)
                tempo = x->x_sig2 ? in2[i] : x->x_tempo_list[0];
            else
                tempo = x->x_sig2 ? in2[j*n + i] : x->x_tempo_list[j];
            float swing;
            if(x->x_ch3 == 1)
                swing = x->x_sig3 ? in3[i] : x->x_swing_list[0];
            else
                swing = x->x_sig3 ? in3[j*n + i] : x->x_swing_list[j];
            float sync = x->x_ch4 == 1 ? in4[i] : in4[j*n + i];
            float output = 0, out2 = 0;
            // get and set tempo/phase step
            if(!x->x_sig2){
                if(!x->x_synced && phase[j] >= 1){
                    if(x->x_ch2 == 1)
                        x->x_tempo_list[0] = x->x_set_tempo_list[0];
                    else
                        x->x_tempo_list[j] = x->x_set_tempo_list[j];
                }
                tempo = x->x_ch2 == 1 ? x->x_tempo_list[0] : x->x_tempo_list[j];
            }
            if(tempo < 0)
                tempo = 0;
            double t = (double)tempo, ms;
            int zero = 0;
            if(x->x_mode == 0){ // bpm
                zero = t == 0;
                if(t == 0){
                    zero = 1;
                    ms = x->x_ms[j];
                }
                else
                    ms = 60000./t;
            }
            else if(x->x_mode == 1) // ms
                ms = t;
            else // x->x_mode == 2 is hz
                ms = 1000./t;
            x->x_ms[j] = ms;
            ms *= (deviation[j]/(double)x->x_current_mul);
            int period = (int)((ms / 1000.) * sr);
            if(period < 1)
                period = 1;
            double hz = 1000./ms;
            double phase_step;
            if(zero)
                phase_step = 0;
            else
                phase_step = hz / sr;
            if(phase_step < 0)
                phase_step = 0;
            if(phase_step > 1)
                phase_step = 1;
    // deal with gate
            if(gate || x->x_run){ // gate on
                if(gate){ // audio gate
                    if(!x->x_run){
                        if(last_gate[j] == 0){
                            phase[j] = 1;
                            count[j] = period;
                        }
                    }
                }
                else{ // control gate (x->x_run)
                    if(phase[j] == 1)
                        count[j] = period;
                }
                if(sync != 0 && last_sync[j] == 0){
                    phase[j] = 1;
                    count[j] = period;
                    x->x_swing_index[j] = 0;
                }
                if(x->x_synced){ // synced update mode, use count
                    if(phase[j] >= 1.){
                        phase[j] = 0;
                        count[j] = period;
                    }
                    output = count[j] >= period;
                    if(count[j] >= period){
                        x->x_tempo_list[j] = x->x_set_tempo_list[j];
                        count[j] = 0; // reset
                        x->x_current_mul = x->x_mul;
                        tempo_update_deviation(x, swing, j);
                    }
                    out2 = phase[j] = (double)count[j]/(double)period;
                    if(!zero)
                        count[j]++;
                }
                else{ // adaptive, use phase step
                    output = phase[j] >= 1.;
                    if(phase[j] >= 1.){
                        x->x_tempo_list[j] = x->x_set_tempo_list[j];
                        phase[j] -= 1; // wrapped phase
                        x->x_current_mul = x->x_mul;
                        tempo_update_deviation(x, swing, j);
                    }
                    out2 = phase[j];
                    phase[j] += phase_step;
                }
            }
            else{
                out2 = phase[j] = 0; // also reset phase for real???
                x->x_swing_index[j] = 0;
            }
            last_gate[j] = gate;
            last_sync[j] = sync;
            deviation[j] = x->x_deviation[j];
            out[j*n+i] = output;
            phaseout[j*n+i] = out2;
        }
    }
    x->x_count = count;
    x->x_phase = phase;
    x->x_last_gate = last_gate;
    x->x_last_sync = last_sync;
    return(w+8);
}

static void tempo_dsp(t_tempo *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_sr = sp[0]->s_sr;
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    x->x_ch2 = x->x_sig2 ? sp[1]->s_nchans : x->x_tempo_list_size;
    x->x_ch3 = x->x_sig3 ? sp[2]->s_nchans : x->x_swing_list_size;
    x->x_ch4 = sp[3]->s_nchans;
    int chs = x->x_ch1 = sp[0]->s_nchans;
    if(x->x_ch2 > 1)
        chs = x->x_ch2;
    if(x->x_ch3 > 1)
        chs = x->x_ch3;
    if(x->x_ch4 > 1)
        chs = x->x_ch4;
    if(x->x_nchs != chs){
        x->x_swing_array = (float *)resizebytes(x->x_swing_array,
            x->x_nchs * LEN * sizeof(float), chs * LEN * sizeof(float));
        x->x_slack = (float *)resizebytes(x->x_slack,
            x->x_nchs * LEN * sizeof(float), chs * LEN * sizeof(float));
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_ms = (double *)resizebytes(x->x_ms,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_count = (int *)resizebytes(x->x_count,
            x->x_nchs * sizeof(int), chs * sizeof(int));
        x->x_swing_index = (int *)resizebytes(x->x_swing_index,
            x->x_nchs * sizeof(int), chs * sizeof(int));
        x->x_deviation = (t_float *)resizebytes(x->x_deviation,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_last_gate = (t_float *)resizebytes(x->x_last_gate,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_last_sync = (t_float *)resizebytes(x->x_last_sync,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchs = chs;
    }
    signal_setmultiout(&sp[4], x->x_nchs);
    signal_setmultiout(&sp[5], x->x_nchs);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchs)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchs)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchs)
    || (x->x_ch4 > 1 && x->x_ch4 != x->x_nchs)){
        dsp_add_zero(sp[4]->s_vec, x->x_nchs*x->x_n);
        dsp_add_zero(sp[5]->s_vec, x->x_nchs*x->x_n);
        pd_error(x, "[tempo~]: channel sizes mismatch");
    }
    else
        dsp_add(tempo_perform, 7, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
            sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
}

static void tempo_seed(t_tempo *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
    for(int i = 0; i < x->x_nchs; i++)
        x->x_deviation[i] = x->x_phase[i] = 1; // ??????
}

static void tempo_bang(t_tempo *x){
    for(int i = 0; i < x->x_nchs; i++)
        x->x_phase[i] = 1;
}

static void tempo_mul(t_tempo *x, t_floatarg f){
    x->x_mul = f <= 0 ? 0.0000000000000001 : f;
}

static void tempo_stop(t_tempo *x){
    x->x_run = 0;
}

static void tempo_start(t_tempo *x){
    if(!x->x_run){
        x->x_run = 1;
        for(int i = 0; i < x->x_nchs; i++)
            x->x_phase[i] = 1;
    }
}

static void tempo_swing(t_tempo *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0 || ac > 128)
        return;
    for(int i = 0; i < ac; i++)
        x->x_swing_list[i] = atom_getfloat(av+i);
    else_magic_setnan(x->x_sigscalar2);
    if(x->x_swing_list_size != ac){
        x->x_swing_list_size = ac;
        canvas_update_dsp();
    }
}

static void tempo_tempo(t_tempo *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_tempo_list_size != ac){
        x->x_tempo_list_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_set_tempo_list[i] = x->x_tempo_list[i] = atom_getfloat(av+i);
    else_magic_setnan(x->x_sigscalar1);
}

static void tempo_set(t_tempo *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_tempo_list_size != ac){
        x->x_tempo_list_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_set_tempo_list[i] = atom_getfloat(av+i);
    else_magic_setnan(x->x_sigscalar1);
}

static void tempo_synced(t_tempo *x, t_floatarg f){
    x->x_synced = (f != 0);
}

static void tempo_cycle(t_tempo *x, t_floatarg f){
    x->x_cycle = f > 128 ? 128 : (int)f;
}

static void tempo_bpm(t_tempo *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac){
        pd_float((t_pd *)x->x_inlet_tempo, atom_getfloatarg(0, ac, av));
        if(ac >= 2)
            pd_float((t_pd *)x->x_inlet_swing, atom_getfloatarg(1, ac, av));
        if(ac == 3)
            tempo_cycle(x, atom_getfloatarg(2, ac, av));
    }
    x->x_mode = 0;
}

static void tempo_ms(t_tempo *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac){
        pd_float((t_pd *)x->x_inlet_tempo, atom_getfloatarg(0, ac, av));
        if(ac >= 2)
            pd_float((t_pd *)x->x_inlet_swing, atom_getfloatarg(1, ac, av));
        if(ac == 3)
            tempo_cycle(x, atom_getfloatarg(2, ac, av));
    }
    x->x_mode = 1;
}

static void tempo_hz(t_tempo *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac){
        pd_float((t_pd *)x->x_inlet_tempo, atom_getfloatarg(0, ac, av));
        if(ac >= 2)
            pd_float((t_pd *)x->x_inlet_swing, atom_getfloatarg(1, ac, av));
        if(ac == 3)
            tempo_cycle(x, atom_getfloatarg(2, ac, av));
    }
    x->x_mode = 2;
}

static void *tempo_free(t_tempo *x){
    inlet_free(x->x_inlet_tempo);
    inlet_free(x->x_inlet_swing);
    inlet_free(x->x_inlet_sync);
    freebytes(x->x_swing_array, x->x_nchs * LEN * sizeof(*x->x_swing_array));
    freebytes(x->x_slack, x->x_nchs * LEN * sizeof(*x->x_slack));
    free(x->x_tempo_list);
    free(x->x_set_tempo_list);
    free(x->x_swing_list);
    return(void *)x;
}

static void *tempo_new(t_symbol *s, int ac, t_atom *av){
    t_tempo *x = (t_tempo *)pd_new(tempo_class);
    x->x_id = random_get_id();
    tempo_seed(x, s, 0, NULL);
    int init_cycle = 0;
    t_float init_swing = 0, init_tempo = 0;
    t_float on = 0, mode = 0, mul = 1;
    x->x_synced = 0;
    int flag_no_more = 0;
    x->x_nchs = 1;
    x->x_phase = (double*)malloc(sizeof(double));
    x->x_phase[0] = 1;
    x->x_ms = (double*)malloc(sizeof(double));
    x->x_ms[0] = 0;
    x->x_swing_index = (int*)malloc(sizeof(int));
    x->x_swing_index[0] = 0;
    x->x_count = (int*)malloc(sizeof(int));
    x->x_count[0] = 0;
    x->x_last_sync = (t_float*)malloc(sizeof(t_float));
    x->x_last_sync[0] = 0;
    x->x_last_gate = (t_float*)malloc(sizeof(t_float));
    x->x_last_gate[0] = 0;
    x->x_deviation = (t_float*)malloc(sizeof(t_float));
    x->x_deviation[0] = 0;
    x->x_tempo_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_tempo_list_size = 1;
    x->x_set_tempo_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_swing_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_swing_list_size = 1;
    x->x_swing_array = (float*)malloc(LEN * sizeof(float));
    x->x_slack = (float*)malloc(LEN * sizeof(float));
/////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            flag_no_more = 1;
            t_float aval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    init_tempo = aval;
                    break;
                case 1:
                    init_swing = aval;
                    break;
                case 2:
                    init_cycle = (int)aval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--;
            av++;
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
                mode = 1;
                ac--;
                av++;
            }
            else if(curarg == gensym("-hz")){
                mode = 2;
                ac--;
                av++;
            }
            else if(curarg == gensym("-sync")){
                x->x_synced = 1;
                ac--;
                av++;
            }
            else if(curarg == gensym("-mul")){
                if((av+1)->a_type == A_FLOAT){
                    mul = atom_getfloatarg(1, ac, av);
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
    };
/////////////////////////////////////////////////////////////////////////////////////
    if(init_tempo < 0)
        init_tempo = 0;
    if(init_swing < 0)
        init_swing = 0;
    if(mul < 1)
        mul = 1;
    x->x_current_mul = x->x_mul = mul;
    x->x_cycle = init_cycle < 0 ? 0 : init_cycle > 128 ? 128 : init_cycle;
    x->x_run = 0;
    x->x_mode = mode;
    x->x_sr = sys_getsr(); // sample rate
    x->x_tempo_list[0] = x->x_set_tempo_list[0] = init_tempo;
    x->x_swing_list[0] = init_swing;
    x->x_inlet_tempo = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_inlet_swing = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_inlet_sync = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    x->x_sigscalar1 = obj_findsignalscalar((t_object *)x, 1);
    else_magic_setnan(x->x_sigscalar1);
    x->x_sigscalar2 = obj_findsignalscalar((t_object *)x, 2);
    else_magic_setnan(x->x_sigscalar2);
    if(on)
        tempo_start(x);
    return(x);
errstate:
    pd_error(x, "[tempo~]: improper args");
    return(NULL);
}

void tempo_tilde_setup(void){
    tempo_class = class_new(gensym("tempo~"),(t_newmethod)(void *)tempo_new,
        (t_method)tempo_free, sizeof(t_tempo), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(tempo_class, t_tempo, x_gate);
    class_addbang(tempo_class, tempo_bang);
    class_addmethod(tempo_class, (t_method)tempo_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(tempo_class, (t_method)tempo_ms, gensym("ms"), A_GIMME, 0);
    class_addmethod(tempo_class, (t_method)tempo_hz, gensym("hz"), A_GIMME, 0);
    class_addmethod(tempo_class, (t_method)tempo_bpm, gensym("bpm"), A_GIMME, 0);
    class_addmethod(tempo_class, (t_method)tempo_mul, gensym("mul"), A_FLOAT, 0);
    class_addmethod(tempo_class, (t_method)tempo_start, gensym("start"), 0);
    class_addmethod(tempo_class, (t_method)tempo_stop, gensym("stop"), 0);
    class_addmethod(tempo_class, (t_method)tempo_tempo, gensym("tempo"), A_GIMME, 0);
    class_addmethod(tempo_class, (t_method)tempo_set, gensym("set"), A_GIMME, 0);
    class_addmethod(tempo_class, (t_method)tempo_swing, gensym("swing"), A_GIMME, 0);
    class_addmethod(tempo_class, (t_method)tempo_cycle, gensym("cycle"), A_FLOAT, 0);
    class_addmethod(tempo_class, (t_method)tempo_synced, gensym("sync"), A_FLOAT, 0);
    class_addmethod(tempo_class, (t_method)tempo_seed, gensym("seed"), A_GIMME, 0);
}
