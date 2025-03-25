// porres 2020

#include <m_pd.h>
#include <m_imp.h>
#include <buffer.h>

#define DRAW_PERIOD     500.        // draw period

typedef struct _tabwriter{
    t_object            x_obj;
    t_buffer           *x_buffer;
    t_float             x_f; // dummy input float
    t_float            *x_gate_vec; // gate signal vector
    t_float             x_last_gate;
    t_int               x_continue;
    t_int               x_loop;
    t_int               x_phase;       // write head
    t_clock            *x_clock;
    double              x_clocklasttick;
    float               x_range_start;
    float               x_range_end;
    t_int               x_isrunning;
    t_int               x_newrun;
    int                 x_whole_array;
    unsigned long long  x_startindex;
    unsigned long long  x_endindex;
    unsigned long long  x_index;
    int                 x_indexed;
    int                 x_n;
    t_float             x_ksr;
    t_int               x_numchans;
    // new var to store namemode: 0 = <ch>-<arrayname>, 1 = <arrayname>-<ch> 
    int                 x_namemode;         // 0 is <ch>-<arrayname>, 1 is <arrayname>-<ch>
    t_outlet           *x_outlet_bang;
    t_float           **x_ins; // input vectors
    t_float            *x_out; // signal output
}t_tabwriter;

static t_class *tabwriter_class;

static void tabwriter_draw(t_tabwriter *x){
    if(x->x_buffer != NULL){
        if(x->x_buffer->c_playable)
            buffer_redraw(x->x_buffer);
    }
}

static void tabwriter_tick(t_tabwriter *x){ // Redraw!
    double timesince = clock_gettimesince(x->x_clocklasttick);
    if(timesince >= DRAW_PERIOD){
        buffer_redraw(x->x_buffer);
        x->x_clocklasttick = clock_getlogicaltime();
    }
}

