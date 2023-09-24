// porres and alex mitchell 2023
// This object is based on the sfizz library and is licensed under a BSD 2-clause license.
// you should find a copy of the BSD license in the 'license' folder of the repository.

#ifdef _WIN32
#include <io.h>
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif

#include <m_pd.h>
#include "../../../shared/elsefile.h"
#include <sfizz.h>
#include <sfizz/import/sfizz_import.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>

static t_class* sfz_class;

typedef struct _sfz{
    t_object        x_obj;
    sfizz_synth_t  *x_synth;
    t_canvas       *x_canvas;
    int             x_midinum;
    float           x_a4;
    int             x_base;
    float           x_ratio;
    float           x_bratio;
    t_binbuf       *x_binbuf;
    char           *x_buf;               // sfz file as a string
    int             x_bufsize;
    char           *x_custom_path;       // custom path for sfz_set
    char           *x_patch_path;        // patch path
    char           *x_filename;
    
    t_elsefile     *x_elsefilehandle;
}t_sfz;

static void sfz_do_open(t_sfz *x, t_symbol *name){
    const char *filename = name->s_name;
    const char *ext = strrchr(filename, '.');
    char realdir[MAXPDSTRING], *realname = NULL;
    int fd;
    if(ext && !strchr(ext, '/')){ // extension already supplied, no default extension
        ext = "";
        fd = canvas_open(x->x_canvas, filename, ext, realdir, &realname, MAXPDSTRING, 0);
        if(fd < 0){
            pd_error(x, "[sfz~]: can't find SFZ file %s", filename);
            return;
        }
    }
    else{
        ext = ".sfz"; // let's try sfz
        fd = canvas_open(x->x_canvas, filename, ext, realdir, &realname, MAXPDSTRING, 0);
        if(fd < 0){ // failed
            pd_error(x, "[sfz~]: can't find SFZ file %s", filename);
            return;
        }
    }
    sys_close(fd);
    if(x->x_filename && strlen(x->x_filename))
        free(x->x_filename);
    x->x_filename = (char*)malloc(strlen(filename));
    strcpy(x->x_filename, filename);
    if(x->x_bufsize){
        free(x->x_buf);
        x->x_bufsize = 0;
    }
    chdir(realdir);
    sfizz_load_or_import_file(x->x_synth, realname, NULL);
}

static void sfz_readhook(t_pd *z, t_symbol *fn, int ac, t_atom *av){
    ac = 0;
    av = NULL;
    sfz_do_open((t_sfz *)z, fn);
}

static void sfz_click(t_sfz *x){
   elsefile_panel_click_open(x->x_elsefilehandle);
}

static void sfz_scala(t_sfz* x, t_symbol *s){
    if(!sfizz_load_scala_file(x->x_synth, s->s_name))
        post("[sfz~] could not load scala file");
}

static void sfz_scale(t_sfz* x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    char scale[2048] = "custom\n";
    char* pscale = &scale[0] + strlen(scale);
    pscale += sprintf(pscale, "%d\n", ac-1);
    for(int i = 1; i < ac; i++)
        pscale += sprintf(pscale, "%f\n", atom_getfloat(av+i));
    if(!sfizz_load_scala_string(x->x_synth, scale))
        post("[sfz~] could not load scale");
}

static void sfz_setfreq(t_sfz* x, t_float f){
    sfizz_set_tuning_frequency(x->x_synth, f);
}

static void sfz_a4(t_sfz* x, t_float f){
    x->x_a4 = f;
    sfz_setfreq(x, x->x_a4 * x->x_ratio * x->x_bratio);
}

static void sfz_transp(t_sfz* x, t_float f){
    x->x_ratio = pow(2, f/1200);
    sfz_setfreq(x, x->x_a4 * x->x_ratio * x->x_bratio);
}

static void sfz_base(t_sfz* x, t_float f){
    x->x_base = (int)f-60;
    x->x_bratio = pow(2, (float)x->x_base/12.);
    sfz_setfreq(x, x->x_a4 * x->x_ratio * x->x_bratio);
}

static void sfz_open(t_sfz *x, t_symbol *s){
    if(s && s != &s_)
        sfz_do_open(x, s);
    else
        elsefile_panel_click_open(x->x_elsefilehandle);
}

