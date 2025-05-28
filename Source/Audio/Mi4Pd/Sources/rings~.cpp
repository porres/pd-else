#include "m_pd.h"

#include "rings/dsp/part.h"
#include "rings/dsp/patch.h"
#include "rings/dsp/strummer.h"
#include "rings/dsp/string_synth_part.h"

inline float constrain(float v, float vMin, float vMax){
    return(std::max<float>(vMin, std::min<float>(vMax, v)));
}

static t_class *rings_class;

typedef struct _rings{
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
    float pitch_offset = 0.0f;
    static const int REVERB_SZ = 32768;
    uint16_t buffer[REVERB_SZ];
}t_rings;

t_int *rings_tperform(t_int *w){
    t_rings *x = (t_rings *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    t_sample *aux = (t_sample *)(w[4]);
    int n = (int)(w[5]);
    size_t size = n;
    for(int i = 0; i < n; i++)
        out[i] = in[i];
    if(n > x->iobufsz){
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
    x->f_trig = 0.0f;
    x->f_polyphony = constrain(1 << int(x->f_polyphony) , 1, rings::kMaxPolyphony);
    if(x->f_polyphony != x->part.polyphony()){
        x->part.set_polyphony(x->f_polyphony);
        x->string_synth.set_polyphony(x->f_polyphony);
    }
    rings::ResonatorModel model = static_cast<rings::ResonatorModel>((int) x->f_model);
    x->f_model = constrain(x->f_model, 0, rings::ResonatorModel::RESONATOR_MODEL_LAST - 1);
    if(model != x->part.model()){
        x->part.set_model(model);
        x->string_synth.set_fx(static_cast<rings::FxType>(model));
    }
    x->part.set_bypass(x->f_bypass > 0.5);
    if(x->f_easter_egg > 0.5){
        for(size_t i = 0; i < size; ++i)
            x->in[i] = in[i];
        x->strummer.Process(NULL, size, &(x->performance_state));
        x->string_synth.Process(x->performance_state, x->patch, in, out, aux, size);
    }
    else{ // Apply noise gate.
        for(size_t i = 0; i < size; ++i){
            float in_sample = in[i];
            float error, gain;
            error = in_sample * in_sample - x->in_level;
            x->in_level += error * (error > 0.0f ? 0.1f : 0.0001f);
            gain = x->in_level <= x->kNoiseGateThreshold ?
                (1.0f / x->kNoiseGateThreshold) * x->in_level : 1.0f;
            x->in[i] = gain * in_sample;
        }
        x->strummer.Process(x->in, size, &(x->performance_state));
        x->part.Process(x->performance_state, x->patch, x->in, out, aux, size);
    }
    return(w+6);
}

void rings_dsp(t_rings *x, t_signal **sp) {
    dsp_add(rings_tperform, 5, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[0]->s_n);
}

void rings_pitch(t_rings *x, t_floatarg f){
    x->f_pitch = f;
}

void rings_transpose(t_rings *x, t_floatarg f){
    x->f_transpose = f;
}

void rings_structure(t_rings *x, t_floatarg f){
    x->f_structure = f;
}

void rings_brightness(t_rings *x, t_floatarg f){
    x->f_brightness = f;
}

void rings_damping(t_rings *x, t_floatarg f){
    x->f_damping = f;
}

void rings_position(t_rings *x, t_floatarg f){
    x->f_position = f;
}

void rings_bypass(t_rings *x, t_floatarg f){
    x->f_bypass = f;
}

void rings_easter_egg(t_rings *x, t_floatarg f){
    x->f_easter_egg = f;
}

void rings_poly(t_rings *x, t_floatarg f){
    x->f_polyphony = f;
}

void rings_model(t_rings *x, t_floatarg f){
    x->f_model = f;
}

void rings_fm(t_rings *x, t_floatarg f){
    x->f_fm = f;
}

void rings_trig(t_rings *x){
    x->f_trig = 1.0f;
}

void rings_chord(t_rings *x, t_floatarg f){
    x->f_chord = f;
}

void rings_gen_strum(t_rings *x, t_floatarg f){
    x->f_internal_strum = f;
}

void rings_gen_exciter(t_rings *x, t_floatarg f){
    x->f_internal_exciter = f;
}

void rings_gen_note(t_rings *x, t_floatarg f){
    x->f_internal_note = f;
}

void rings_free(t_rings *x) {
    delete[] x->in;
    outlet_free(x->x_out_odd);
    outlet_free(x->x_out_even);
}

void *rings_new(t_floatarg){
    t_rings *x = (t_rings *) pd_new(rings_class);
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
    if(sys_getsr() != 48000.0f){
//        post("rings~.pd is designed for 48k, not %f, approximating pitch", sys_getsr());
        if(sys_getsr() == 44100){
            x->pitch_offset = 1.46f;
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
    rings::ResonatorModel model = static_cast<rings::ResonatorModel > ((int)x->f_model);
    x->part.set_model(model);
    x->string_synth.set_fx(static_cast<rings::FxType>(model));
    return(void *)x;
}

extern "C" void rings_tilde_setup(void) {
    rings_class = class_new(gensym("rings~"), (t_newmethod) rings_new,
        (t_method)rings_free, sizeof(t_rings), CLASS_DEFAULT, A_DEFFLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_dsp, gensym("dsp"), A_NULL);
    CLASS_MAINSIGNALIN(rings_class, t_rings, f_dummy);
    class_addmethod(rings_class,(t_method)rings_brightness, gensym("brightness"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_damping, gensym("damping"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_structure, gensym("structure"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_position, gensym("position"),
        A_FLOAT, A_NULL);
    
    class_addmethod(rings_class,(t_method)rings_transpose, gensym("transp"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_fm, gensym("fm"),
        A_FLOAT, A_NULL);
    
    class_addmethod(rings_class,(t_method)rings_poly, gensym("poly"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_model, gensym("model"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_pitch, gensym("pitch"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_bypass, gensym("bypass"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_easter_egg, gensym("easter_egg"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_gen_strum, gensym("gen_strum"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_gen_exciter, gensym("gen_exciter"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_gen_note, gensym("gen_note"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_chord, gensym("chord"),
        A_FLOAT, A_NULL);
    class_addmethod(rings_class,(t_method)rings_trig, gensym("trig"), A_NULL);
}
