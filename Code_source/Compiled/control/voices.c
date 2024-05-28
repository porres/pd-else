// porres 2018-2024

#include "m_pd.h"
#include "else_alloca.h"
#include <stdlib.h>

static t_class *voices_class;

typedef struct _voice{
    struct _voices *v_owner;
    t_clock        *v_clock;
    t_float         v_pitch;
    t_symbol       *v_pitchsym;
    int             v_used;      // if voice is being used (1) or not (0)
    int             v_released;  // if voice is released (1) or not (0)
    unsigned long   v_count;     // serial count id for voice (which was pressed first)
}t_voice;

typedef struct _voices{
    t_object        x_obj;
    t_voice        *x_vec;
    t_outlet       *x_extra;
    unsigned long   x_count;
    int             x_n;
    int             x_retrig;
    int             x_steal;
    float           x_release;
    float           x_offset;
}t_voices;

static void voice_tick(t_voice *v_n){
    v_n->v_used = v_n->v_released = v_n->v_pitch = 0.;
    v_n->v_pitchsym = NULL;
    t_voices *x = v_n->v_owner;
    v_n->v_count = x->x_count++;
}

static void voices_newvoice(t_voices *x, int ac, t_atom *av){
    t_voice *v, *first_used = 0, *first_unused = 0;
    unsigned int used_idx = 0, unused_idx = 0;
    int i;
    unsigned int count_used = 0xffffffff, count_unused = 0xffffffff;
    float pitch = 0;
    t_symbol *pitchsym = NULL;
    if(av->a_type == A_FLOAT)
       pitch = atom_getfloat(av);
    else if(av->a_type == A_SYMBOL)
       pitchsym = atom_getsymbol(av);
    float vel = atom_getfloat(av+1);
    for(v = x->x_vec, i = 0; i < x->x_n; v++, i++){
        if(v->v_used && v->v_count < count_used){ // find first used (on) voice
            first_used = v;
            used_idx = i;
            count_used = (unsigned int)v->v_count; // update min count value
        }
        else if(!v->v_used && v->v_count < count_unused){ // find first unused (off) voice
            first_unused = v;
            unused_idx = i;
            count_unused = (unsigned int)v->v_count;
        }
    }
    if(first_unused){ // if there's an unused voice, use it
        first_unused->v_used = 1; // mark as used
        if(pitchsym != NULL)
           first_unused->v_pitchsym = pitchsym; // set pitch
        else
            first_unused->v_pitch = pitch;      // set pitch
        first_unused->v_count = x->x_count++;   // increase counter
        t_atom* at = ALLOCA(t_atom, ac + 1);
        SETFLOAT(at, unused_idx + x->x_offset); // voice number
        for(i = 0; i < ac; i++){
            if((av+i)->a_type == A_FLOAT)
                SETFLOAT(at+i+1, atom_getfloat(av+i));
            else if((av+i)->a_type == A_SYMBOL)
                SETSYMBOL(at+i+1, atom_getsymbol(av+i));
        }
        outlet_list(x->x_obj.ob_outlet, &s_list, ac+1, at);
        FREEA(at, t_atom, ac + 1);
    }
    else{ // there's no unused voice / all voices are being used
        if(x->x_steal){ // if "steal", steal first used voice
            t_atom at1[3];
            SETFLOAT(at1, used_idx + x->x_offset);    // voice number
            SETFLOAT(at1+1, first_used->v_pitch);     // pitch
            SETFLOAT(at1+2, 0);                       // Note-Off
            outlet_list(x->x_obj.ob_outlet, &s_list, 3, at1);
            // note on
            t_atom* at2 = ALLOCA(t_atom, ac + 1);
            SETFLOAT(at2, used_idx + x->x_offset);    // voice number
            for(i = 0; i < ac; i++){
                if((av+i)->a_type == A_FLOAT)
                    SETFLOAT(at2+i+1, atom_getfloat(av+i));
                else if((av+i)->a_type == A_SYMBOL)
                    SETSYMBOL(at2+i+1, atom_getsymbol(av+i));
            }
            outlet_list(x->x_obj.ob_outlet, &s_list, ac+1, at2);
            FREEA(at2, t_atom, ac + 1);
            first_used->v_released = 0;
            clock_unset(first_used->v_clock);
            if(pitchsym != NULL)
               first_used->v_pitchsym = pitchsym; // set new pitch
            else
               first_used->v_pitch = pitch;       // set new pitch
            first_used->v_count = x->x_count++;   // increase counter
        }
        else{ // don't steal, output in extra outlet
            t_atom at[2];
            if(pitchsym != NULL)
               SETSYMBOL(at, pitchsym);   // pitch
            else
                SETFLOAT(at, pitch);
            SETFLOAT(at+1, vel);          // Note-On velocity
            outlet_list(x->x_extra, &s_list, 2, at);
        }
    }
}