static void sfz_set(t_sfz *x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    if(x->x_bufsize)
        freebytes(x->x_buf, x->x_bufsize);
    binbuf_clear(x->x_binbuf);
    binbuf_restore(x->x_binbuf, ac, av);
    binbuf_gettext(x->x_binbuf, &x->x_buf, &x->x_bufsize);
    // Allocate memory to null-terminate the binbuf string
    char* new_buf = (char*)malloc(x->x_bufsize + 1);
    if(new_buf == NULL){
        post("[sfz~] unable to allocate memory to load SFZ string");
        return;
    }
    strncpy(new_buf, x->x_buf, x->x_bufsize);
    new_buf[x->x_bufsize] = '\0';
    freebytes(x->x_buf, x->x_bufsize);
    x->x_buf = new_buf;
    x->x_bufsize = strlen(x->x_buf);
    if(!sfizz_load_string(x->x_synth, x->x_custom_path, x->x_buf))
        post("[sfz~] could not set SFZ string");
}

static void sfz_path(t_sfz *x, t_symbol *path){
    if(path->s_name[0] == '\0'){
        char* patchPath = (char*)malloc(strlen(x->x_patch_path));
        strcpy(patchPath, x->x_patch_path);
        if(x->x_custom_path && strlen(x->x_custom_path))
            free(x->x_custom_path);
        x->x_custom_path = patchPath;
        return;
    }
    char *custom_path_buf;
    if(path->s_name[strlen(path->s_name)] != '/'){ // add trailing slash if it ain't got one
        custom_path_buf = (char*)malloc(strlen(path->s_name) + 2);
        strcpy(custom_path_buf, path->s_name);
        strcat(custom_path_buf, "/");
    }
    else{
        custom_path_buf = (char*)malloc(strlen(path->s_name) + 1);
        strcpy(custom_path_buf, path->s_name);
    }
    if(x->x_custom_path && strlen(x->x_custom_path))
        free(x->x_custom_path);
    x->x_custom_path = custom_path_buf;
}

