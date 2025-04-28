// porres 2018-2025

#include <m_pd.h>
#include "buffer.h"
#include <stdlib.h>
#include <math.h>

#define MAX_SIZE  4096
#define LOG001 log(0.001)

enum { LINEAR, CURVE, POWER, LAG, SINE, DUMMY, HANN };

typedef struct _function{
    t_object    x_obj;
    t_float     x_f;
    int         x_nchans;
    int         x_nblock;
    float      *x_points;
    float      *x_dur;
    t_atom      x_at_exp[MAX_SIZE];
    t_atom      x_at_av[MAX_SIZE];
    int         x_single_exp;
    float       x_curve;
    int         x_lintype;
    int         x_last_n;
    int        *x_n;
    t_symbol   *x_ignore;
}t_function;

static t_class *function_class;

static void function_set_curve(t_function *x, int n){
    if(x->x_single_exp)
        n = 0;
    if(x->x_at_exp[n].a_type == A_FLOAT){
        x->x_curve = x->x_at_exp[n].a_w.w_float * -4;
        x->x_lintype = fabs(x->x_curve) > 0.001 ? CURVE : LINEAR;
    }
    else{
        char p[MAXPDSTRING];
        t_symbol *sym = x->x_at_exp[n].a_w.w_symbol;
        if(sym == gensym("lin")){
            x->x_lintype = LINEAR;
            return;
        }
        else if(sym == gensym("lag")){
            x->x_lintype = LAG;
            return;
        }
        else if(sym == gensym("sin")){
            x->x_lintype = SINE;
            return;
        }
        else if(sym == gensym("hann")){
            x->x_lintype = HANN;
            return;
        }
        sprintf(p, "%s", sym->s_name);
        if(p[0] == '^'){
            x->x_curve = atof(p + 1); // convert after '^'
            if(x->x_curve == 0 || fabs(x->x_curve) == 1)
                x->x_lintype = LINEAR;
            else
                x->x_lintype = POWER;
        }
        else{
            post("[function~]: unknown curve type, setting to linear");
            x->x_lintype = LINEAR;
            return;
        }
    }
}

static float function_getval(t_function *x, float phase, int n){
    function_set_curve(x, n-1);
    float b = x->x_points[n-1], c = x->x_points[n];
    float dur_m1 = x->x_dur[n-1], dur = x->x_dur[n];
    float frac = (phase-dur_m1)/(dur-dur_m1);
    if(x->x_lintype == SINE || x->x_lintype == HANN){
        float d = c-b, i = d > 0 ? frac : 1.0 - frac;
        float sin = read_fadetab(i, x->x_lintype);
        float step = d > 0 ? sin * d : (1 - sin) * d;
        return(b + step);
    }
    else if(x->x_lintype == CURVE){
        float d = c - b;
        float b0 = d / (1.0f - expf(x->x_curve));
        float a = b + b0;
        float b_phase = b0 * expf(x->x_curve * frac);
        return(a - b_phase);
    }
    else if (x->x_lintype == LAG) {
        float d = b - c; 
        return(c + d * expf(LOG001 * frac));
    }
    else if(x->x_lintype == POWER){
        return(interp_pow(frac, b, c, x->x_curve));
    }
    else // if(x->x_lintype == LINEAR){
        return(interp_lin(frac, b, c));
}
    
static t_int *function_perform(t_int *w){
    t_function *x = (t_function *)(w[1]);
    t_sample *in = (t_float *)(w[2]);
    t_sample *out = (t_float *)(w[3]);
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < x->x_nblock; i++){
            t_float phase = in[j*x->x_nblock + i], val;
            int n = x->x_n[j], last = x->x_last_n;
            if(n > last)
               n = last;
            while((n > 0) && (phase < x->x_dur[n-1]))
                n--;
            while((n < last) && (x->x_dur[n] < phase))
                n++;
            if(n == 0 || phase >= x->x_dur[last])
                val = x->x_points[n];
            else
                val = function_getval(x, phase, n);
            out[j*x->x_nblock + i] = val;
        }
    }
    return(w+4);
}

static void function_dsp(t_function *x, t_signal **sp){
    x->x_nblock = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(x->x_nchans != chs){
       x->x_n = (int *)resizebytes(x->x_n,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], chs);
    dsp_add(function_perform, 3, x,sp[0]->s_vec, sp[1]->s_vec);
}

static void function_init(t_function *x, int ac, t_atom* av){
    if(ac < 3)
        return;
    float *dur, *val, tdur = 0;
    x->x_dur[0] = 0;
    x->x_last_n = (int)ac/2;
    if(x->x_last_n > MAX_SIZE){
        post("[function~]: too many lines, maximum is %d", MAX_SIZE);
        return;
    }
    dur = x->x_dur;
    val = x->x_points;
    *val = atom_getfloat(av++);
    *dur = 0.0;
    ac--;
    dur++;
    while(ac > 0){
        *dur++ = (tdur += atom_getfloat(av++));
        ac--;
        *++val = ac > 0 ? atom_getfloat(av++): 0;
        ac--;
    }
}

static void function_norm_dur(t_function* x){ // normalize duration
    for(int i = 1; i <= x->x_last_n; i++)
        x->x_dur[i] /= x->x_dur[x->x_last_n];
}

static void function_curve(t_function *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        return;
    x->x_single_exp = (ac == 1);
    for(int i = 0; i < ac; i++){
        if((av+i)->a_type == A_FLOAT)
            SETFLOAT(x->x_at_exp+i, (av+i)->a_w.w_float);
        else
            SETSYMBOL(x->x_at_exp+i, (av+i)->a_w.w_symbol);
    }
}

static void function_list(t_function *x,t_symbol* s, int ac,t_atom* av){
    x->x_ignore = s;
    function_init(x, ac, av);
    function_norm_dur(x);
}

static void function_free(t_function *x){
    freebytes(x->x_n, x->x_nchans * sizeof(*x->x_n));
}

static void *function_new(t_symbol *s,int ac,t_atom* av){
    t_function *x = (t_function *)pd_new(function_class);
    x->x_ignore = s;
    init_fade_tables();
    for(int i = 0; i < MAX_SIZE; i++) // set exponential list to linear
        SETFLOAT(x->x_at_exp+i, 0);
    x->x_single_exp = 1;
    x->x_f = 0;
    x->x_points = getbytes(MAX_SIZE*sizeof(float));
    x->x_dur = getbytes(MAX_SIZE*sizeof(float));
    x->x_n = (int *)getbytes(sizeof(*x->x_n));
    x->x_nchans = 1;
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-curve")){
                if(ac >= 2){
                    ac--, av++;
                    function_curve(x, NULL, 1, av);
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        if(av->a_type == A_FLOAT){
            if(ac < 3){
                pd_error(x, "[function~]: needs at least 3 float arguments");
                goto errstate;
            }
            else{
                function_init(x, ac, av);
                break;
            }
        }
    }
    function_norm_dur(x);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
errstate:
    pd_error(x, "[function~]: improper args");
    return(NULL);
}

void function_tilde_setup(void){
    function_class = class_new(gensym("function~"), (t_newmethod)function_new,
        (t_method)function_free, sizeof(t_function), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(function_class, t_function, x_f);
    class_addlist(function_class, function_list);
    class_addmethod(function_class, (t_method)function_dsp, gensym("dsp"), 0);
    class_addmethod(function_class, (t_method)function_curve, gensym("curve"), A_GIMME, 0);
}
