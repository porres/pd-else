// Porres 2017-2018

#include "m_pd.h"
#include "s_stuff.h"
#include "g_canvas.h"
#include <string.h>

static t_class *sr_class;

typedef struct _settings{
    int naudioindev, audioindev[MAXAUDIOINDEV], chindev[MAXAUDIOINDEV];
    int naudiooutdev, audiooutdev[MAXAUDIOOUTDEV], choutdev[MAXAUDIOOUTDEV];
    int rate, advance, callback, blocksize;
}t_settings;

typedef struct _sr{
    t_object    x_obj;
    t_float     x_sr;
    int         x_khz;
    int         x_period;
    t_settings  x_settings;
}t_sr;

///////////////////////////////////////////////////////////////////////////////////////////

static void audio_settings(int *pnaudioindev, int *paudioindev, int *pchindev, int *pnaudiooutdev,
    int *paudiooutdev, int *pchoutdev, int *prate, int *padvance, int *pcallback, int *pblocksize){
        sys_get_audio_params(pnaudioindev , paudioindev , pchindev, pnaudiooutdev,
            paudiooutdev, pchoutdev, prate, padvance, pcallback, pblocksize);
}

static void sr_apply(t_sr *x){
    t_atom av [2*MAXAUDIOINDEV + 2*MAXAUDIOOUTDEV + 3];
    int ac = 2*MAXAUDIOINDEV + 2*MAXAUDIOOUTDEV + 3;
    int i = 0;
    for(i = 0; i < MAXAUDIOINDEV; i++){
        SETFLOAT(av+i + 0*MAXAUDIOINDEV, (t_float)(x->x_settings.audioindev[i]));
        SETFLOAT(av+i + 1*MAXAUDIOINDEV, (t_float)(x->x_settings.chindev[i]));
    }
    for(i = 0; i < MAXAUDIOOUTDEV; i++){
        SETFLOAT(av+i + 2*MAXAUDIOINDEV + 0*MAXAUDIOOUTDEV, (t_float)(x->x_settings.audiooutdev[i]));
        SETFLOAT(av+i + 2*MAXAUDIOINDEV + 1*MAXAUDIOOUTDEV, (t_float)(x->x_settings.choutdev[i]));
    }
    SETFLOAT(av+2 * MAXAUDIOINDEV + 2*MAXAUDIOOUTDEV + 0, (t_float)(x->x_settings.rate));
    SETFLOAT(av+2 * MAXAUDIOINDEV + 2*MAXAUDIOOUTDEV + 1, (t_float)(x->x_settings.advance));
    SETFLOAT(av+2 * MAXAUDIOINDEV + 2*MAXAUDIOOUTDEV + 2, (t_float)(x->x_settings.callback));
    if(gensym("pd")->s_thing)
        typedmess(gensym("pd")->s_thing, gensym("audio-dialog"), ac, av);
    //  "; pd audio-dialog indev[4]/inch[4]/outdev[4]/outch[4]/rate/advance/callback"
}

static void get_settings(t_settings *setts){
    int i = 0;
    memset(setts, 0, sizeof(t_settings));
    setts->callback = -1;
    audio_settings(&setts->naudioindev,  setts->audioindev,  setts->chindev,
               &setts->naudiooutdev, setts->audiooutdev, setts->choutdev, &setts->rate,
               &setts->advance, &setts->callback,    &setts->blocksize);
    for(i = setts->naudioindev; i < MAXAUDIOINDEV; i++){
        setts->audioindev[i] = 0;
        setts->chindev[i] = 0;
    }
    for(i = setts->naudiooutdev; i < MAXAUDIOOUTDEV; i++){
        setts->audiooutdev[i] = 0;
        setts->choutdev[i] = 0;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

static void sr_set(t_sr *x, t_floatarg f){
    t_int rate = (int)f;
    if(rate > 0){
        x->x_settings.rate = rate;
        sr_apply(x);
    }
}

static void sr_bang(t_sr *x){
    t_float sr = sys_getsr();
    x->x_sr = sr;
    if(x->x_khz)
        sr *= 0.001;
    if(x->x_period)
        sr = 1./sr;
    outlet_float(x->x_obj.ob_outlet, sr);
}

static void sr_hz(t_sr *x){
    x->x_khz = x->x_period = 0;
    sr_bang(x);
}

static void sr_khz(t_sr *x){
    x->x_khz = 1;
    x->x_period = 0;
    sr_bang(x);
}

static void sr_ms(t_sr *x){
    x->x_khz = x->x_period = 1;
    sr_bang(x);
}

static void sr_sec(t_sr *x){
    x->x_khz = 0;
    x->x_period = 1;
    sr_bang(x);
}

static void sr_loadbang(t_sr *x, t_floatarg action){
    if (action == LB_LOAD)
        sr_bang(x);
}

static void sr_dsp(t_sr *x, t_signal **sp){
    t_float sr = sys_getsr();
    if(sr != x->x_sr){
        x->x_sr = sr;
        if(x->x_khz)
            sr *= 0.001;
        if(x->x_period)
            sr = 1./sr;
        outlet_float(x->x_obj.ob_outlet, sr);
    }
}

static void *sr_new(t_symbol *s, int ac, t_atom *av){
    t_sr *x = (t_sr *)pd_new(sr_class);
    get_settings(&x->x_settings);
    x->x_khz = x->x_period = 0;
    if(ac <= 2){
        while(ac){
            if(av->a_type == A_SYMBOL){
                t_symbol *curarg = s; // get rid of warning
                curarg = atom_getsymbolarg(0, ac, av);
                if(!strcmp(curarg->s_name, "-khz"))
                    x->x_khz = 1;
                else if(!strcmp(curarg->s_name, "-ms"))
                    x->x_khz = x->x_period = 1;
                else if(!strcmp(curarg->s_name, "-sec"))
                    x->x_period = 1;
                else
                    goto errstate;
                ac--;
                av++;
            }
            else{
                sr_set(x, atom_getfloatarg(0, ac, av));
                ac--;
                av++;
            }
        }
    }
    else
        goto errstate;
    outlet_new(&x->x_obj, &s_float);
    return (x);
    errstate:
        pd_error(x, "sr~: improper args");
        return NULL;
}

void sr_tilde_setup(void){
    sr_class = class_new(gensym("sr~"),
            (t_newmethod)sr_new, 0, sizeof(t_sr), 0, A_GIMME, 0);
    class_addmethod(sr_class, nullfn, gensym("signal"), 0);
    class_addmethod(sr_class, (t_method)sr_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(sr_class, (t_method)sr_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(sr_class, (t_method)sr_hz, gensym("hz"), 0);
    class_addmethod(sr_class, (t_method)sr_khz, gensym("khz"), 0);
    class_addmethod(sr_class, (t_method)sr_ms, gensym("ms"), 0);
    class_addmethod(sr_class, (t_method)sr_sec, gensym("sec"), 0);
    class_addmethod(sr_class, (t_method)sr_set, gensym("set"), A_DEFFLOAT, 0);
    class_addbang(sr_class, (t_method)sr_bang);
}
