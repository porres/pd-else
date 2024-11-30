// tim schoen

#include <m_pd.h>
#include <blep.h>

typedef struct blimp{
    t_object    x_obj;
    t_float     x_f;
    t_elliptic_blep x_elliptic_blep;
    t_float     x_phase;
    t_float     x_sr;
    t_int       x_midi;
}t_blimp;

t_class *blimp_class;

static t_int *blimp_perform(t_int *w){
    t_blimp* x        = (t_blimp*)(w[1]);
    t_elliptic_blep* blep = &x->x_elliptic_blep;
    t_int n            = (t_int)(w[2]);
    t_float* freq_vec  = (t_float *)(w[3]);
    t_float* out       = (t_float *)(w[4]);
    while(n--){
        t_float freq = *freq_vec++;
        if(x->x_midi && freq < 256)
            freq = pow(2, (freq - 69)/12) * 440;
        
        *out++ = elliptic_blep_get(blep);
        
        t_float phase_increment = freq / x->x_sr; // Update frequency
        x->x_phase += phase_increment;
    
        elliptic_blep_step(blep);
        
        if(x->x_phase >= 1 || x->x_phase < 0) {
            x->x_phase = x->x_phase < 0 ? x->x_phase+1 : x->x_phase-1;
            t_float samples_in_past = x->x_phase / phase_increment;
            elliptic_blep_add_in_past(blep, 1.0f, 0, samples_in_past);
        }
    }
    return(w+5);
}

static void blimp_midi(t_blimp *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void blimp_dsp(t_blimp *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    
    elliptic_blep_create(&x->x_elliptic_blep, 1, sp[0]->s_sr);
    
    dsp_add(blimp_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void *blimp_new(t_symbol *s, int ac, t_atom *av){
    t_blimp *x = (t_blimp *)pd_new(blimp_class);
    x->x_phase = 0.0;
    x->x_midi = 0;
    t_float init_freq = 0;
    if(ac && av->a_type == A_SYMBOL){
        if(atom_getsymbol(av) == gensym("-midi"))
            x->x_midi = 1;
    }
    if(ac && av->a_type == A_FLOAT){
        init_freq = av->a_w.w_float;
        ac--; av++;
    }
    x->x_f = init_freq;
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void setup_bl0x2eimp_tilde(void){
    blimp_class = class_new(gensym("bl.imp~"), (t_newmethod)blimp_new,
        0, sizeof(t_blimp), 0, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(blimp_class, t_blimp, x_f);
    class_addmethod(blimp_class, (t_method)blimp_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(blimp_class, (t_method)blimp_dsp, gensym("dsp"), A_NULL);
}
