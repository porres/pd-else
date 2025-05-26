#include "m_pd.h"

//IMPROVE - 
//IMPROVE - 
//TODO - hep file

#include "rings/dsp/part.h"
#include "rings/dsp/patch.h"
#include "rings/dsp/strummer.h"
#include "rings/dsp/string_synth_part.h"


inline float constrain(float v, float vMin, float vMax) {
    return std::max<float>(vMin, std::min<float>(vMax, v));
}

static t_class *rngs_tilde_class;

typedef struct _rngs_tilde {
    t_object x_obj;

    t_float f_dummy;

    t_float f_pitch;
    t_float f_transpose;
    t_float f_structure;
    t_float f_brightness;
    t_float f_damping;
    t_float f_position;
    t_float f_bypass;
    t_float f_easter_egg;
    t_float f_polyphony;
    t_float f_model;
    t_float f_chord;
    t_float f_trig;
    t_float f_fm;
    t_float f_internal_strum;
    t_float f_internal_exciter;
    t_float f_internal_note;

    // CLASS_MAINSIGNALIN  = in
    t_outlet *x_out_odd;
    t_outlet *x_out_even;

    rings::Part part;
    rings::PerformanceState performance_state;
    rings::StringSynthPart string_synth;
    rings::Strummer strummer;
    rings::Patch patch;


    float kNoiseGateThreshold;
    float in_level;
    float *in;
    int iobufsz;
    float pitch_offset=0.0f;

    static const int REVERB_SZ = 32768;
    uint16_t buffer[REVERB_SZ];


} t_rngs_tilde;


