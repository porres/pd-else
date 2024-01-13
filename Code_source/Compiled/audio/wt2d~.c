// Porres 2024

#include "m_pd.h"
#include "magic.h"
#include "buffer.h"

static t_class *wt2d_class;

typedef struct _wt2d{
    t_object  x_obj;
    t_buffer *x_buffer;
    double    x_phase;
    double    x_last_phase_offset;
    t_float   x_freq;
    t_inlet  *x_inlet_phase;
    t_inlet  *x_inlet_sync;
    t_outlet *x_outlet;
    t_int midi;
    t_int soft;
    t_float   x_sr;
    t_int     x_interp;
    t_int     x_columns, x_rows;
// MAGIC:
    t_glist  *x_glist;              // object list
    t_float  *x_signalscalar;       // right inlet's float field
    int       x_hasfeeders;         // right inlet connection flag
    t_float   x_phase_sync_float;   // float from magic
}t_wt2d;

static double wt2d_read(t_wt2d *x, double pos, int frame1, int size, t_word *vp){
    double val = 0;
    int offset = (frame1*size);
    int ndx = (int)pos;
    if(ndx == size)
        ndx = 0;
    if(x->x_interp == 0)
        return((double)vp[ndx+offset].w_float);
    double a, b, c, d, frac = pos - ndx;
    int ndx1 = ndx + 1;
    if(ndx1 == size)
        ndx1 = 0;
    if(x->x_interp <= 2){
        b = (double)vp[ndx+offset].w_float;
        c = (double)vp[ndx1+offset].w_float;
        val = x->x_interp == 2 ? interp_cos(frac, b, c) : interp_lin(frac, b, c);
    }
    else{
        int ndxm1 = ndx - 1;
        if(ndxm1 < 0)
            ndxm1 = size - 1;
        int ndx2 = ndx1 + 1;
        if(ndx2 == size)
            ndx2 = 0;
        a = (double)vp[ndxm1+offset].w_float;
        b = (double)vp[ndx+offset].w_float;
        c = (double)vp[ndx1+offset].w_float;
        d = (double)vp[ndx2+offset].w_float;
        if(x->x_interp == 3)
            val = interp_lagrange(frac, a, b, c, d);
        else
            val = interp_spline(frac, a, b, c, d);
    }
    return(val);
}

