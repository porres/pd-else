
#include "m_pd.h"

#define ENVGEN_MAX_SIZE  512 // maximum segments

typedef struct _envgenseg{
    float  s_target;
    float  s_delta;
}t_envgen_seg;

typedef struct _envgen{
    t_object        x_obj;
    float           x_value;
    float           x_target;
    float           x_delta;
    float           x_inc;
    float           x_biginc;
    float           x_ksr;
    int             x_ac;
    t_atom          *x_av;
    int             x_nleft;
    int             x_retarget;
    int             x_size;      // as allocated
    int             x_nsegs;     // as used
    int             x_pause;
    int             x_status;
    t_float         x_gain;
    t_envgen_seg    *x_curseg;
    t_envgen_seg    *x_segs;
    t_envgen_seg    x_segini[ENVGEN_MAX_SIZE];
    t_clock         *x_clock;
    t_outlet        *x_out2;
} t_envgen;

static t_class *envgen_class;

static void envgen_tick(t_envgen *x){
    if(x->x_status)
        outlet_float(x->x_out2, x->x_status = 0);
}

static void copy_atoms(t_atom *src, t_atom *dst, int n){
    while(n--)
        *dst++ = *src++;
}

static void envgen_set(t_envgen *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    t_atom *a;
    t_int n;
    for(n = 0, a = av; n < ac; n++, a++)
        if(a->a_type != A_FLOAT){
            pd_error(x, "envgen~: set needs to only contain floats");
            return;
        }
    if(!ac)
        return;
    x->x_ac = ac;
    x->x_av = getbytes(x->x_ac * sizeof(*(x->x_av)));
    copy_atoms(av, x->x_av, x->x_ac);
}

static void envgen_list(t_envgen *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    if(!x->x_status)
        outlet_float(x->x_out2, x->x_status = 1); // turn on status
    t_envgen_seg *segp;
    t_atom *a;
    t_int n;
    for(n = 0, a = av; n < ac; n++, a++)
        if(a->a_type != A_FLOAT){
            pd_error(x, "envgen~: list needs to only contain floats");
            return;
        }
    if(!ac)
        return;
    x->x_ac = ac;
    x->x_av = getbytes(x->x_ac * sizeof(*(x->x_av)));
    copy_atoms(av, x->x_av, x->x_ac);
    t_int odd = ac % 2;
    t_int nsegs = ac / 2;
    if(odd)
        nsegs++; // add an extra segment
    if(nsegs > ENVGEN_MAX_SIZE){
        nsegs = ENVGEN_MAX_SIZE;
        odd = 0;
    };
    x->x_nsegs = nsegs; // define number of line segments
    segp = x->x_segs;
    if(odd){ // initialize 1st segment
        segp->s_delta = 0;
        segp->s_target = av++->a_w.w_float * x->x_gain;
        segp++;
        nsegs--;
    }
    while(nsegs--){
        segp->s_delta = av++->a_w.w_float;
        segp->s_target = av++->a_w.w_float * x->x_gain;
        segp++;
    }
    x->x_target = x->x_segs->s_target;
    x->x_curseg = x->x_segs;
    x->x_retarget = 1;
    x->x_pause = 0;
}

static void envgen_bang(t_envgen *x){
    envgen_list(x, &s_list, x->x_ac, x->x_av);
}

static void envgen_float(t_envgen *x, t_float f){
    t_atom a[2];
    SETFLOAT(a, 0);
    SETFLOAT(a+1, f);
    envgen_list(x, &s_list, 2, a);
}

static void envgen_gain(t_envgen *x, t_float f){
    if(f != 0){
        x->x_gain = f;
        envgen_list(x, &s_list, x->x_ac, x->x_av);
    }
}

