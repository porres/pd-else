// Porres 2017

#include <m_pd.h>
#include <math.h>

typedef struct _pulsecount{
    t_object  x_obj;
    t_float  *x_lastin;
    t_float  *x_count;
    t_float  *x_page;
    t_float  *x_output1;
    t_float  *x_output2;
    t_int     x_n;
    t_int     x_nchans;
    t_int     x_ch2;
    t_int     x_max;
    t_int     x_maxpages;
    t_inlet  *x_triglet;
    t_outlet *x_outlet;
    t_outlet *x_out_pages;
}t_pulsecount;

static t_class *pulsecount_class;

static t_int *pulsecount_perform(t_int *w){
    t_pulsecount *x = (t_pulsecount *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    t_float *lastin = x->x_lastin;
    t_float *count = x->x_count;
    t_float *page = x->x_page;
    t_float *output1 = x->x_output1;
    t_float *output2 = x->x_output2;
    int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            t_float in = in1[j*n + i];
            t_float trig = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            int pulse = (in > 0 && lastin[j] <= 0);
            if(trig > 0){
                count[j] = -1;
                page[j] = 0;
            }
            if(pulse){
                count[j]++;
                if(x->x_max > 0 && count[j] >= x->x_max){
                    count[j] = 0;
                    page[j]++;
                    if(x->x_maxpages > 0 && page[j] >= x->x_maxpages){
                        page[j] = 0;
                    }
                }
                output1[j] = count[j];
                output2[j] = page[j];
            }
            out1[j*n + i] = output1[j];
            out2[j*n + i] = output2[j];
            lastin[j] = in;
        }
    }
    x->x_output1 = output1;
    x->x_output2 = output2;
    x->x_lastin = lastin;
    x->x_count = count;
    x->x_page = page;
    return(w+6);
}

static void pulsecount_set(t_pulsecount *x, t_floatarg f){
    for(int i = 0; i < x->x_nchans; i++){
        x->x_count[i] = (int)f - 1;
        if(x->x_count[i] < 0)
            x->x_count[i] = -1;
    }
}

static void pulsecount_setpage(t_pulsecount *x, t_floatarg f){
    for(int i = 0; i < x->x_nchans; i++){
        x->x_page[i] = (int)f - 1;
        if(x->x_page[i] < 0)
            x->x_page[i] = -1;
    }
}

static void pulsecount_max(t_pulsecount *x, t_floatarg f){
    x->x_max = (int)f;
    if(x->x_max < 1)
        x->x_max = -1;
}

static void pulsecount_pages(t_pulsecount *x, t_floatarg f){
    x->x_maxpages = (int)f;
    if(x->x_maxpages < 1)
        x->x_maxpages = -1;
}

static void pulsecount_reset(t_pulsecount *x){
    for(int i = 0; i < x->x_nchans; i++){
        x->x_count[i] = -1;
        x->x_page[i] = 0;
    }
}

static void pulsecount_dsp(t_pulsecount *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_nchans != chs){
       x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_count = (t_float *)resizebytes(x->x_count,
             x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_page = (t_float *)resizebytes(x->x_page,
             x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_output1 = (t_float *)resizebytes(x->x_output1,
             x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_output2 = (t_float *)resizebytes(x->x_output2,
             x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        for(int i = 0; i < chs; i++){
            x->x_lastin[i] = 1;
            x->x_count[i] = -1;
            x->x_page[i] = 0;
            x->x_output1[i] = -1;
            x->x_output2[i] = -1;
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchans);
    signal_setmultiout(&sp[3], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_n);
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[pulsecount~]: channel sizes mismatch");
        return;
    }
    dsp_add(pulsecount_perform, 5, x, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *pulsecount_free(t_pulsecount *x){
    freebytes(x->x_lastin, x->x_nchans * sizeof(*x->x_lastin));
    freebytes(x->x_count, x->x_nchans * sizeof(*x->x_count));
    freebytes(x->x_page, x->x_nchans * sizeof(*x->x_page));
    freebytes(x->x_output1, x->x_nchans * sizeof(*x->x_output1));
    freebytes(x->x_output2, x->x_nchans * sizeof(*x->x_output2));
    inlet_free(x->x_triglet);
    outlet_free(x->x_outlet);
    outlet_free(x->x_out_pages);
    return(void *)x;
}

static void *pulsecount_new(t_floatarg f1, t_floatarg f2){
    t_pulsecount *x = (t_pulsecount *)pd_new(pulsecount_class);
    x->x_lastin = (t_float *)getbytes(sizeof(*x->x_lastin));
    x->x_lastin[0] = 1;
    x->x_count = (t_float *)getbytes(sizeof(*x->x_count));
    x->x_output1 = (t_float *)getbytes(sizeof(*x->x_output1));
    x->x_output2 = (t_float *)getbytes(sizeof(*x->x_output2));
    x->x_count[0] = x->x_output1[0] = x->x_output2[0] = -1;
    x->x_page = (t_float *)getbytes(sizeof(*x->x_page));
    x->x_page[0] = 0;
    x->x_max = (int)f1;
    if(x->x_max < 1)
       x->x_max = -1;
    x->x_maxpages = (int)f2;
    if(x->x_maxpages < 1)
       x->x_maxpages = -1;
    x->x_triglet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_out_pages = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void pulsecount_tilde_setup(void){
    pulsecount_class = class_new(gensym("pulsecount~"), (t_newmethod)pulsecount_new,
        (t_method)pulsecount_free, sizeof(t_pulsecount), CLASS_MULTICHANNEL, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(pulsecount_class, nullfn, gensym("signal"), 0);
    class_addmethod(pulsecount_class, (t_method) pulsecount_dsp, gensym("dsp"), A_CANT, 0);
    class_addbang(pulsecount_class, pulsecount_reset);
    class_addmethod(pulsecount_class, (t_method)pulsecount_max, gensym("max"), A_DEFFLOAT, 0);
    class_addmethod(pulsecount_class, (t_method)pulsecount_pages, gensym("pages"), A_DEFFLOAT, 0);
    class_addmethod(pulsecount_class, (t_method)pulsecount_set, gensym("set"), A_DEFFLOAT, 0);
    class_addmethod(pulsecount_class, (t_method)pulsecount_setpage, gensym("setpage"), A_DEFFLOAT, 0);
}
