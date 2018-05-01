#include "m_pd.h"

static t_class *voices_class;

typedef struct voice{
    t_float v_pitch;
    int v_used;
    unsigned long v_count;
}t_voice;

typedef struct voices{
    t_object x_obj;
    int x_n;
    int x_offset;
    int x_retrig;
    t_voice *x_vec;
    t_float x_vel;
    unsigned long x_count;
    int x_steal;
}t_voices;

static void voices_noteon(t_voices *x, t_float f){
    t_voice *v, *first_used = 0, *first_unused = 0;
    unsigned int used_idx = 0, unused_idx = 0;
    int i;
    unsigned int count_used = 0xffffffff, count_unused = 0xffffffff;
// find first_used (on) / first_unused (off)
    for(v = x->x_vec, i = 0; i < x->x_n; v++, i++){
        if(v->v_used && v->v_count < count_used) // find first used (on) voice
            first_used = v, count_used = (unsigned int)v->v_count, used_idx = i;
        else if(!v->v_used && v->v_count < count_unused) // find first unused (off) voice
            first_unused = v, count_unused = (unsigned int)v->v_count, unused_idx = i;
    }
    if(first_unused){ // if there's an unused voice, use it
        first_unused->v_used = 1; // mark as used
        first_unused->v_pitch = f; // set pitch
        first_unused->v_count = x->x_count++; // increase counter
        //  post("first_unused->v_count = %d", first_unused->v_count);
        t_atom at[3];
        SETFLOAT(at, unused_idx + x->x_offset);                // voice number
        SETFLOAT(at+1, first_unused->v_pitch);                 // pitch
        SETFLOAT(at+2, x->x_vel);                              // Note-On velocity
        outlet_list(x->x_obj.ob_outlet, &s_list, 3, at);
    }
    else if(x->x_steal && first_used){ // there's no unused voice, if "steal", steal first used voice
    // "free" first used (send note off)
        t_atom at1[3];
// at1 / at2 ?
        SETFLOAT(at1, used_idx + x->x_offset);                 // voice number
        SETFLOAT(at1+1, first_used->v_pitch);                  // pitch
        SETFLOAT(at1+2, 0);                                    // Note-Off
        outlet_list(x->x_obj.ob_outlet, &s_list, 3, at1);
    // set new first on, output note on and increase counter
        first_used->v_pitch = f; // set new pitch
        t_atom at2[3];
        SETFLOAT(at2, used_idx + x->x_offset);                 // voice number
        SETFLOAT(at2+1, first_used->v_pitch);                  // pitch
        SETFLOAT(at2+2, x->x_vel);                             // Note-On velocity
        outlet_list(x->x_obj.ob_outlet, &s_list, 3, at2);
        first_used->v_count = x->x_count++; // increase counter
//        post("first_used->v_count = %d", first_used->v_count);
    }
}

