
#include "m_pd.h"
#include "m_imp.h"
#include "buffer.h"
#include <string.h>

#define REC_MAXBD 1E+32 // cheap higher bound for boundary points
#define REC_DRAW_PERIOD  1000.  // draw period

typedef struct _rec{
    t_object    x_obj;
    t_buffer   *x_buffer;
    t_float    *x_gate_vec; // gate signal
    t_float     x_last_gate;
    int         x_continuemode;
    int         x_loopmode;
    int         x_phase;       /* writing head */
    t_float     x_f; // dummy input float
    t_float     x_sync; // output sync value
    t_clock    *x_clock;
    double      x_clocklasttick;
    int         x_isrunning;
    int         x_newrun; // if running turned from off and on for this current block
    t_outlet    *x_outlet;
    t_float     x_ksr; //sample rate in ms
    int 	    x_numchans;
    t_float   **x_ivecs; // input vectors
    t_float     x_start; //start position (in ms) vector
    t_float     x_end; //endposition (in ms) vec
    t_float    *x_ovec; //output vector
}t_rec;

static t_class *rec_class;

static void rec_tick(t_rec *x){
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
        x->x_isrunning = 1;
    x->x_start = 0;
    x->x_end = (t_float)x->x_buffer->c_npts/x->x_ksr; //array size in samples
}

static int rec_startpoint(t_rec *x, t_floatarg f){
    int npts = x->x_buffer->c_npts;
    //some bounds checking to prevent overflow
    int startindex;
    if(f < REC_MAXBD)
        startindex = (int)(f * x->x_ksr);
    else
        startindex = SHARED_INT_MAX;
    if(startindex < 0)
		startindex = 0;  /* CHECKED */
    else if(startindex >= npts) // make it the last index addressable
		startindex = npts - 1;  /* CHECKED (both ways) */
    return startindex;
}

static int rec_endpoint(t_rec *x, t_floatarg f){
    // noninclusive
    int endindex;
    int npts = x->x_buffer -> c_npts;
    //some bounds checking to prevent overflow
    if(f < REC_MAXBD)
        endindex = (int)(f * x->x_ksr);
    else
        endindex = SHARED_INT_MAX;
    if(endindex < 0)
        endindex = 0;
    if (endindex >= npts) // if bigger than the end, set it to the end
		endindex = npts;  // CHECKED (both ways)
    return endindex;
}

static void rec_continue(t_rec *x, t_floatarg f){
	x->x_continuemode = (f != 0);
}

static void rec_loop(t_rec *x, t_floatarg f){
    x->x_loopmode = (f != 0);
}

static void rec_rec(t_rec *x){
    x->x_isrunning = 1;
    x->x_newrun = 1;
}

static void rec_stop(t_rec *x){
    x->x_isrunning = 0;
    clock_delay(x->x_clock, 0); // trigger a redraw
    x->x_sync = 0.;
    if(x->x_continuemode == 0)
        x->x_phase = 0.;
}