static void voices_list(t_voices *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac < 2)
        return;
    float pitch = 0;
    t_symbol *pitchsym = NULL;
    if(av->a_type == A_FLOAT)
       pitch = atom_getfloat(av);
    else if(av->a_type == A_SYMBOL)
       pitchsym = atom_getsymbol(av);
    float vel = atom_getfloat(av+1);
    int i;
    t_voice *v;
    if(vel > 0){ // Note-on
        if(x->x_retrig == 0) // retrigger mode 0: different output
            voices_newvoice(x, ac, av); // add new note, nothing different
        else{ // retrigger mode 1 & 2
            t_voice *prev = 0;
            unsigned int prev_idx = 0;
            for(v = x->x_vec, i = 0; i < x->x_n; v++, i++){
                if(pitchsym != NULL){
                    if(v->v_used && v->v_pitchsym == pitchsym){ // find previous pitch
                        prev = v, prev_idx = i;
                        break;
                    }
                }
                else if(v->v_used && v->v_pitch == pitch){      // find previous pitch
                    prev = v, prev_idx = i;
                    break;
                }
            }
            if(prev){ // note already in voice allocation
                if(x->x_retrig == 1){ // retrigger on the same voice allocation
                    t_atom* at = ALLOCA(t_atom, ac + 1);
                    SETFLOAT(at, prev_idx + x->x_offset);  // voice number
                    for(i = 0; i < ac; i++){
                        if((av+i)->a_type == A_FLOAT)
                            SETFLOAT(at+i+1, atom_getfloat(av+i));
                        else if((av+i)->a_type == A_SYMBOL)
                            SETSYMBOL(at+i+1, atom_getsymbol(av+i));
                    }
                    outlet_list(x->x_obj.ob_outlet, &s_list, ac+1, at);
                    FREEA(at, t_atom, ac + 1);
                    prev->v_released = 0;
                    clock_unset(prev->v_clock);
                }
                else if(x->x_retrig == 2) // go to extra
                    outlet_list(x->x_extra, &s_list, ac, av);
            }
            else // new note (not in voice allocation)
                voices_newvoice(x, ac, av);
        }
    }
    else{ // Note off (vel = 0)
        t_voice *used_pitch = 0;
        unsigned int used_idx = 0, count = 0xffffffff;
        for(v = x->x_vec, i = 0; i < x->x_n; v++, i++){ // search existing pitch
            int check_pitch = 0;
            if(v->v_pitchsym != NULL && v->v_pitchsym == pitchsym)
                check_pitch = 1;
            else if(v->v_pitchsym == NULL && v->v_pitch == pitch)
                check_pitch = 1;
            if(v->v_used && !v->v_released && check_pitch && v->v_count < count)
                used_pitch = v, count = (unsigned int)v->v_count, used_idx = i;
        }
        if(used_pitch){ // pitch was found in a used and unreleased voice
            // send note-off
            t_atom* at = ALLOCA(t_atom, ac + 1);
            SETFLOAT(at, used_idx + x->x_offset); // voice number
            for(i = 0; i < ac; i++){
                if((av+i)->a_type == A_FLOAT)
                    SETFLOAT(at+i+1, atom_getfloat(av+i));
                else if((av+i)->a_type == A_SYMBOL)
                    SETSYMBOL(at+i+1, atom_getsymbol(av+i));
            }
            outlet_list(x->x_obj.ob_outlet, &s_list, ac+1, at);
            FREEA(at, t_atom, ac + 1);
            if(x->x_release > 0){ // free voice after release time
                clock_delay(used_pitch->v_clock, x->x_release);
                used_pitch->v_released = 1;
            }
            else{
                used_pitch->v_used = used_pitch->v_pitch = 0;
                used_pitch->v_pitchsym = NULL;
                used_pitch->v_count = x->x_count++;
            }
        }
        else // pitch not found, send note-off in extra outlet
            outlet_list(x->x_extra, &s_list, ac, av);
    }
}