static t_int *wt2d_perform(t_int *w){
    t_wt2d *x = (t_wt2d *)(w[1]);
    int n = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // freq
    t_float *in2 = (t_float *)(w[4]); // sync
    t_float *in3 = (t_float *)(w[5]); // phase
    t_float *in4 = (t_float *)(w[6]); // y
    t_float *in5 = (t_float *)(w[7]); // z
    t_float *out = (t_float *)(w[8]);
    t_word *vp = x->x_buffer->c_vectors[0];
    if(!x->x_hasfeeders){ // Magic
        t_float *scalar = x->x_signalscalar;
        if(!else_magic_isnan(*x->x_signalscalar)){
            t_float input_phase = fmod(*scalar, 1);
            if(input_phase < 0)
                input_phase += 1;
            x->x_phase = input_phase;
            else_magic_setnan(x->x_signalscalar);
        }
    }
    double phase = x->x_phase;
    double last_phase_offset = x->x_last_phase_offset;
    double sr = x->x_sr;
    int npts_frame = (t_int)(x->x_buffer->c_npts) / (x->x_columns * x->x_rows);
    while(n--){
        if(x->x_buffer->c_playable){
            double hz = *in1++;
            if(x->midi)
                hz = pow(2, (hz - 69)/12) * 440;
            double phase_offset = (double)*in3++;
            double xpos = (double)(*in4++);
            double ypos = (double)(*in5++);
            double phase_step = hz / sr; // phase_step
            phase_step = phase_step > 0.5 ? 0.5 : phase_step < -0.5 ? -0.5 : phase_step; // clip nyq
            if(x->soft)
                phase_step *= (x->soft);
            double phase_dev = phase_offset - last_phase_offset;
            if(phase_dev >= 1 || phase_dev <= -1)
                phase_dev = fmod(phase_dev, 1); // wrap
            if(x->x_hasfeeders){ // signal connected, no magic
                t_float trig = *in2++;
                if(trig > 0 && trig <= 1){
                    if(x->soft)
                        x->soft = x->soft == 1 ? -1 : 1;
                    else
                        phase = trig;
                }
            }
            phase = phase + phase_dev;
            if(phase <= 0)
                phase += 1.; // wrap deviated phase
            if(phase >= 1)
                phase -= 1.; // wrap deviated phase
            if(vp){
                if(npts_frame < 4) // minimum table size is 4 points.
                    *out++ = 0;
                else{
                    double pos = phase*(double)npts_frame;
                    if(x->x_columns * x->x_rows == 1)
                        *out++ = wt2d_read(x, pos, 0, npts_frame, vp);
                    else{
                        if(xpos < 0)
                            xpos = 0;
                        if(xpos > 1)
                            xpos = 1;
                        xpos *= (x->x_columns - 1);
                        if(ypos < 0)
                            ypos = 0;
                        if(ypos > 1)
                            ypos = 1;
                        ypos *= (x->x_rows - 1);
                        int frame, xframe, yframe;
                        double xfadex, xfadey, frame1, frame2, row1, row2;
                        
                        xframe = (int)xpos;
                        yframe = (int)ypos;
                        xfadex = xpos - (double)xframe;
                        xfadey = ypos - (double)yframe;
                        
                        frame = xframe + yframe * (x->x_rows);
                        frame1 = wt2d_read(x, pos, frame, npts_frame, vp);
                        frame2 = wt2d_read(x, pos, frame+1, npts_frame, vp);
                        row1 = frame1 * (1-xfadex) + frame2 * xfadex;
                        
                        frame = xframe + (yframe + 1) * (x->x_rows);
                        frame1 = wt2d_read(x, pos, frame, npts_frame, vp);
                        frame2 = wt2d_read(x, pos, frame+1, npts_frame, vp);
                        row2 = frame1 * (1-xfadex) + frame2 * xfadex;
                        
                        *out++ = row1 * (1-xfadey) + row2 * xfadey;
                    }
                }
            }
            else // ??? maybe we dont need "playable"?
                *out++ = 0;
            phase += phase_step; // next phase
            last_phase_offset = phase_offset; // last phase offset
        }
        else
            *out++ = 0;
    }
    x->x_phase = phase;
    x->x_last_phase_offset = last_phase_offset;
    return(w+9);
}