static t_int *rec_perform(t_int *w){
    t_rec *x = (t_rec *)(w[1]);
    t_buffer * c = x->x_buffer;
    int nch = c->c_numchans;
    int nblock = (int)(w[2]);
    t_float *gatein = x->x_gate_vec;
    t_float *out = x->x_ovec;
    t_float gate, startms, endms, sync;
    int startsamp, endsamp, phase, range, i, j;
    buffer_validate(c, 0);
    t_float last_gate = x->x_last_gate;
    for(i = 0; i < nblock; i++){
        gate = gatein[i];
        if(gate != 0 && last_gate == 0)
            rec_rec(x);
        else if(gate == 0 && last_gate != 0)
            rec_stop(x);
        last_gate = gate;
        startms = x->x_start;
        endms = x->x_end;
        if((startms < endms) && c->c_playable && x->x_isrunning){
            startsamp = rec_startpoint(x, startms);
            endsamp = rec_endpoint(x, endms);
            range = endsamp - startsamp;
            //continue mode shouldn't reset phase
            if(x->x_newrun == 1 && x->x_continuemode == 0){
                //isrunning 0->1 from last block, means reset phase appropriately
                x->x_newrun = 0;
                x->x_phase = startsamp;
                x->x_sync = 0.;
            };
            phase = x->x_phase;
            //boundschecking, do it here because points might changed when paused
            //easier case, when we're "done"
            if(phase >= endsamp){
                if(x->x_loopmode == 1)
                    phase = startsamp;
                else{
                    //not looping, just stop it
                    x->x_isrunning = 0;
                    //mb a bit redundant, but just make sure x->x_sync is 1
                    x->x_sync = 1;
                    //trigger redraw
                };
                //in either case calculate a redraw
	        clock_delay(x->x_clock, 0);
            };
            // harder case up to interpretation
            if(phase < startsamp) // if before startsamp, just jump to startsamp?
                phase = startsamp;
            if(x->x_isrunning == 1){ // if we're still running after boundschecking
                for(j = 0; j < nch; j++){
                    t_word *vp = c->c_vectors[j];
                    t_float *insig = x->x_ivecs[j];
                    if(vp)
                        vp[phase].w_float = insig[i];
                };
                //sync output
                sync = (t_float)(phase - startsamp)/(t_float)range;
                //increment stage
                phase++;
                x->x_phase = phase;
                x->x_sync = sync;
             };
        };
        //in any case, output sync value
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
    x->x_gate_vec = (*sigp++)->s_vec; // the first sig in is the gate
    for(i = 0; i < x->x_numchans; i++) //input vectors first
        *(x->x_ivecs+i) = (*sigp++)->s_vec;
    x->x_ovec = (*sigp++)->s_vec;
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
    if(start < 0)
        start = 0;
    x->x_start = start;
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

static void *rec_new(t_symbol *s, int argc, t_atom *argv){
    t_symbol *dummy = s;
    dummy = NULL;
    t_float numchan = 1;
    t_float continuemode = 0;
    t_float loopstatus = 0;
    t_float start = 0;
    t_float end = -1;
    int nameset = 0; //flag if name is set
    t_symbol * arrname = NULL;
    if(argc > 0 && argv ->a_type == A_SYMBOL){
        if(!nameset){ //if name not passed so far, count arg as array name
            arrname = atom_getsymbolarg(0, argc, argv);
            argc--;
            argv++;
            nameset = 1;
        }
    };
    int argnum = 0;
    while(argc > 0){
        if(argv->a_type == A_SYMBOL){
            t_symbol * curarg = atom_getsymbolarg(0, argc, argv);
            if(!strcmp(curarg->s_name, "-continue")){
                if(argc >= 2){
                    continuemode = atom_getfloatarg(1, argc, argv);
                    argc-=2;
                    argv+=2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(curarg->s_name, "-loop")){
                if(argc >= 2){
                    loopstatus = atom_getfloatarg(1, argc, argv);
                    argc-=2;
                    argv+=2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(curarg->s_name, "-start")){
                if(argc >= 2){
                    start = atom_getfloatarg(1, argc, argv);
                    argc-=2;
                    argv+=2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(curarg->s_name, "-end")){
                if(argc >= 2){
                    end = atom_getfloatarg(1, argc, argv);
                    argc-=2;
                    argv+=2;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else if(argv->a_type == A_FLOAT){
            if(nameset){
                t_float argval = atom_getfloatarg(0, argc, argv);
                switch(argnum){
                    case 0:
                        numchan = argval;
                        break;
                    default:
                        break;
                };
                argnum++;
                argc--;
                argv++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    int chn_n = (int)numchan > 64 ? 64 : (int)numchan;
    //old arsic notes - chn_n number of channels, 0 nsigs 1 nauxsigs
    t_rec *x = (t_rec *)pd_new(rec_class);
    x->x_ksr = (float)sys_getsr() * 0.001;
    x->x_buffer = buffer_init((t_class *)x, arrname, chn_n, 0);
    t_buffer * c = x->x_buffer;
    // init
    x->x_last_gate = 0;
    x->x_newrun = 0;
    x->x_isrunning = 0;
    x->x_sync = 0;
    x->x_phase = 0;
    if(c){ //setting channels and array sizes
        x->x_numchans = c->c_numchans;
        t_float arraysmp = (t_float)c->c_npts;
        //allocate input vectors
        x->x_ivecs = getbytes(x->x_numchans * sizeof(*x->x_ivecs));
        if(end < 0){ // bounds checking
            //if end not set
            //if less than 0 and if nonzero array found, set it to arraylen in ms
            if(arraysmp > 0)
                end = (arraysmp/x->x_ksr);
            else //else default to max size of float (defaults to size of array with boundschecking
                end = REC_MAXBD;
        };
        if(start < 0)
            start = 0;
        x->x_start = start;
        x->x_end = end;
        buffer_setminsize(x->x_buffer, 2);
        rec_continue(x, continuemode);
        rec_loop(x, loopstatus);
        x->x_clock = clock_new(x, (t_method)rec_tick);
        x->x_clocklasttick = clock_getlogicaltime();
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        for(int i = 1; i < x->x_numchans; i++)
            inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        x->x_outlet = outlet_new(&x->x_obj, gensym("signal")); // sync output
    };
    return(x);
errstate:
    post("rec~: improper args");
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
