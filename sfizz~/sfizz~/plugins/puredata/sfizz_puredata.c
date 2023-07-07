// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#ifdef _WIN32
#include <io.h>
#endif

#include <m_pd.h>
#include <sfizz.h>
#include <sfizz/import/sfizz_import.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


static t_class* sfz_class;

typedef struct _sfz{
    t_object        x_obj;
    sfizz_synth_t  *x_synth;
    t_canvas       *x_canvas;
    int             x_midinum;
    t_outlet       *outputs[2];
}t_sfz;

static t_float clamp01(t_float x){
    x = (x > 0) ? x : 0;
    x = (x < 1) ? x : 1;
    return(x);
}

static t_float clampB1(t_float x){
    x = (x > -1) ? x : -1;
    x = (x < 1) ? x : 1;
    return(x);
}

static void sfz_open(t_sfz *x, t_symbol *name){
    const char *filename = name->s_name;
    const char *ext = strrchr(filename, '.');
    char realdir[MAXPDSTRING], *realname = NULL;
    int fd;
    if(ext && !strchr(ext, '/')){ // extension already supplied, no default extension
        ext = "";
        fd = canvas_open(x->x_canvas, filename, ext, realdir, &realname, MAXPDSTRING, 0);
        if(fd < 0){
            pd_error(x, "[sfz~]: can't find soundfont %s", filename);
            return;
        }
    }
    else{
        ext = ".sfz"; // let's try sfz
        fd = canvas_open(x->x_canvas, filename, ext, realdir, &realname, MAXPDSTRING, 0);
        if(fd < 0){ // failed
            pd_error(x, "[sfz~]: can't find soundfont %s", filename);
            return;
        }
    }
    sys_close(fd);
    chdir(realdir);
    sfizz_load_or_import_file(x->x_synth, realname, NULL);
}

static void sfz_note(t_sfz* x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    if(ac == 2 && av[0].a_type == A_FLOAT && av[1].a_type == A_FLOAT) {
        int key = (int)av[0].a_w.w_float;
        if(key < 0 || key > 127)
            return;
        int vel = av[1].a_w.w_float;
        if(vel > 0)
            sfizz_send_note_on(x->x_synth, 0, key, vel);
        else
            sfizz_send_note_off(x->x_synth, 0, key, 0);
    }
}

static void sfz_midiin(t_sfz* x, t_float f){
    int byte = (int)f;
    bool isstatus = (byte & 0x80) != 0;
    int midi[3];
//    int* midi = x->midi;
    int midinum = x->x_midinum;
    //
    if(isstatus){
        midi[0] = byte;
        midinum = 1;
    }
    else if(midinum != -1 && midinum < 3)
        midi[midinum++] = byte;
    else
        midinum = -1;
    //
    switch(midinum){
    case 2:
        switch(midi[0] & 0xf0){
        case 0xd0: // channel aftertouch
            sfizz_send_channel_aftertouch(x->x_synth, 0, midi[1]);
            break;
        }
        break;
    case 3:
        switch(midi[0] & 0xf0){
        case 0x90: // note on
            if(midi[2] == 0)
                goto noteoff;
            sfizz_send_note_on(x->x_synth, 0, midi[1], midi[2]);
            break;
        case 0x80: // note off
        noteoff:
            sfizz_send_note_off(x->x_synth, 0, midi[1], midi[2]);
            break;
        case 0xb0: // controller
            sfizz_send_cc(x->x_synth, 0, midi[1], midi[2]);
            break;
        case 0xa0: // key aftertouch
            sfizz_send_poly_aftertouch(x->x_synth, 0, midi[1], midi[2]);
            break;
        case 0xe0: // pitch bend
            sfizz_send_pitch_wheel(x->x_synth, 0, (midi[1] + (midi[2] << 7)) - 8192);
            break;
        }
        break;
    }
    x->x_midinum = midinum;
}

