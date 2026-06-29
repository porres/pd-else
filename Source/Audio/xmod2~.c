#define TWOPI (3.14159265358979323846 * 2)

#include <m_pd.h>
#include <math.h>
#include <string.h>

typedef struct xmod2{
    t_object    x_obj;
    t_float     x_f;
    t_int       x_type;
    t_float    *x_x1;
    t_float    *x_y1;
    t_float    *x_x2;
    t_float    *x_y2;
    int         x_n;
    int         x_nchans;
    int         x_ch2;
    int         x_ch3;
    int         x_ch4;
    t_float     x_scale;
    t_inlet    *x_inlet_index1;
    t_inlet    *x_inlet_freq2;
    t_inlet    *x_inlet_index2;
}t_xmod2;

t_class *xmod2_class;

static t_int *xmod2_perform(t_int *w){
    t_xmod2 *x = (t_xmod2 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *index1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *index2 = (t_float *)(w[5]);
    t_float *out1 = (t_float *)(w[6]);
    t_float *out2 = (t_float *)(w[7]);
    t_float *x1 = x->x_x1;
    t_float *y1 = x->x_y1;
    t_float *x2 = x->x_x2;
    t_float *y2 = x->x_y2;
    int n = x->x_n, ch2 = x->x_ch2, ch3 = x->x_ch3, ch4 = x->x_ch4;
    t_float z1, dx1, dy1, z2, dx2, dy2;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            t_float freq1 = in1[j*n + i];
            t_float idx1 = ch2 == 1 ? index1[i] : index1[j*n + i];
            t_float freq2 = ch3 == 1 ? in2[i] : in2[j*n + i];
            t_float idx2 = ch4 == 1 ? index2[i] : index2[j*n + i];
            // osc 1
            freq1 = tan(freq1 * x->x_scale) / x->x_scale;
            z1 = (freq1 + (idx2 * x2[j])) * x->x_scale;
            dx1 = x1[j] - (z1 * y1[j]);
            dy1 = y1[j] + (z1 * x1[j]);
            x1[j] = dx1 > 1 ? 1 : dx1 < -1 ? -1 : dx1;
            y1[j] = dy1 > 1 ? 1 : dy1 < -1 ? -1 : dy1;
            // osc 2
            freq2 = tan(freq2 * x->x_scale) / x->x_scale;
            z2 = (freq2 + (idx1 * x1[j])) * x->x_scale;
            dx2 = x2[j] - (z2 * y2[j]);
            dy2 = y2[j] + (z2 * x2[j]);
            x2[j] = dx2 > 1 ? 1 : dx2 < -1 ? -1 : dx2;
            y2[j] = dy2 > 1 ? 1 : dy2 < -1 ? -1 : dy2;
            out1[j*n + i] = x1[j];
            out2[j*n + i] = x2[j];
        }
    }
    x->x_x1 = x1;
    x->x_y1 = y1;
    x->x_x2 = x2;
    x->x_y2 = y2;
    return(w+8);
}

static void xmod2_dsp(t_xmod2 *x, t_signal **sp){
    x->x_scale = TWOPI / sp[0]->s_sr;
    int chs = sp[0]->s_nchans;
    int ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans, ch4 = sp[3]->s_nchans;
    x->x_n = sp[0]->s_n;
    if(x->x_nchans != chs){
        x->x_x1 = (t_float *)resizebytes(x->x_x1,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_y1 = (t_float *)resizebytes(x->x_y1,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_x2 = (t_float *)resizebytes(x->x_x2,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_y2 = (t_float *)resizebytes(x->x_y2,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        for(int i = 0; i < chs; i++){
            x->x_x1[i] = x->x_x2[i] = 1;
            x->x_y1[i] = x->x_y2[i] = 0;
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[4], chs);
    signal_setmultiout(&sp[5], chs);
    if((ch2 > 1 && ch2 != x->x_nchans) || (ch3 > 1 && ch3 != x->x_nchans)
    || (ch4 > 1 && ch4 != x->x_nchans)){
        dsp_add_zero(sp[4]->s_vec, chs*x->x_n);
        dsp_add_zero(sp[5]->s_vec, chs*x->x_n);
        pd_error(x, "[xmod2~]: channel sizes mismatch");
        return;
    }
    dsp_add(xmod2_perform, 7, x, sp[0]->s_vec, sp[1]->s_vec,
            sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
}

static void *xmod2_free(t_xmod2 *x){
    freebytes(x->x_x1, x->x_nchans * sizeof(*x->x_x1));
    freebytes(x->x_y1, x->x_nchans * sizeof(*x->x_y1));
    freebytes(x->x_x2, x->x_nchans * sizeof(*x->x_x2));
    freebytes(x->x_y2, x->x_nchans * sizeof(*x->x_y2));
    return(void *)x;
}

static void *xmod2_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_xmod2 *x = (t_xmod2 *)pd_new(xmod2_class);
    x->x_x1 = (t_float *)getbytes(sizeof(*x->x_x1));
    x->x_y1 = (t_float *)getbytes(sizeof(*x->x_y1));
    x->x_x2 = (t_float *)getbytes(sizeof(*x->x_x2));
    x->x_y2 = (t_float *)getbytes(sizeof(*x->x_y2));
    x->x_x1[0] = x->x_x2[0] = 1;
    x->x_y1[0] = x->x_y2[0] = 0;
    t_int type = 0;
    x->x_f = 0;
    t_float freq2 = 0;
    t_float index1 = 0;
    t_float index2 = 0;
    t_int argnum = 0;
    x->x_scale = TWOPI / sys_getsr();
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            t_float argval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    x->x_f = argval;
                    break;
                case 1:
                    index1 = argval;
                    break;
                case 2:
                    freq2 = argval;
                    break;
                case 3:
                    index2 = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--;
            av++;
        }
        else
            goto errstate;
    };
    x->x_inlet_index1 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_index1, index1);
    x->x_inlet_freq2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq2, freq2);
    x->x_inlet_index2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_index2, index2);
    outlet_new(&x->x_obj, gensym("signal"));
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_type = type;
    return(void *)x;
    errstate:
        pd_error(x, "[xmod2~]: improper args");
        return NULL;
}

void xmod2_tilde_setup(void){
    xmod2_class = class_new(gensym("xmod2~"), (t_newmethod)xmod2_new,
        (t_method)xmod2_free, sizeof(t_xmod2), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(xmod2_class, t_xmod2, x_f);
    class_addmethod(xmod2_class, (t_method)xmod2_dsp, gensym("dsp"), 0);
}