static t_int *envgen_perform(t_int *w){
    t_envgen *x = (t_envgen *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    int nblock = (int)(w[4]);
    int nxfer = x->x_nleft;
    float curval = x->x_value;
    float inc = x->x_inc;
    float biginc = x->x_biginc;
    if(x->x_pause){
        while (nblock--)
            *out++ = curval;
        return(w + 5);
    }
    if(PD_BIGORSMALL(curval))
        curval = x->x_value = 0;
retarget:
    if(x->x_retarget){
        float target = x->x_curseg->s_target;
        float delta = x->x_curseg->s_delta;
    	int npoints = delta * x->x_ksr + 0.5;  /* LATER rethink */
        x->x_nsegs--;
        x->x_curseg++;
    	while(npoints <= 0){
            curval = x->x_value = target;
            if (x->x_nsegs){
                target = x->x_curseg->s_target;
                delta = x->x_curseg->s_delta;
                npoints = delta * x->x_ksr + 0.5;  /* LATER rethink */
                x->x_nsegs--;
                x->x_curseg++;
            }
            else{
                while (nblock--)
                    *out++ = curval;
                x->x_nleft = 0;
                clock_delay(x->x_clock, 0);
                x->x_retarget = 0;
                return(w + 5);
            }
        }
    	nxfer = x->x_nleft = npoints;
    	inc = x->x_inc = (target - x->x_value) / (float)npoints;
        x->x_biginc = (int)(w[4]) * inc;
        biginc = nblock * inc;
        x->x_target = target;
    	x->x_retarget = 0;
    }
    if(nxfer >= nblock){
        if ((x->x_nleft -= nblock) == 0){
            if (x->x_nsegs)
                x->x_retarget = 1;
            else
                clock_delay(x->x_clock, 0);
            x->x_value = x->x_target;
        }
        else
            x->x_value += biginc;
    	while(nblock--)
            *out++ = curval, curval += inc;
    }
    else if(nxfer > 0){
        nblock -= nxfer;
        do
            *out++ = curval, curval += inc;
        while(--nxfer);
            curval = x->x_value = x->x_target;
        if(x->x_nsegs){
            x->x_retarget = 1;
            goto
                retarget;
        }
        else{
            while(nblock--)
                *out++ = curval;
            x->x_nleft = 0;
            clock_delay(x->x_clock, 0);
        }
    }
    else
        while(nblock--)
            *out++ = curval;
    return(w + 5);
}

static void envgen_dsp(t_envgen *x, t_signal **sp){
    x->x_ksr = sp[0]->s_sr * 0.001;
    dsp_add(envgen_perform, 4, x, sp[0]->s_vec, sp[0]->s_vec, sp[0]->s_n);
}

static void envgen_free(t_envgen *x){
    if(x->x_segs != x->x_segini)
        freebytes(x->x_segs, x->x_size * sizeof(*x->x_segs));
    if(x->x_clock)
        clock_free(x->x_clock);
}

static void envgen_pause(t_envgen *x){
    x->x_pause = 1;
}

static void envgen_resume(t_envgen *x){
    x->x_pause = 0;
}

static void *envgen_new(t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    t_envgen *x = (t_envgen *)pd_new(envgen_class);
    x->x_gain = 1;
    x->x_nleft = 0;
    x->x_retarget = 0;
    x->x_size = ENVGEN_MAX_SIZE;
    x->x_nsegs = 0;
    x->x_pause = 0;
    x->x_segs = x->x_segini;
    x->x_curseg = 0;
    x->x_ac = ac;
    x->x_av = getbytes(x->x_ac * sizeof(*(x->x_av)));
    copy_atoms(av, x->x_av, x->x_ac);
    outlet_new((t_object *)x, &s_signal);
    x->x_out2 = outlet_new((t_object *)x, &s_float);
    x->x_clock = clock_new(x, (t_method)envgen_tick);
    return(x);
}

void envgen_tilde_setup(void){
    envgen_class = class_new(gensym("envgen~"), (t_newmethod)envgen_new,
            (t_method)envgen_free, sizeof(t_envgen), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(envgen_class, nullfn, gensym("signal"), 0);
    class_addmethod(envgen_class, (t_method)envgen_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(envgen_class, envgen_float);
    class_addlist(envgen_class, envgen_list);
    class_addbang(envgen_class, envgen_bang);
    class_addmethod(envgen_class, (t_method)envgen_gain, gensym("gain"), A_FLOAT, 0);
    class_addmethod(envgen_class, (t_method)envgen_set, gensym("set"), A_GIMME, 0);
    class_addmethod(envgen_class, (t_method)envgen_pause, gensym("pause"), 0);
    class_addmethod(envgen_class, (t_method)envgen_resume, gensym("resume"), 0);
}