static void tabwriter_set(t_tabwriter *x, t_symbol *s){
    if(x->x_buffer != NULL)
        buffer_setarray(x->x_buffer, s);
    else{
        // init buffer according to namemode: 0 = <ch>-<arrayname>, 1 = <arrayname>-<ch>
        if(x->x_namemode == 0) {
            x->x_buffer = buffer_init((t_class *)x, s, x->x_numchans, 0, 0); /// new arg added 0: <ch>-<arrayname> mode / 1: <arrayname>-<ch> mode !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        }
        else if(x->x_namemode == 1){
            x->x_buffer = buffer_init((t_class *)x, s, x->x_numchans, 0, 1); 
        }    
        // x->x_buffer = buffer_init((t_class *)x, s, x->x_numchans, 0);
        buffer_setminsize(x->x_buffer, 2);
    }
}

static void tabwriter_reset(t_tabwriter *x){
    x->x_startindex = 0;
    x->x_whole_array = 1; // array size in samples
}

static void tabwriter_continue(t_tabwriter *x, t_floatarg f){
	x->x_continue = (f != 0);
}

static void tabwriter_loop(t_tabwriter *x, t_floatarg f){
    x->x_loop = (f != 0);
}

static void tabwriter_rec(t_tabwriter *x){
    if(x->x_buffer != NULL){
        if(!x->x_continue)
            x->x_phase = 0.;
        x->x_isrunning = x->x_newrun = 1;
        buffer_redraw(x->x_buffer);
    }
}

static void tabwriter_stop(t_tabwriter *x){
    if(x->x_buffer != NULL){
        if(!x->x_continue)
            x->x_phase = 0.;
        x->x_isrunning = 0;
        buffer_redraw(x->x_buffer);
    }
}

static void tabwriter_float(t_tabwriter *x, t_floatarg f){
    f > 0 ? tabwriter_rec(x) : tabwriter_stop(x);
}

static t_int *tabwriter_perform(t_int *w){
    t_tabwriter *x = (t_tabwriter *)(w[1]);
    if(x->x_buffer == NULL)
        return(w+2);
    t_buffer *c = x->x_buffer;
    t_int nch = c->c_numchans;
    t_float *gatein = x->x_gate_vec;
    t_float *out = x->x_out;
    t_int i, j, bang = 0;
    unsigned long long phase, range;
    buffer_validate(c, 0);
    t_float last_gate = x->x_last_gate;
    clock_delay(x->x_clock, 0);
    for(i = 0; i < x->x_n; i++){
        unsigned long long start = x->x_startindex;
        unsigned long long end;
        unsigned long long npts = (unsigned long long)c->c_npts;
        if(x->x_whole_array)
            end = npts;
        else{
            end = x->x_endindex;
            if(end > npts)
                end = npts;
        }
        if(x->x_indexed){
            if((start < end) && c->c_playable){
                unsigned long long idx = (unsigned long long)gatein[i];
                if(idx >= start && idx < end){
                    for(j = 0; j < nch; j++){
                        t_word *vp = c->c_vectors[j];
                        t_float *insig = x->x_ins[j];
                        if(vp)
                            vp[idx].w_float = insig[i];
                    };
                }
            }
            out[i] = 0;
        }
        else{
            unsigned long long index = x->x_index;
            t_float gate = gatein[i];
            if(gate != 0 && last_gate == 0)
                tabwriter_rec(x);
            else if(gate == 0 && last_gate != 0)
                tabwriter_stop(x);
            last_gate = gate;
            if((start < end) && c->c_playable && x->x_isrunning){
                range = end - start;
                if(x->x_newrun == 1 && x->x_continue == 0){ // continue shouldn't reset phase
                    x->x_newrun = 0;
                    x->x_phase = start;
                };
                phase = x->x_phase;
                if(phase >= end){ // boundscheck (might've changed when paused)
                    bang = 1;
                    if(x->x_loop == 1)
                        phase = start;
                    else{ // stop
                        x->x_isrunning = 0;
                        buffer_redraw(x->x_buffer);
                    };
                };
                if(phase < start)
                    phase = start;
                if(x->x_isrunning == 1){ // if we're still running after boundschecking
                    for(j = 0; j < nch; j++){
                        t_word *vp = c->c_vectors[j];
                        t_float *insig = x->x_ins[j];
                        if(vp)
                            vp[phase].w_float = insig[i];
                    };
                    index = phase;
                    phase++;
                    x->x_phase = phase;
                 };
            };
            out[i] = index;
            x->x_index = index;
        }
    };
    if(bang)
        outlet_bang(x->x_outlet_bang);
    x->x_last_gate = last_gate;
    return(w+2);
}

static void tabwriter_dsp(t_tabwriter *x, t_signal **sp){
    if(x->x_buffer != NULL)
        buffer_checkdsp(x->x_buffer);
    x->x_ksr = sp[0]->s_sr * 0.001;
    x->x_n = sp[0]->s_n;
    t_signal **sigp = sp;
    for(int i = 0; i < x->x_numchans; i++)  // input vectors for each channel
        *(x->x_ins+i) = (*sigp++)->s_vec;
    x->x_gate_vec = (*sigp++)->s_vec;       // last is gate or index
    x->x_out = (*sigp++)->s_vec;            // output
    dsp_add(tabwriter_perform, 1, x);
}

static void tabwriter_free(t_tabwriter *x){
    if(x->x_buffer != NULL)
        buffer_free(x->x_buffer);
    outlet_free(x->x_outlet_bang);
    freebytes(x->x_ins, x->x_numchans * sizeof(*x->x_ins));
    if(x->x_clock)
        clock_free(x->x_clock);
    pd_unbind(&x->x_obj.ob_pd, gensym("pd-dsp-stopped"));
}

static void tabwriter_range_check(t_tabwriter *x){
    if(x->x_startindex > x->x_endindex){
        unsigned long long temp = x->x_startindex;
        x->x_startindex = x->x_endindex;
        x->x_endindex = temp;
    }
}

static void tabwriter_index(t_tabwriter *x, t_float f){
    x->x_indexed = f != 0;
}

static void tabwriter_start(t_tabwriter *x, t_float f){
    x->x_startindex = f < 0 ? 0 : (unsigned long long)(f * x->x_ksr);
    tabwriter_range_check(x);
}

static void tabwriter_end(t_tabwriter *x, t_float end){
    if(end < 0)
        x->x_whole_array = 1;
    else{
        x->x_endindex = (unsigned long long)(end * x->x_ksr);
        x->x_whole_array = 0;
    }
    tabwriter_range_check(x);
}

static void tabwriter_range(t_tabwriter *x, t_floatarg f1, t_floatarg f2){
    if(!x->x_buffer) return;
    
    f1 = f1 < 0 ? 0 : f1 > 1 ? 1 : f1;
    f2 = f2 < 0 ? 0 : f2 > 1 ? 1 : f2;
    x->x_startindex = (unsigned long long)(f1 * x->x_buffer->c_npts);
    x->x_endindex = (unsigned long long)(f2 * x->x_buffer->c_npts);
    x->x_whole_array = x->x_endindex < 0;
    tabwriter_range_check(x);
}

static void *tabwriter_new(t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    t_tabwriter *x = (t_tabwriter *)pd_new(tabwriter_class);
    x->x_ksr = (float)sys_getsr() * 0.001;
    x->x_last_gate = x->x_newrun = x->x_isrunning = x->x_phase = 0;
    t_float numchan = 1;
    t_float start = 0;
    t_float end = -1;
    x->x_whole_array = 1;
    x->x_continue = 0;
    x->x_loop = 0;
    x->x_indexed = 0;
    t_int nameset = 0;     // flag if name is set
    t_int argn = 0;
    t_symbol *name = NULL;
    x->x_buffer = NULL;
    // new var to store namemode: 0 = <ch>-<arrayname>, 1 = <arrayname>-<ch>
    x->x_namemode = 0; 
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            t_symbol *curarg = atom_getsymbolarg(0, ac, av);
            // added flag to set namemode: 0 = <ch>-<arrayname>, 1 = <arrayname>-<ch>
            if(curarg == gensym("-chafter") && !argn){
                x->x_namemode = 1; 
                ac--, av++;
            } 
            else if(curarg == gensym("-continue") && !argn){
                if(ac >= 1){
                    x->x_continue = 1;
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else if(curarg == gensym("-loop") && !argn){
                if(ac >= 1){
                    x->x_loop = 1;
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else if(curarg == gensym("-start") && !argn){
                if(ac >= 2){
                    start = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(curarg == gensym("-end") && !argn){
                if(ac >= 2){
                    end = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(curarg == gensym("-ch") && !argn){
                if(ac >= 2){
                    numchan = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(curarg == gensym("-index") && !argn){
                if(ac >= 1){
                    x->x_indexed = 1;
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else if(!nameset){ // if name not passed so far, count arg as array name
                name = atom_getsymbolarg(0, ac, av);
                argn = 1;
                ac--, av++;
                nameset = 1;
            }
            else
                goto errstate;
        }
        else if(av->a_type == A_FLOAT){
            argn = 1;
            numchan = atom_getfloatarg(0, ac, av);
            ac--, av++;
        }
        else
            goto errstate;
    };
    int chn_n = (int)numchan > 64 ? 64 : (int)numchan;
    if(name != NULL){
        // init buffer according to namemode: 0 = <ch>-<arrayname>, 1 = <arrayname>-<ch>
        if(x->x_namemode == 0) {
            x->x_buffer = buffer_init((t_class *)x, name, chn_n, 0, 0); /// new arg added 0: <ch>-<arrayname> mode / 1: <arrayname>-<ch> mode !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        }
        else if(x->x_namemode == 1){
            x->x_buffer = buffer_init((t_class *)x, name, chn_n, 0, 1); 
        }  
        t_buffer *c = x->x_buffer;
        if(c) // set channels and array sizes
            buffer_setminsize(x->x_buffer, 2);
    }
    x->x_numchans = chn_n;
    x->x_ins = getbytes(x->x_numchans * sizeof(*x->x_ins)); // allocate in vectors
    x->x_startindex = start < 0 ? 0 : (unsigned long long)(start * x->x_ksr);
    tabwriter_end(x, end);
    x->x_clock = clock_new(x, (t_method)tabwriter_tick);
    x->x_clocklasttick = clock_getlogicaltime();
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    for(t_int i = 1; i < x->x_numchans; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_outlet_bang = outlet_new(&x->x_obj, 0);
    pd_bind(&x->x_obj.ob_pd, gensym("pd-dsp-stopped"));
    return(x);
errstate:
    post("[tabwriter~]: improper args");
    return(NULL);
}

void tabwriter_tilde_setup(void){
    tabwriter_class = class_new(gensym("tabwriter~"), (t_newmethod)tabwriter_new,
        (t_method)tabwriter_free, sizeof(t_tabwriter), CLASS_DEFAULT, A_GIMME, 0);
    class_addfloat(tabwriter_class, tabwriter_float);
    class_addbang(tabwriter_class, tabwriter_draw); // for pd-dsp-stopped, undocumented
//    class_addbang(tabwriter_class, tabwriter_rec);
    class_addmethod(tabwriter_class, nullfn, gensym("signal"), 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_continue, gensym("continue"), A_FLOAT, 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_loop, gensym("loop"), A_FLOAT, 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_reset, gensym("reset"), 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_start, gensym("start"), A_FLOAT, 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_range, gensym("range"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_end, gensym("end"), A_FLOAT, 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_index, gensym("index"), A_FLOAT, 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_rec, gensym("rec"), 0);
    class_addmethod(tabwriter_class, (t_method)tabwriter_stop, gensym("stop"), 0);
}