static void wt2d_dsp(t_wt2d *x, t_signal **sp){
    buffer_checkdsp(x->x_buffer);
    if(x->x_buffer->c_playable && x->x_buffer->c_npts < 4)
        pd_error(x, "[wt2d~]: table too small, minimum size is 4");
    x->x_hasfeeders = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_sr = sp[0]->s_sr;
    dsp_add(wt2d_perform, 8, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
}

static void wt2d_slices(t_wt2d *x, t_floatarg f1, t_floatarg f2){
    x->x_columns = f1 < 1 ? 1 : (int)f1;
    x->x_rows = f1 < 1 ? 1 : (int)f2;
}

static void wt2d_set(t_wt2d *x, t_symbol *s){
    buffer_setarray(x->x_buffer, s);
}

static void wt2d_none(t_wt2d *x){
    x->x_interp = 0;
}

static void wt2d_lin(t_wt2d *x){
    x->x_interp = 1;
}

static void wt2d_cos(t_wt2d *x){
    x->x_interp = 2;
}

static void wt2d_lagrange(t_wt2d *x){
    x->x_interp = 3;
}

static void wt2d_spline(t_wt2d *x){
    x->x_interp = 4;
}

static void wt2d_midi(t_wt2d *x, t_floatarg f){
    x->midi = (int)(f != 0);
}

static void wt2d_soft(t_wt2d *x, t_floatarg f){
    x->soft = (int)(f != 0);
}

static void *wt2d_free(t_wt2d *x){
    buffer_free(x->x_buffer);
    inlet_free(x->x_inlet_sync);
    inlet_free(x->x_inlet_phase);
    outlet_free(x->x_outlet);
    return(void *)x;
}

static void *wt2d_new(t_symbol *s, int ac, t_atom *av){
    t_wt2d *x = (t_wt2d *)pd_new(wt2d_class);
    s = NULL;
    t_symbol *name = NULL;
    int nameset = 0, floatarg = 0;
    x->x_columns = x->x_rows = 1;
    x->x_freq = x->x_phase = x->x_last_phase_offset = 0.;
    t_float phaseoff = 0;
    x->x_interp = 4;
    x->midi = x->soft = 0;
    while(ac){
        if(av->a_type == A_SYMBOL){
            t_symbol *curarg = atom_getsymbol(av);
            if(curarg == gensym("-none")){
                if(nameset)
                    goto errstate;
                wt2d_none(x), ac--, av++;
            }
            else if(curarg == gensym("-lin")){
                if(nameset)
                    goto errstate;
                wt2d_lin(x), ac--, av++;
            }
            else if(curarg == gensym("-cos")){
                if(nameset)
                    goto errstate;
                wt2d_cos(x), ac--, av++;
            }
            else if(curarg == gensym("-lagrange")){
                if(nameset)
                    goto errstate;
                wt2d_lagrange(x), ac--, av++;
            }
            else if(curarg == gensym("-midi")){
                ac--, av++;
                if(nameset)
                    goto errstate;
                x->midi = 1;
            }
            else if(curarg == gensym("-soft")){
                ac--, av++;
                if(nameset)
                    goto errstate;
                x->soft = 1;
            }
            else if(curarg == gensym("-n")){
                if(nameset)
                    goto errstate;
                ac--, av++;
                if(ac < 2 || (av->a_type == A_SYMBOL) || ((av+1)->a_type == A_SYMBOL))
                    goto errstate;
                x->x_columns = atom_getint(av);
                if(x->x_columns < 1)
                    x->x_columns = 1;
                ac--, av++;
                x->x_rows = atom_getint(av);
                if(x->x_rows < 1)
                    x->x_rows = 1;
                ac--, av++;
            }
            else{
                if(nameset || floatarg)
                    goto errstate;
                name = curarg;
                nameset = 1, ac--, av++;
            }
        }
        else{ //else float
            switch(floatarg){
                case 0:
                    x->x_freq = atom_getfloatarg(0, ac, av);
                    break;
                case 1:
                    phaseoff = atom_getfloatarg(0, ac, av);
                    break;
                default:
                    break;
            };
            floatarg++, ac--, av++;
        };
    };
    x->x_inlet_sync = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_inlet_phase = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_phase, phaseoff < 0 || phaseoff > 1 ? 0 : phaseoff);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_outlet = outlet_new(&x->x_obj, gensym("signal"));
    // Magic
    x->x_glist = canvas_getcurrent();
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 1);
    // Magic End
    x->x_buffer = buffer_init((t_class *)x, name, 1, 0);
    return(x);
    errstate:
        post("[wt2d~]: improper args");
        return(NULL);
}

void wt2d_tilde_setup(void){
    wt2d_class = class_new(gensym("wt2d~"), (t_newmethod)wt2d_new,
        (t_method)wt2d_free, sizeof(t_wt2d), CLASS_DEFAULT, A_GIMME, 0);
    CLASS_MAINSIGNALIN(wt2d_class, t_wt2d, x_freq);
    class_addmethod(wt2d_class, (t_method)wt2d_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(wt2d_class, (t_method)wt2d_none, gensym("none"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_lin, gensym("lin"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_cos, gensym("cos"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_lagrange, gensym("lagrange"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_spline, gensym("spline"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_soft, gensym("soft"), A_DEFFLOAT, 0);
    class_addmethod(wt2d_class, (t_method)wt2d_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(wt2d_class, (t_method)wt2d_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(wt2d_class, (t_method)wt2d_slices, gensym("n"), A_FLOAT, A_FLOAT, 0);
}