//define pure data methods
extern "C" {
t_int *rngs_tilde_render(t_int *w);
void rngs_tilde_dsp(t_rngs_tilde *x, t_signal **sp);
void rngs_tilde_free(t_rngs_tilde *x);
void *rngs_tilde_new(t_floatarg f);

void rngs_tilde_setup(void);


void rngs_tilde_pitch(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_transpose(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_fm(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_trig(t_rngs_tilde *x);
void rngs_tilde_model(t_rngs_tilde *x, t_floatarg f);

void rngs_tilde_chord(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_poly(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_structure(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_brightness(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_damping(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_position(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_bypass(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_easter_egg(t_rngs_tilde *x, t_floatarg f);

void rngs_tilde_gen_strum(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_gen_exciter(t_rngs_tilde *x, t_floatarg f);
void rngs_tilde_gen_note(t_rngs_tilde *x, t_floatarg f);
}

// puredata methods implementation -start
t_int *rngs_tilde_render(t_int *w) {
    t_rngs_tilde *x = (t_rngs_tilde *) (w[1]);
    t_sample *in = (t_sample *) (w[2]);
    t_sample *out = (t_sample *) (w[3]);
    t_sample *aux = (t_sample *) (w[4]);
    int n = (int) (w[5]);
    size_t size = n;

    for (int i = 0; i < n; i++) {
        out[i] = in[i];
    }

    if (n > x->iobufsz) {
        delete[] x->in;
        x->iobufsz = n;
        x->in = new float[x->iobufsz];
    }


    x->patch.brightness = constrain(x->f_brightness, 0.0f, 1.0f);
    x->patch.damping = constrain(x->f_damping, 0.0f, 1.0f);
    x->patch.position = constrain(x->f_position, 0.0f, 1.0f);
    x->patch.structure = constrain(x->f_structure, 0.0f, 0.9995f);
    x->performance_state.fm = constrain(x->f_fm, -48.0f, 48.0f);
    x->performance_state.note = x->f_pitch + x->pitch_offset;
    x->performance_state.tonic = 12.0f + x->f_transpose;
    x->performance_state.internal_exciter = x->f_internal_exciter > 0.5;
    x->performance_state.internal_strum =  x->f_internal_strum > 0.5;
    x->performance_state.internal_note =  x->f_internal_note > 0.5;
    x->performance_state.chord = x->patch.structure * constrain(x->f_chord, 0, rings::kNumChords - 1);

    x->performance_state.strum = x->f_trig > 0.5;
    x->f_trig=0.0f;


    x->f_polyphony = constrain(1 << int(x->f_polyphony) , 1, rings::kMaxPolyphony);

    if(x->f_polyphony != x->part.polyphony()) {
        x->part.set_polyphony(x->f_polyphony);
        x->string_synth.set_polyphony(x->f_polyphony);
    }

    rings::ResonatorModel model = static_cast<rings::ResonatorModel>((int) x->f_model);
    x->f_model=constrain(x->f_model,0,rings::ResonatorModel::RESONATOR_MODEL_LAST - 1);
    if(model != x->part.model()) {
        x->part.set_model(model);
        x->string_synth.set_fx(static_cast<rings::FxType>(model));
    }


    x->part.set_bypass(x->f_bypass > 0.5);

    if (x->f_easter_egg > 0.5) {
        for (size_t i = 0; i < size; ++i) {
            x->in[i] = in[i];
        }
        x->strummer.Process(NULL, size, &(x->performance_state));
        x->string_synth.Process(x->performance_state, x->patch, in, out, aux, size);
    } else {
        // Apply noise gate.
        for (size_t i = 0; i < size; ++i) {
            float in_sample = in[i];
            float error, gain;
            error = in_sample * in_sample - x->in_level;
            x->in_level += error * (error > 0.0f ? 0.1f : 0.0001f);
            gain = x->in_level <= x->kNoiseGateThreshold
                   ? (1.0f / x->kNoiseGateThreshold) * x->in_level : 1.0f;
            x->in[i] = gain * in_sample;
        }
        x->strummer.Process(x->in, size, &(x->performance_state));
        x->part.Process(x->performance_state, x->patch, x->in, out, aux, size);
    }

    return (w + 6); // # args + 1
}


void rngs_tilde_dsp(t_rngs_tilde *x, t_signal **sp) {
    // add the perform method, with all signal i/o
    dsp_add(rngs_tilde_render, 5,
            x,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, // signal i/o (clockwise)
            sp[0]->s_n);
}

void rngs_tilde_free(t_rngs_tilde *x) {
    delete[] x->in;
    outlet_free(x->x_out_odd);
    outlet_free(x->x_out_even);
}

void *rngs_tilde_new(t_floatarg) {
    t_rngs_tilde *x = (t_rngs_tilde *) pd_new(rngs_tilde_class);

    x->f_polyphony = 0.0f;
    x->f_model = 0.0f;
    x->f_pitch = 60.0f;
    x->f_structure = 0.4f;
    x->f_brightness = 0.5f;
    x->f_damping = 0.5f;
    x->f_position = 0.5f;
    x->f_bypass = 0.0f;
    x->f_easter_egg = 0.0f;
    x->f_internal_exciter = 1;
    x->f_internal_strum = 0;
    x->f_internal_note= 0;
    x->f_chord = 0;
    x->f_transpose = 0;
    x->f_fm = 0;
    x->f_trig = 0;

    x->in_level = 0.0f;

    x->kNoiseGateThreshold = 0.00003f;

    if(sys_getsr()!=48000.0f) {
        post("rngs~.pd is designed for 48k, not %f, approximating pitch", sys_getsr());
        if(sys_getsr()==44100) {
            x->pitch_offset=1.46f;
        }
    }

    x->strummer.Init(0.01f, sys_getsr() / sys_getblksize());
    x->string_synth.Init(x->buffer);
    x->part.Init(x->buffer);

    x->iobufsz = sys_getblksize();
    x->in = new float[x->iobufsz];

    //x_in_strike = main input
    x->x_out_odd = outlet_new(&x->x_obj, &s_signal);
    x->x_out_even = outlet_new(&x->x_obj, &s_signal);


    x->part.set_polyphony(x->f_polyphony);
    x->string_synth.set_polyphony(x->f_polyphony);
    rings::ResonatorModel model = static_cast<rings::ResonatorModel>((int) x->f_model);
    x->part.set_model(model);
    x->string_synth.set_fx(static_cast<rings::FxType>(model));

    return (void *) x;
}


void rngs_tilde_setup(void) {
    rngs_tilde_class = class_new(gensym("rngs~"),
                                 (t_newmethod) rngs_tilde_new,
                                 (t_method) rngs_tilde_free,
                                 sizeof(t_rngs_tilde),
                                 CLASS_DEFAULT,
                                 A_DEFFLOAT, A_NULL);

    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_dsp,
                    gensym("dsp"), A_NULL);

    // represents strike input
    CLASS_MAINSIGNALIN(rngs_tilde_class, t_rngs_tilde, f_dummy);

    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_poly, gensym("poly"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_model, gensym("model"),
                    A_DEFFLOAT, A_NULL);


    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_pitch, gensym("pitch"),
                    A_DEFFLOAT, A_NULL);

    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_transpose, gensym("transpose"),
                    A_DEFFLOAT, A_NULL);

    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_structure, gensym("structure"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_brightness, gensym("brightness"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_damping, gensym("damping"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_position, gensym("position"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_bypass, gensym("bypass"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_easter_egg, gensym("easter_egg"),
                    A_DEFFLOAT, A_NULL);

    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_gen_strum, gensym("gen_strum"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_gen_exciter, gensym("gen_exciter"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_gen_note, gensym("gen_note"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_chord, gensym("chord"),
                    A_DEFFLOAT, A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_trig, gensym("trig"),
                     A_NULL);
    class_addmethod(rngs_tilde_class,
                    (t_method) rngs_tilde_fm, gensym("fm"),
                    A_DEFFLOAT, A_NULL);
}

void rngs_tilde_pitch(t_rngs_tilde *x, t_floatarg f) {
    x->f_pitch = f;
}

void rngs_tilde_transpose(t_rngs_tilde *x, t_floatarg f) {
    x->f_transpose = f;
}

void rngs_tilde_structure(t_rngs_tilde *x, t_floatarg f) {
    x->f_structure = f;
}

void rngs_tilde_brightness(t_rngs_tilde *x, t_floatarg f) {
    x->f_brightness = f;
}

void rngs_tilde_damping(t_rngs_tilde *x, t_floatarg f) {
    x->f_damping = f;
}

void rngs_tilde_position(t_rngs_tilde *x, t_floatarg f) {
    x->f_position = f;
}


void rngs_tilde_bypass(t_rngs_tilde *x, t_floatarg f) {
    x->f_bypass = f;
}

void rngs_tilde_easter_egg(t_rngs_tilde *x, t_floatarg f) {
    x->f_easter_egg = f;

}

void rngs_tilde_poly(t_rngs_tilde *x, t_floatarg f) {
    x->f_polyphony = f;
}

void rngs_tilde_model(t_rngs_tilde *x, t_floatarg f) {
    x->f_model= f;
}

void rngs_tilde_fm(t_rngs_tilde *x, t_floatarg f) {
    x->f_fm= f;
}

void rngs_tilde_trig(t_rngs_tilde *x) {
    x->f_trig = 1.0f;
}

void rngs_tilde_chord(t_rngs_tilde *x, t_floatarg f) {
    x->f_chord = f;
}

void rngs_tilde_gen_strum(t_rngs_tilde *x, t_floatarg f) {
    x->f_internal_strum = f;
}

void rngs_tilde_gen_exciter(t_rngs_tilde *x, t_floatarg f) {
    x->f_internal_exciter = f;
}

void rngs_tilde_gen_note(t_rngs_tilde *x, t_floatarg f) {
    x->f_internal_note = f;
}



// puredata methods implementation - end