static void voices_anything(t_voices *x, t_symbol *s, int ac, t_atom *av){ // custom
    float pitch = 0;
    t_symbol *pitchsym = NULL;
    if(av->a_type == A_FLOAT)
       pitch = atom_getfloat(av);
    else if(av->a_type == A_SYMBOL)
       pitchsym = atom_getsymbol(av);
    t_voice *prev = 0;
    unsigned int prev_idx = 0;
    int i;
    t_voice *v;
    for(v = x->x_vec, i = 0; i < x->x_n; v++, i++){
        if(pitchsym != NULL){
            if(v->v_used && v->v_pitchsym == pitchsym){ // find previous pitch
                prev = v, prev_idx = i;
                break;
            }
        }
        else if(v->v_used && v->v_pitch == pitch){ // find previous pitch
            prev = v, prev_idx = i;
            break;
        }
    }
    if(prev){ // found in a voice allocation
        t_atom* at = ALLOCA(t_atom, ac+2);
        SETFLOAT(at, prev_idx + x->x_offset); // voice number
        SETSYMBOL(at+1, s);                   // message type
        for(i = 0; i < ac; i++){
            if((av+i)->a_type == A_FLOAT)
                SETFLOAT(at+i+2, atom_getfloat(av+i));
            else if((av+i)->a_type == A_SYMBOL)
                SETSYMBOL(at+i+2, atom_getsymbol(av+i));
        }
        outlet_list(x->x_obj.ob_outlet, &s_list, ac+2, at);
        FREEA(at, t_atom, ac+2);
    }
    else // go to extra
        outlet_list(x->x_extra, s, ac, av);
}

static void voices_offset(t_voices *x, t_float f){
    x->x_offset = (int)f;
}

static void voices_steal(t_voices *x, t_float f){
    x->x_steal = (f != 0);
}

static void voices_release(t_voices *x, t_float f){
    x->x_release = f;
}

static void voices_retrig(t_voices *x, t_float f){
    x->x_retrig = f < 0 ? 0 : f > 2 ? 2 : (int)f;
}

static void voices_flush(t_voices *x){
    t_voice *v;
    int i;
    for(i = 0, v = x->x_vec; i < x->x_n; i++, v++){
        if(v->v_used){
            t_atom at[3];
            SETFLOAT(at, i + x->x_offset);                // voice number
            if(v->v_pitchsym != NULL)
                SETSYMBOL(at+1, v->v_pitchsym);           // pitch
            else
                SETFLOAT(at+1, v->v_pitch);               // pitch
            SETFLOAT(at+2, 0);                            // Note-Off
            outlet_list(x->x_obj.ob_outlet, &s_list, 3, at);
            clock_unset(v->v_clock);
            v->v_used = v->v_count = v->v_pitch = 0;      // fuck release time
            v->v_pitchsym = NULL;
        }
    }
    x->x_count = 0;
}