static void sfz_midiin(t_sfz* x, t_float f){
    int byte = (int)f;
    bool isstatus = (byte & 0x80) != 0;
    int midi[3];
    int midinum = x->x_midinum;
    if(isstatus){
        midi[0] = byte;
        midinum = 1;
    }
    else if(midinum != -1 && midinum < 3)
        midi[midinum++] = byte;
    else
        midinum = -1;
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
            sfizz_send_note_on(x->x_synth, 0, midi[1] - x->x_base, midi[2]);
            break;
        case 0x80: // note off
        noteoff:
            sfizz_send_note_off(x->x_synth, 0, midi[1] - x->x_base, midi[2]);
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

static void sfz_note(t_sfz* x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    if(ac == 2 && av[0].a_type == A_FLOAT && av[1].a_type == A_FLOAT) {
        int key = (int)av[0].a_w.w_float;
        if(key < 0 || key > 127)
            return;
        int vel = av[1].a_w.w_float;
        if(vel > 0)
            sfizz_send_hd_note_on(x->x_synth, 0, key - x->x_base, vel/127.);
        else
            sfizz_send_note_off(x->x_synth, 0, key - x->x_base, 0);
    }
}

static void sfz_ctl(t_sfz* x, t_float f1, t_float f2){
    int cc = (int)f2;
    if(cc < 0 || cc >= MIDI_CC_COUNT)
        return;
    sfizz_send_hdcc(x->x_synth, 0, cc, f1/127.);
}

static void sfz_pgm(t_sfz* x, t_float f1){
    sfizz_send_program_change(x->x_synth, 0, f1);
}

static void sfz_bend(t_sfz* x, t_float f1){
    sfizz_send_pitch_wheel(x->x_synth, 0, f1);
}

static void sfz_touch(t_sfz* x, t_float f1){
    sfizz_send_channel_aftertouch(x->x_synth, 0, f1);
}

static void sfz_polytouch(t_sfz* x, t_float f1, t_float key){
    if(key < 0 || key > 127)
        return;
    sfizz_send_poly_aftertouch(x->x_synth, 0, (int)key, f1);
}

static void sfz_volume(t_sfz* x, t_float f){
    sfizz_set_volume(x->x_synth, f);
}

static void sfz_voices(t_sfz* x, t_float f){
    int numvoices = (int)f;
    numvoices = (numvoices < 1) ? 1 : numvoices;
    sfizz_set_num_voices(x->x_synth, numvoices);
}

static void sfz_panic(t_sfz* x){
    sfizz_all_sound_off(x->x_synth);
}

static void sfz_flush(t_sfz* x){
    for(int i = 0; i <= 127; i++)
        sfizz_send_note_off(x->x_synth, 0, i, 0);
}

static void sfz_info(t_sfz *x){
    post("[sfz~] info -------------------------");
    post("Using SFIZZ version '%s'", SFIZZ_VERSION);
    if(x->x_bufsize)
        post("SFZ string loaded");
    else if(x->x_filename && strlen(x->x_filename))
        post("SFZ file loaded (%s)", x->x_filename);
    else
        post("no SFZ string or file loaded");
    post("custom path loaded = %s", x->x_custom_path);
    post("-------------------------------------");
}

static t_int* sfz_perform(t_int* w){
    t_sfz *x = (t_sfz *)(w[1]);
    t_sample* outputs[2];
    outputs[0] = (t_sample *)(w[2]);
    outputs[1] = (t_sample *)(w[3]);
    t_int n = (t_int)(w[4]);
    sfizz_render_block(x->x_synth, outputs, 2, n);
    return(w+5);
}

static void sfz_dsp(t_sfz* x, t_signal** sp){
    dsp_add(&sfz_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

static void sfz_free(t_sfz* x){
    if(x->x_synth)
        sfizz_free(x->x_synth);
    if(x->x_elsefilehandle)
        elsefile_free(x->x_elsefilehandle);
    binbuf_free(x->x_binbuf);
    if(x->x_custom_path && strlen(x->x_custom_path))
        free(x->x_custom_path);
    if(x->x_filename)
        free(x->x_filename);
    if(x->x_bufsize)
        freebytes(x->x_buf, x->x_bufsize);
}

static void* sfz_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_sfz* x = (t_sfz*)pd_new(sfz_class);
    x->x_elsefilehandle = elsefile_new((t_pd *)x, sfz_readhook, 0);
    x->x_binbuf = binbuf_new();
    x->x_bufsize = 0;
    
    x->x_canvas = canvas_getcurrent();
    char *canvas_dir = (char *)canvas_getdir(x->x_canvas)->s_name;
    x->x_patch_path = (char*)malloc(strlen(canvas_dir) + 2);
    strcpy(x->x_patch_path, canvas_dir);
    x->x_patch_path[strlen(canvas_dir)] = '/';
    x->x_patch_path[strlen(canvas_dir) + 1] = '\0';
    
    x->x_custom_path = (char*)malloc(strlen(x->x_patch_path) + 1);
    strcpy(x->x_custom_path, x->x_patch_path);
    
    x->x_synth = sfizz_create_synth();
    sfizz_set_sample_rate(x->x_synth , sys_getsr());
    sfizz_set_samples_per_block(x->x_synth , sys_getblksize());
    x->x_a4 = 440;
    x->x_base = 0;
    x->x_ratio = x->x_bratio = 1.;
    x->x_filename = NULL;
    if(ac == 1 && av[0].a_type == A_SYMBOL)
        sfz_open(x, av[0].a_w.w_symbol);
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void sfz_tilde_setup(){
    sfz_class = class_new(gensym("sfz~"), (t_newmethod)&sfz_new,
        (t_method)sfz_free, sizeof(t_sfz), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(sfz_class, (t_method)sfz_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(sfz_class, (t_method)sfz_midiin);
    class_addlist(sfz_class, (t_method)sfz_note);
    class_addmethod(sfz_class, (t_method)sfz_note, gensym("note"), A_GIMME, 0);
    class_addmethod(sfz_class, (t_method)sfz_ctl, gensym("ctl"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_pgm, gensym("pgm"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_bend, gensym("bend"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_touch, gensym("touch"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_polytouch, gensym("polytouch"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_panic, gensym("panic"), 0);
    class_addmethod(sfz_class, (t_method)sfz_flush, gensym("flush"), 0);
    class_addmethod(sfz_class, (t_method)sfz_open, gensym("open"), A_DEFSYM, 0);
    class_addmethod(sfz_class, (t_method)sfz_set, gensym("set"), A_GIMME, 0);
    class_addmethod(sfz_class, (t_method)sfz_path, gensym("path"), A_DEFSYM, 0);
    class_addmethod(sfz_class, (t_method)sfz_scala, gensym("scala"), A_DEFSYM, 0);
    class_addmethod(sfz_class, (t_method)sfz_a4, gensym("a4"), A_FLOAT, 0);
//    class_addmethod(sfz_class, (t_method)sfz_tuningstretch, gensym("tuningstretch"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_base, gensym("base"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_scale, gensym("scale"), A_GIMME, 0);
    class_addmethod(sfz_class, (t_method)sfz_transp, gensym("transp"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_volume, gensym("volume"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_voices, gensym("voices"), A_FLOAT, 0);
    class_addmethod(sfz_class, (t_method)sfz_info, gensym("info"), 0);
    class_addmethod(sfz_class, (t_method)sfz_click, gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    elsefile_setup();
}