static void sfz_cc(t_sfz* x, t_float f1, t_float f2){
    int cc = (int)f2;
    if(cc < 0 || cc >= MIDI_CC_COUNT)
        return;
    sfizz_automate_hdcc(x->x_synth, 0, (int)cc, clamp01(f1/127));
}

static void sfz_bend(t_sfz* x, t_float f1){
    sfizz_send_pitch_wheel(x->x_synth, 0, clampB1(f1));
}

static void sfz_touch(t_sfz* x, t_float f1){
    sfizz_send_channel_aftertouch(x->x_synth, 0, clamp01(f1));
}

static void sfz_polytouch(t_sfz* x, t_float f1, t_float key){
    if(key < 0 || key > 127)
        return;
    sfizz_send_poly_aftertouch(x->x_synth, 0, (int)key, clamp01(f1));
}

static void sfz_tuningfreq(t_sfz* x, t_float f){
    sfizz_set_tuning_frequency(x->x_synth, f);
}

static void sfz_voices(t_sfz* x, t_float f){
    int numvoices = (int)f;
    numvoices = (numvoices < 1) ? 1 : numvoices;
    sfizz_set_num_voices(x->x_synth, numvoices);
}

static void sfz_panic(t_sfz* x){
    sfizz_all_sound_off(x->x_synth);
}

static void sfz_version(t_sfz *x){
    (void)x;
    post("[sfz~] uses sfizz version '%s'", SFIZZ_VERSION);
}

static t_int* sfz_perform(t_int* w){
    t_sfz* x;
    t_sample* outputs[2];
    t_int nframes;
    w++;
    x = (t_sfz*)*w++;
    outputs[0] = (t_sample*)*w++;
    outputs[1] = (t_sample*)*w++;
    nframes = (t_int)*w++;
    sfizz_render_block(x->x_synth, outputs, 2, nframes);
    return(w);
}

static void sfz_dsp(t_sfz* x, t_signal** sp){
    dsp_add(&sfz_perform, 4, (t_int)x,
        (t_int)sp[0]->s_vec, (t_int)sp[1]->s_vec, (t_int)sp[0]->s_n);
}

static void sfz_free(t_sfz* x){
    if(x->x_synth)
        sfizz_free(x->x_synth);
    if(x->outputs[0])
        outlet_free(x->outputs[0]);
    if(x->outputs[1])
        outlet_free(x->outputs[1]);
}

static void* sfz_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_sfz* x = (t_sfz*)pd_new(sfz_class);
    x->x_canvas = canvas_getcurrent();
    x->outputs[0] = outlet_new(&x->x_obj, &s_signal);
    x->outputs[1] = outlet_new(&x->x_obj, &s_signal);
    sfizz_synth_t* synth = sfizz_create_synth();
    x->x_synth = synth;
    sfizz_set_sample_rate(synth, sys_getsr());
    sfizz_set_samples_per_block(synth, sys_getblksize());
    sfizz_load_string(x->x_synth, "default.sfz", "<region>sample=*sine");
    if(ac == 1 && av[0].a_type == A_SYMBOL)
        sfz_open(x, av[0].a_w.w_symbol);
    return(x);
}

#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
void sfz_tilde_setup(){
    sfz_class = class_new(gensym("sfz~"), (t_newmethod)&sfz_new,
        (t_method)sfz_free, sizeof(t_sfz), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(sfz_class, (t_method)sfz_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(sfz_class, (t_method)sfz_midiin);
    class_addlist(sfz_class, (t_method)sfz_note);
    class_addmethod(sfz_class, (t_method)sfz_note, gensym("note"), A_GIMME, 0);
    class_addmethod(sfz_class, (t_method)sfz_cc, gensym("ctl"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_bend, gensym("bend"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_touch, gensym("touch"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_polytouch, gensym("polytouch"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_open, gensym("open"), A_DEFSYM, 0);
    class_addmethod(sfz_class, (t_method)sfz_voices, gensym("voices"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_tuningfreq, gensym("tuningfreq"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_panic, gensym("panic"), 0);
    class_addmethod(sfz_class, (t_method)sfz_version, gensym("version"), 0);
}