static void voices_n(t_voices *x, t_float f){
    int n = (int)f < 1 ? 1 : (int)f;
    if(n == x->x_n)
        return;
    voices_flush(x);
    if(x->x_vec)
        freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
    x->x_vec = (t_voice *)getbytes(n * sizeof(*x->x_vec));
    x->x_n = n;
    int i;
    t_voice *v;
    for(v = x->x_vec, i = x->x_n; i--; v++){ // initialize voices
        v->v_pitch = v->v_used = v->v_count = 0;
        v->v_pitchsym = NULL;
        v->v_clock = clock_new(v, (t_method)voice_tick);
        v->v_owner = x;
    }
}

static void voices_clear(t_voices *x){
    t_voice *v;
    int i;
    for(v = x->x_vec, i = x->x_n; i--; v++){
        v->v_pitch = v->v_used = v->v_released = v->v_count = 0; // zero voices
        v->v_clock = clock_new(v, (t_method)voice_tick);
        v->v_pitchsym = NULL;
    }
    x->x_count = 0;
}

static void voices_free(t_voices *x){
    t_voice *v;
    int i;
    for(v = x->x_vec, i = x->x_n; i--; v++)
        clock_free(v->v_clock);
    freebytes(x->x_vec, x->x_n * sizeof (*x->x_vec));
}

static void *voices_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_voices *x = (t_voices *)pd_new(voices_class);
    x->x_offset = 0;
    int retrig = 0;
    int n = 1;
    x->x_steal = 0;
    float release = 0;
    x->x_count = 0;
/////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_SYMBOL && !argnum){
            t_symbol *cursym = atom_getsymbolarg(0, ac, av);
            if(cursym == gensym("-retrig")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    retrig = (int)atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(cursym ==  gensym("-rel")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    release = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(cursym ==  gensym("-n")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    n = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(cursym ==  gensym("-offset")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_offset = atom_getintarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(cursym ==  gensym("-steal")){
                x->x_steal = 1;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else if(av->a_type == A_FLOAT){
            switch(argnum){
                case 0:
                    n = (int)atom_getfloatarg(0, ac, av);
                    break;
                case 1:
                    x->x_steal = atom_getfloatarg(0, ac, av) != 0;
                    break;
                default:
                    break;
            };
            ac--, av++;
            argnum++;
        }
        else
            goto errstate;
    };
/////////////////////////////////////////////////////////////////////////////////
    x->x_release = release < 0 ? 0 : release;
    x->x_retrig = retrig < 0 ? 0 : retrig > 2 ? 2 : retrig;
    x->x_n = n < 1 ? 1 : n;
    x->x_vec = (t_voice *)getbytes(n * sizeof(*x->x_vec));
    int i;
    t_voice *v;
    for(v = x->x_vec, i = n; i--; v++){ // initialize voices
        v->v_pitch = v->v_used = v->v_count = 0;
        v->v_pitchsym = NULL;
        v->v_clock = clock_new(v, (t_method)voice_tick);
        v->v_owner = x;
    }
    floatinlet_new(&x->x_obj, &x->x_release);
    outlet_new(&x->x_obj, &s_list);
    x->x_extra = outlet_new((t_object *)x, &s_list);
    return(x);
errstate:
    pd_error(x, "[voices]: improper args");
    return(NULL);
}

void voices_setup(void){
    voices_class = class_new(gensym("voices"), (t_newmethod)voices_new,
        (t_method)voices_free, sizeof(t_voices), 0, A_GIMME, 0);
    class_addlist(voices_class, voices_list);
    class_addanything(voices_class, voices_anything);
    class_addmethod(voices_class, (t_method)voices_offset, gensym("offset"), A_FLOAT, 0);
    class_addmethod(voices_class, (t_method)voices_steal, gensym("steal"), A_FLOAT, 0);
    class_addmethod(voices_class, (t_method)voices_retrig, gensym("retrig"), A_FLOAT, 0);
    class_addmethod(voices_class, (t_method)voices_release, gensym("rel"), A_FLOAT, 0);
    class_addmethod(voices_class, (t_method)voices_n, gensym("n"), A_FLOAT, 0);
    class_addmethod(voices_class, (t_method)voices_flush, gensym("flush"), 0);
    class_addmethod(voices_class, (t_method)voices_clear, gensym("clear"), 0);
}
