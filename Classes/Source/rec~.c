// based on cyclone's record~

#include "m_pd.h"
#include "m_imp.h"
#include "buffer.h"
#include <string.h>

#define REC_MAXBD 1E+32 // cheap higher bound for boundary points
#define REC_DRAW_PERIOD  1000.  // draw period

typedef struct _rec{
    t_object    x_obj;
    t_buffer   *x_buffer;
    t_float     x_f; // dummy input float
    t_float    *x_gate_vec; // gate signal vector
    t_float     x_last_gate;
    t_int         x_continue_flag;
    t_int         x_loop_flag;
    t_int         x_phase;       // write head
    t_float     x_sync; // output sync value
    t_clock    *x_clock;
    double      x_clocklasttick;
    t_int         x_isrunning;
    t_int         x_newrun;
    t_float     x_start; // start position (in ms) vector
    t_float     x_end; // endposition (in ms) vec
    t_outlet    *x_outlet;
    t_float     x_ksr;
    t_int         x_numchans;
    t_float   **x_ivecs; // input vectors
    t_float    *x_ovec; // signal output
}t_rec;

static t_class *rec_class;

static void rec_tick(t_rec *x){ // Redraw!
    double timesince = clock_gettimesince(x->x_clocklasttick);
    if(timesince >= REC_DRAW_PERIOD){
        buffer_redraw(x->x_buffer);
        x->x_clocklasttick = clock_getlogicaltime();
    }
    else
        clock_delay(x->x_clock, REC_DRAW_PERIOD - timesince);
}

static void rec_set(t_rec *x, t_symbol *s){
    buffer_setarray(x->x_buffer, s );
}

static void rec_reset(t_rec *x){
    if(x->x_sync > 0)
        x->x_isrunning = 1; // ???
    x->x_start = 0;
    x->x_end = (t_float)x->x_buffer->c_npts/x->x_ksr; //array size in samples
}

static int rec_startpoint(t_rec *x, t_floatarg f){
    t_int npts = x->x_buffer->c_npts;
    t_int startindex = f < REC_MAXBD ? (int)(f * x->x_ksr) : SHARED_INT_MAX;
    if(startindex < 0)
		startindex = 0;
    else if(startindex >= npts) // make it the last index addressable
		startindex = npts - 1;
    return startindex;
}

static int rec_endpoint(t_rec *x, t_floatarg f){
    t_int endindex = f < REC_MAXBD ? (int)(f * x->x_ksr) : SHARED_INT_MAX;
    t_int npts = x->x_buffer->c_npts;
    if(endindex < 0)
        endindex = 0;
    if(endindex >= npts) // if bigger than the end, set it to the end
		endindex = npts;
    return endindex;
}

static void rec_continue(t_rec *x, t_floatarg f){
	x->x_continue_flag = (f != 0);
}

static void rec_loop(t_rec *x, t_floatarg f){
    x->x_loop_flag = (f != 0);
}

static void rec_rec(t_rec *x){
    x->x_isrunning = x->x_newrun = 1;
}

static void rec_stop(t_rec *x){
    if(!x->x_continue_flag)
        x->x_phase = 0.;
    x->x_isrunning = x->x_sync = 0;
    buffer_redraw(x->x_buffer);
}

static t_int *rec_perform(t_int *w){
    t_rec *x = (t_rec *)(w[1]);
    t_buffer * c = x->x_buffer;
    t_int nch = c->c_numchans;
    t_int nblock = (int)(w[2]);
    t_float *gatein = x->x_gate_vec;
    t_float *out = x->x_ovec;
    t_int phase, range, i, j;
    buffer_validate(c, 0);
    t_float last_gate = x->x_last_gate;
    clock_delay(x->x_clock, 0);
    for(i = 0; i < nblock; i++){
        t_float gate = gatein[i];
        if(gate != 0 && last_gate == 0)
            rec_rec(x);
        else if(gate == 0 && last_gate != 0)
            rec_stop(x);
        last_gate = gate;
        t_float startms = x->x_start;
        t_float endms = x->x_end;
        if((startms < endms) && c->c_playable && x->x_isrunning){
            t_int startsamp = rec_startpoint(x, startms);
            t_int endsamp = rec_endpoint(x, endms);
            range = endsamp - startsamp;
            if(x->x_newrun == 1 && x->x_continue_flag == 0){ // continue shouldn't reset phase
                x->x_newrun = 0;
                x->x_phase = startsamp;
                x->x_sync = 0.;
            };
            phase = x->x_phase;
            if(phase >= endsamp){ // boundscheck (might've changed when paused)
                if(x->x_loop_flag == 1)
                    phase = startsamp;
                else{ // stop
                    x->x_isrunning = 0;
                    x->x_sync = 1; // maybe redundant???
                };
            buffer_redraw(x->x_buffer);
            };
            if(phase < startsamp)
                phase = startsamp;
            if(x->x_isrunning == 1){ // if we're still running after boundschecking
                for(j = 0; j < nch; j++){
                    t_word *vp = c->c_vectors[j];
                    t_float *insig = x->x_ivecs[j];
                    if(vp)
                        vp[phase].w_float = insig[i];
                };
                x->x_sync = (t_float)(phase - startsamp)/(t_float)range;
                phase++;
                x->x_phase = phase;
             };
        };
        out[i] = x->x_sync;
    };
    x->x_last_gate = last_gate;
    return(w+3);
}

