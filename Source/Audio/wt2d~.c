// Porres 2018-2025

#include <m_pd.h>
#include <math.h>
#include <stdlib.h>
#include "magic.h"
#include "buffer.h"

#define MAXLEN 1024

typedef struct _wt2d{
    t_object    x_obj;
    t_buffer   *x_buffer;
    double     *x_phase;
    int         x_nchans;
    t_int       x_n;
    t_int       x_sig1;
    t_int       x_sig2;
    t_int       x_ch2;
    t_int       x_ch3;
    t_int       x_ch4;
    t_int       x_ch5;
    t_int       x_midi;
    t_int       x_soft;
    t_int      *x_dir;
    float      *x_freq_list;
    t_int       x_list_size;
    double      x_sr_rec;
    t_int     x_interp;
    t_int     x_columns, x_rows, x_nframes;
    t_symbol   *x_ignore;
    t_inlet    *x_inlet_sync;
    t_inlet    *x_inlet_phase;
    t_inlet    *x_inlet_xfadeX;
    t_inlet    *x_inlet_xfadeY;
    t_outlet   *x_outlet;
// MAGIC:
    t_glist    *x_glist; // object list
    t_float    *x_signalscalar; // right inlet's float field
    t_float     x_phase_sync_float; // float from magic
}t_wt2d;

static t_class *wt2d_class;

static double wt2d_read(t_wt2d *x, double xpos, int frame, int size, t_word *vp){
    double val = 0;
    if(frame >= x->x_nframes)
        return(val);
    int offset = (frame*size);
    int ndx = (int)xpos;
    if(ndx == size)
        ndx = 0;
    if(x->x_interp == 0)
        return((double)vp[ndx+offset].w_float);
    double a, b, c, d, frac = xpos - ndx;
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

double wt2d_wrap_phase(double phase){
    while(phase >= 1)
        phase -= 1.;
    while(phase < 0)
        phase += 1.;
    return(phase);
}

static t_int *wt2d_perform(t_int *w){
    t_wt2d *x = (t_wt2d *)(w[1]);
    t_float *in1 = (t_float *)(w[2]); // freq
    t_float *in2 = (t_float *)(w[3]); // sync
    t_float *in3 = (t_float *)(w[4]); // phase
    t_float *in4 = (t_float *)(w[5]); // y
    t_float *in5 = (t_float *)(w[6]); // z
    t_float *out = (t_float *)(w[7]);
    t_word *vp = x->x_buffer->c_vectors[0];
    t_int *dir = x->x_dir;
    double *phase = x->x_phase;
// Magic Start
    if(!x->x_sig2){
        t_float *scalar = x->x_signalscalar;
        if(!else_magic_isnan(*x->x_signalscalar)){
            t_float input_phase = fmod(*scalar, 1);
            if(input_phase < 0)
                input_phase += 1;
            for(int j = 0; j < x->x_nchans; j++)
                x->x_phase[j] = input_phase;
            else_magic_setnan(x->x_signalscalar);
        }
    }
// Magic End
    int npts_frame = (t_int)(x->x_buffer->c_npts) / (x->x_nframes >= 1 ? x->x_nframes : 1);
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            double hz = x->x_sig1 ? in1[j*n + i] : x->x_freq_list[j];
            if(x->x_midi){
                if(hz > 127)
                    hz = 127;
                hz = hz <= 0 ? 0 : pow(2, (hz - 69)/12) * 440;
            }
            double step = hz * x->x_sr_rec; // phase step
            step = step > 0.5 ? 0.5 : step < -0.5 ? -0.5 : step;
            double xpos = x->x_ch4 == 1 ? in4[i] : in4[j*n + i];
            double ypos = x->x_ch5 == 1 ? in5[i] : in5[j*n + i];
            if(x->x_sig2){
                if(x->x_soft){
                    if(dir[j] == 0)
                        dir[j] = 1;
                    step *= (dir[j]);
                }
                t_float trig = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
                if(trig > 0 && trig <= 1){
                    if(x->x_soft)
                        dir[j] = dir[j] == 1 ? -1 : 1;
                    else
                        phase[j] = trig;
                }
            }
            double phase_offset = x->x_ch3 == 1 ? in3[i] : in3[j*n + i];
            double wraped_phase = wt2d_wrap_phase(phase[j] + phase_offset);
            
            t_float output;
            if(vp){
                if(npts_frame < 4) // minimum table size is 4 points.
                    output = 0;
                else{
                    double pos = wraped_phase * (double)npts_frame;
                    if(x->x_nframes == 1)
                        output = wt2d_read(x, pos, 0, npts_frame, vp);
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
                        
                        output = row1 * (1-xfadey) + row2 * xfadey;
                    }
                }
            }
            else // ??? maybe we dont need "playable"?
                output = 0;
    
            
            out[j*n + i] = output;
            
            phase[j] = wt2d_wrap_phase(phase[j] + step);
        }
    }
    x->x_phase = phase;
    x->x_dir = dir;
    return(w+8);
}