static void voices_float(t_voices *x, t_float f){
    int i;
    t_voice *v;
    if(x->x_vel > 0){ // Note-on
        if(x->x_retrig){ // retrigger mode
            t_voice *prev = 0;
            unsigned int prev_idx = 0;
            for(v = x->x_vec, i = 0; i < x->x_n; v++, i++){
                if(v->v_used && v->v_pitch == f){ // find previous pitch
                    prev = v, prev_idx = i;
                    break;
                }
            }
            if(prev){ // note already in voice allocation, retrigger
                t_atom at[3];
                SETFLOAT(at, prev_idx + x->x_offset);       // voice number
                SETFLOAT(at+1, f);                          // pitch
                SETFLOAT(at+2, x->x_vel);                   // Note-On velocity
                outlet_list(x->x_obj.ob_outlet, &s_list, 3, at);
            }
            else // new note
                voices_noteon(x, f);
        }
        else // regular mode
            voices_noteon(x, f);
    }
    else{ // Note off
        t_voice *used_pitch = 0;
        unsigned int used_idx = 0, count = 0xffffffff;
        for(v = x->x_vec, i = 0; i < x->x_n; v++, i++){ // search pitch in oldest entry
            if(v->v_used && v->v_pitch == f && v->v_count < count)
                used_pitch = v, count = (unsigned int)v->v_count, used_idx = i;
        }
        if(used_pitch){ // if pitch found, send note-off and free voice
            t_atom at[3];
            SETFLOAT(at, used_idx + x->x_offset);             // voice number
            SETFLOAT(at+1, used_pitch->v_pitch);              // pitch
            SETFLOAT(at+2, 0);                                // Note-Off
            outlet_list(x->x_obj.ob_outlet, &s_list, 3, at);
            used_pitch->v_used = used_pitch->v_pitch = 0; // free voice
//            post("used_pitch->v_count = %d", used_pitch->v_count);
        }
// now check if all are unused, if so: reset counter
        int used = 0;
        for(v = x->x_vec, i = 0; i < x->x_n; v++, i++){ // search used voices
            if(v->v_used){
                used = 1;
                break;
            }
        }
        if(!used && x->x_count){ // if all are unused, reset counter
            for(v = x->x_vec, i = x->x_n; i--; v++)
                v->v_count = 0;
            x->x_count = 0;
//                post("reset counter");
        }
    }
}

static void voices_offset(t_voices *x, t_float f){
    x->x_retrig = f != 0;
}

static void voices_steal(t_voices *x, t_float f){
    x->x_offset = (int)f;
}

static void voices_retrig(t_voices *x, t_float f){
    x->x_retrig = f != 0;
}

static void voices_flush(t_voices *x){
    t_voice *v;
    int i;
    for(i = 0, v = x->x_vec; i < x->x_n; i++, v++){
        if(v->v_used){
            t_atom at[3];
            SETFLOAT(at, i + x->x_offset);                  // voice number
            SETFLOAT(at+1, v->v_pitch);                     // pitch
            SETFLOAT(at+2, 0);                              // Note-Off
            outlet_list(x->x_obj.ob_outlet, &s_list, 3, at);
            v->v_used = 0;
        }
        v->v_count = v->v_pitch = 0;
    }
    x->x_count = 0;
}

static void voices_clear(t_voices *x){
    t_voice *v;
    int i;
    for(v = x->x_vec, i = x->x_n; i--; v++)
        v->v_pitch = v->v_used = v->v_count = 0; // zero voices
    x->x_count = 0;
}

static void voices_free(t_voices *x){
    freebytes(x->x_vec, x->x_n * sizeof (*x->x_vec));
}

static void *voices_new(t_float f1, t_float f2, t_float f3, t_float f4){
    t_voices *x = (t_voices *)pd_new(voices_class);
    t_voice *v;
    int i, n = (int)f1;
    x->x_steal = (f2 != 0);
    x->x_offset = (int)f3;
    x->x_retrig = f4 != 0;
    x->x_n = n < 1 ? 1 : n;
    x->x_vec = (t_voice *)getbytes(n * sizeof(*x->x_vec));
    for(v = x->x_vec, i = n; i--; v++) // zero voices
        v->v_pitch = v->v_used = v->v_count = 0;
    x->x_vel = x->x_count = 0;
    floatinlet_new(&x->x_obj, &x->x_vel);
    outlet_new(&x->x_obj, &s_list);
    return(x);
}

void voices_setup(void){
    voices_class = class_new(gensym("voices"), (t_newmethod)voices_new,
            (t_method)voices_free, sizeof(t_voices), 0, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addfloat(voices_class, voices_float);
    class_addmethod(voices_class, (t_method)voices_offset, gensym("offset"), A_FLOAT, 0);
    class_addmethod(voices_class, (t_method)voices_steal, gensym("steal"), A_FLOAT, 0);
    class_addmethod(voices_class, (t_method)voices_retrig, gensym("retrig"), A_FLOAT, 0);
    class_addmethod(voices_class, (t_method)voices_flush, gensym("flush"), 0);
    class_addmethod(voices_class, (t_method)voices_clear, gensym("clear"), 0);
}