static void rec_dsp(t_rec *x, t_signal **sp){
    buffer_checkdsp(x->x_buffer);
    x->x_ksr= sp[0]->s_sr * 0.001;
    int i, nblock = sp[0]->s_n;
    t_signal **sigp = sp;
    x->x_gate_vec = (*sigp++)->s_vec; // first sig is the gate input
    for(i = 0; i < x->x_numchans; i++) // input vectors for each channel
        *(x->x_ivecs+i) = (*sigp++)->s_vec;
    x->x_ovec = (*sigp++)->s_vec; // output
    dsp_add(rec_perform, 2, x, nblock);
}

static void rec_free(t_rec *x){
    buffer_free(x->x_buffer);
    outlet_free(x->x_outlet);
    freebytes(x->x_ivecs, x->x_numchans * sizeof(*x->x_ivecs));
    if(x->x_clock)
        clock_free(x->x_clock);
}

static void rec_start(t_rec *x, t_float start){
    x->x_start = start < 0 ? 0 : start;
}

static void rec_end(t_rec *x, t_float end){
    t_float arraysmp = (t_float)x->x_buffer->c_npts;
    if(end < 0){ // if end not set
        if(arraysmp > 0) // if nonzero array, set to arraylen in ms
            end = (arraysmp/x->x_ksr);
        else // default to max size of float (defaults to size of array with boundschecking
            end = REC_MAXBD;
    };
    x->x_end = end;
}

static void *rec_new(t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    t_rec *x = (t_rec *)pd_new(rec_class);
    x->x_ksr = (float)sys_getsr() * 0.001;
    x->x_last_gate = x->x_newrun = x->x_isrunning = x->x_sync = x->x_phase = 0;
    t_float numchan = 1;
    t_float continue_flag = 0;
    t_float loop_flag = 0;
    t_float start = 0;
    t_float end = -1;
    t_int nameset = 0; // flag if name is set
    t_symbol *name = NULL;
    if(ac > 0 && av->a_type == A_SYMBOL){
        if(!nameset){ // if name not passed so far, count arg as array name
            name = atom_getsymbolarg(0, ac, av);
            ac--;
            av++;
            nameset = 1;
        }
    };
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            t_symbol * curarg = atom_getsymbolarg(0, ac, av);
            if(!strcmp(curarg->s_name, "-continue")){
                if(ac >= 2){
                    continue_flag = atom_getfloatarg(1, ac, av);
                    ac-=2;
                    av+=2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(curarg->s_name, "-loop")){
                if(ac >= 2){
                    loop_flag = atom_getfloatarg(1, ac, av);
                    ac-=2;
                    av+=2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(curarg->s_name, "-start")){
                if(ac >= 2){
                    start = atom_getfloatarg(1, ac, av);
                    ac-=2;
                    av+=2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(curarg->s_name, "-end")){
                if(ac >= 2){
                    end = atom_getfloatarg(1, ac, av);
                    ac-=2;
                    av+=2;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else if(av->a_type == A_FLOAT && nameset){
            numchan = atom_getfloatarg(0, ac, av);
            ac--;
            av++;
        }
        else
            goto errstate;
    };
    int chn_n = (int)numchan > 64 ? 64 : (int)numchan;
    x->x_buffer = buffer_init((t_class *)x, name, chn_n, 0);
    t_buffer * c = x->x_buffer;
    if(c){ // set channels and array sizes
        x->x_numchans = c->c_numchans;
        t_float arraysmp = (t_float)c->c_npts;
        x->x_ivecs = getbytes(x->x_numchans * sizeof(*x->x_ivecs)); // allocate in vectors
        x->x_start = start < 0 ? 0 : start;
        if(end < 0) // if end not set, set to array size in ms or max float size
            end = arraysmp > 0 ? arraysmp/x->x_ksr : REC_MAXBD;
        x->x_end = end;
        buffer_setminsize(x->x_buffer, 2);
        x->x_continue_flag = (continue_flag != 0);
        x->x_loop_flag = (loop_flag != 0);
        x->x_clock = clock_new(x, (t_method)rec_tick);
        x->x_clocklasttick = clock_getlogicaltime();
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        for(t_int i = 1; i < x->x_numchans; i++)
            inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        x->x_outlet = outlet_new(&x->x_obj, gensym("signal"));
    };
    return(x);
errstate:
    post("[rec~]: improper args");
    return NULL;
}

void rec_tilde_setup(void){
    rec_class = class_new(gensym("rec~"), (t_newmethod)rec_new, (t_method)rec_free,
			     sizeof(t_rec), CLASS_DEFAULT, A_GIMME, 0);
    CLASS_MAINSIGNALIN(rec_class, t_rec, x_f);
    class_addmethod(rec_class, (t_method)rec_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(rec_class, (t_method)rec_continue, gensym("continue"), A_FLOAT, 0);
    class_addmethod(rec_class, (t_method)rec_loop, gensym("loop"), A_FLOAT, 0);
    class_addmethod(rec_class, (t_method)rec_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(rec_class, (t_method)rec_reset, gensym("reset"), 0);
    class_addmethod(rec_class, (t_method)rec_start, gensym("start"), A_FLOAT, 0);
    class_addmethod(rec_class, (t_method)rec_end, gensym("end"), A_FLOAT, 0);
    class_addmethod(rec_class, (t_method)rec_rec, gensym("rec"), 0);
    class_addmethod(rec_class, (t_method)rec_stop, gensym("stop"), 0);
}
