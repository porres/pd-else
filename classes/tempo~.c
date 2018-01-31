// Porres 2017

#include "m_pd.h"
#include "math.h"

static t_class *tempo_class;

typedef struct _tempo{
    t_object x_obj;
    double  x_phase;
    t_inlet  *x_inlet_bpm;
    t_outlet *x_outlet_dsp_0;
    t_float x_sr;
    t_float x_gate;
    t_float x_last_in;
    t_float x_mul;
} t_tempo;

static void tempo_mul(t_tempo *x, t_floatarg f){
    x->x_mul = f;
}

static t_int *tempo_perform(t_int *w){
    t_tempo *x = (t_tempo *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // gate
    t_float *in2 = (t_float *)(w[4]); // bpm
    t_float *out = (t_float *)(w[5]);
    float lastin = x->x_last_in;
    double phase = x->x_phase;
    double sr = x->x_sr;
    while (nblock--){
        float gate = *in1++;
        double bpm = *in2++;
        double hz;
        if (x->x_mul <= 0)
            hz = sr;
        else{
            if (bpm < 0)
                bpm = 0;
            hz = bpm / (60 * x->x_mul);
        }
        double phase_step = hz / sr; // phase_step
        if (phase_step > 1)
            phase_step = 1;
        if (gate != 0){
            if (lastin == 0)
                phase = 1;
            *out++ = phase >= 1.;
            if (phase >= 1.)
                phase = phase - 1; // wrapped phase
            phase += phase_step; // next phase
            }
        else
            *out++ = 0;
        lastin = gate;
    }
    x->x_phase = phase;
    x->x_last_in = lastin;
    return (w + 6);
}

static void tempo_dsp(t_tempo *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    dsp_add(tempo_perform, 5, x, sp[0]->s_n,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *tempo_free(t_tempo *x){
    inlet_free(x->x_inlet_bpm);
    outlet_free(x->x_outlet_dsp_0);
    return (void *)x;
}

static void *tempo_new(t_symbol *s, int argc, t_atom *argv){
    t_tempo *x = (t_tempo *)pd_new(tempo_class);
    t_float init_mul = 0;
    t_float init_bpm = 0;
    t_float on = 0;
/////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(argc > 0)
    {
        if(argv->a_type == A_FLOAT)
        { //if current argument is a float
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum)
            {
                case 0:
                    init_bpm = argval;
                    break;
                case 1:
                    init_mul = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            argc--;
            argv++;
        }
        else if (argv -> a_type == A_SYMBOL)
        {
            t_symbol *curarg = atom_getsymbolarg(0, argc, argv);
            if(strcmp(curarg->s_name, "-on")==0)
            {
                on = 1;
                argc -= 1;
                argv += 1;
            }
            else
            {
                goto errstate;
            };
        }
    };
/////////////////////////////////////////////////////////////////////////////////////
    if (init_bpm < 0)
        init_bpm = 0;
    if (init_mul <= 0)
        init_mul = 1;
    x->x_mul = init_mul;
    x->x_gate = on;
    x->x_sr = sys_getsr(); // sample rate
    x->x_phase = 1;
    x->x_inlet_bpm = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_bpm, init_bpm);
    x->x_outlet_dsp_0 = outlet_new(&x->x_obj, &s_signal);
    return (x);
errstate:
    pd_error(x, "tempo~: improper args");
    return NULL;
}

void tempo_tilde_setup(void){
    tempo_class = class_new(gensym("tempo~"),(t_newmethod)tempo_new, (t_method)tempo_free,
            sizeof(t_tempo), CLASS_DEFAULT, A_GIMME, 0);
    CLASS_MAINSIGNALIN(tempo_class, t_tempo, x_gate);
    class_addmethod(tempo_class, (t_method)tempo_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(tempo_class, (t_method)tempo_mul, gensym("mul"), A_DEFFLOAT, 0);
}