static void wt2d_dsp(t_wt2d *x, t_signal **sp){
    buffer_checkdsp(x->x_buffer);
    if(x->x_buffer->c_playable && x->x_buffer->c_npts < 4)
        pd_error(x, "[wt2d~]: table too small, minimum size is 4");
    x->x_n = sp[0]->s_n, x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans;
    x->x_ch4 = sp[3]->s_nchans, x->x_ch5 = sp[4]->s_nchans;
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    int chs = x->x_sig1 ? sp[0]->s_nchans : x->x_list_size;
    if(x->x_nchans != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_dir = (t_int *)resizebytes(x->x_dir,
            x->x_nchans * sizeof(t_int), chs * sizeof(t_int));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[5], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)
    || (x->x_ch4 > 1 && x->x_ch4 != x->x_nchans)
    || (x->x_ch5 > 1 && x->x_ch5 != x->x_nchans)){
        dsp_add_zero(sp[5]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[wt2d~]: channel sizes mismatch");
        return;
    }
    dsp_add(wt2d_perform, 7, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
}

static void wt2d_midi(t_wt2d *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void wt2d_slices(t_wt2d *x, t_floatarg f1, t_floatarg f2){
    x->x_columns = f1 < 1 ? 1 : (int)f1;
    x->x_rows = f1 < 1 ? 1 : (int)f2;
    x->x_nframes = x->x_columns * x->x_rows;
}

static void wt2d_table(t_wt2d *x, t_symbol *s){
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

static void wt2d_set(t_wt2d *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac != 2)
        return;
    int i = atom_getint(av);
    float f = atom_getint(av+1);
    if(i >= x->x_list_size)
        i = x->x_list_size;
    if(i <= 0)
        i = 1;
    i--;
    x->x_freq_list[i] = f;
}

static void wt2d_list(t_wt2d *x, t_symbol *s, int ac, t_atom * av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_list_size != ac){
        x->x_list_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_freq_list[i] = atom_getfloat(av+i);
}

static void wt2d_soft(t_wt2d *x, t_floatarg f){
    x->x_soft = (int)(f != 0);
}

static void *wt2d_free(t_wt2d *x){
    buffer_free(x->x_buffer);
    inlet_free(x->x_inlet_sync);
    inlet_free(x->x_inlet_phase);
    inlet_free(x->x_inlet_xfadeX);
    inlet_free(x->x_inlet_xfadeY);
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_dir, x->x_nchans * sizeof(*x->x_dir));
    free(x->x_freq_list);
    return(void *)x;
}

static void *wt2d_new(t_symbol *s, int ac, t_atom *av){
    t_wt2d *x = (t_wt2d *)pd_new(wt2d_class);
    x->x_ignore = s;
    x->x_midi = x->x_soft = 0;
    x->x_dir = (t_int *)getbytes(sizeof(*x->x_dir));
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_freq_list[0] = x->x_phase[0] = 0;
    x->x_list_size = 1;
    x->x_interp = 4;
    x->x_nframes = 1;
    x->x_columns = x->x_rows = 1;
    t_float phaseoff = 0, xpos = 0, ypos = 0;
    t_symbol *name = NULL;
    int nameset = 0, floatarg = 0;
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
                x->x_midi = 1;
            }
            else if(curarg == gensym("-soft")){
                ac--, av++;
                if(nameset)
                    goto errstate;
                x->x_soft = 1;
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
            else if(atom_getsymbol(av) == gensym("-mc")){
                ac--, av++;
                if(!ac || av->a_type != A_FLOAT)
                    goto errstate;
                int n = 0;
                while(ac && av->a_type == A_FLOAT){
                    x->x_freq_list[n] = atom_getfloat(av);
                    ac--, av++, n++;
                }
                x->x_list_size = n;
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
                    x->x_freq_list[0] = atom_getfloatarg(0, ac, av);
                    break;
                case 1:
                    phaseoff = atom_getfloatarg(0, ac, av);
                    break;
                default:
                    break;
            };
            floatarg++, ac--, av++;
        };
    }
    while(ac && av->a_type == A_SYMBOL){
        if(atom_getsymbol(av) == gensym("-midi")){
            x->x_midi = 1;
            ac--, av++;
        }
        else if(atom_getsymbol(av) == gensym("-soft")){
            x->x_soft = 1;
            ac--, av++;
        }
        else if(atom_getsymbol(av) == gensym("-mc")){
            ac--, av++;
            if(!ac || av->a_type != A_FLOAT)
                goto errstate;
            int n = 0;
            while(ac && av->a_type == A_FLOAT){
                x->x_freq_list[n] = atom_getfloat(av);
                ac--, av++, n++;
            }
            x->x_list_size = n;
        }
        else
            goto errstate;
    }
    if(ac && av->a_type == A_FLOAT){
        x->x_freq_list[0] = av->a_w.w_float;
        ac--, av++;
        if(ac && av->a_type == A_FLOAT){
            x->x_phase[0] = av->a_w.w_float;
            ac--, av++;
            if(ac && av->a_type == A_FLOAT){
                xpos = av->a_w.w_float;
                ac--, av++;
                if(ac && av->a_type == A_FLOAT){
                    ypos = av->a_w.w_float;
                    ac--, av++;
                }
            }
        }
    }
    x->x_nframes = x->x_columns * x->x_rows;
    x->x_buffer = buffer_init((t_class *)x, name, 1, 0, 0); // just added fifth argument: see buffer.c / buffer.h 
    x->x_phase[0] = phaseoff < 0 || phaseoff > 1 ? 0 : phaseoff;
    x->x_inlet_sync = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_sync, 0);
    x->x_inlet_phase = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_phase, x->x_phase[0]);
    x->x_inlet_xfadeX = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_xfadeX, xpos);
    x->x_inlet_xfadeY = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_xfadeY, ypos);
    x->x_outlet = outlet_new(&x->x_obj, gensym("signal"));
// Magic
    x->x_glist = canvas_getcurrent();
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 1);
    return(x);
errstate:
    post("[wt2d~]: improper args");
    return(NULL);
}

void wt2d_tilde_setup(void){
    wt2d_class = class_new(gensym("wt2d~"), (t_newmethod)wt2d_new, (t_method)wt2d_free,
        sizeof(t_wt2d), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(wt2d_class, nullfn, gensym("signal"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(wt2d_class, wt2d_list);
    class_addmethod(wt2d_class, (t_method)wt2d_soft, gensym("soft"), A_DEFFLOAT, 0);
    class_addmethod(wt2d_class, (t_method)wt2d_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(wt2d_class, (t_method)wt2d_set, gensym("set"), A_GIMME, 0);
    class_addmethod(wt2d_class, (t_method)wt2d_none, gensym("none"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_lin, gensym("lin"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_cos, gensym("cos"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_lagrange, gensym("lagrange"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_spline, gensym("spline"), 0);
    class_addmethod(wt2d_class, (t_method)wt2d_table, gensym("table"), A_SYMBOL, 0);
    class_addmethod(wt2d_class, (t_method)wt2d_slices, gensym("n"), A_FLOAT, 0);
}
