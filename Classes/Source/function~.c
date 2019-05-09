// porres 2018

#include <m_pd.h>

#define NONE    0
#define STATES  512

typedef struct _function{
    t_object    x_obj;
    t_float     x_val;
    t_float     x_f;
    t_int       x_state;
    t_int       last_state;
    t_float*    finalvalues;
    t_float*    duration;
    t_int       args;          // Get rid of this
    t_int       state;
}t_function;

static t_class *function_class;

static t_int *functionsig_perform(t_int *w){
	t_function *x = (t_function *)(w[1]);
    t_sample *in = (t_float *)(w[2]);
    t_sample *out = (t_float *)(w[3]);
    int n = (int)(w[4]);
    if(x->state > x->last_state) // ???
        x->state = x->last_state;
    while(n--){
        t_sample f = *in++;
        t_sample val;
        while((x->state > 0) && (f < x->duration[x->state-1]))
            x->state--;
        while((x->state <  x->last_state) && (x->duration[x->state] < f))
            x->state++;
        if (x->state == 0 || f >= x->duration[x->last_state]) // Interpolate
            val = x->finalvalues[x->state];
        else{
            val = x->finalvalues[x->state-1] +
            (f - x->duration[x->state-1]) *
            (x->finalvalues[x->state] - x->finalvalues[x->state-1]) /
            (x->duration[x->state] - x->duration[x->state-1]);
        }
        *out++ = val;
    }
    return(w + 5);
}

static void functionsig_dsp(t_function *x, t_signal **sp){
    dsp_add(functionsig_perform, 4, x,sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

static void function_resize(t_function* x,int ns){ // ???
    if(ns > x->args){
        int newargs = ns*sizeof(t_float);
        x->duration = resizebytes(x->duration,x->args*sizeof(t_float),newargs);
        x->finalvalues = resizebytes(x->finalvalues,x->args*sizeof(t_float),newargs);
        x->args = ns;
    }
}

static void function_norm_dur(t_function* x){ // normalize duration
    for(int i = 1; i <= x->last_state; i++)
        x->duration[i] /= x->duration[x->last_state];
}

static void function_init(t_function *x,int argc,t_atom* argv){ // ???
    t_float* dur;
    t_float* val;
    t_float tdur = 0;
    if(!argc)
        return;
    x->duration[0] = 0;
    x->last_state = argc >> 1;
    function_resize(x, argc >> 1);
    dur = x->duration;
    val = x->finalvalues;
    if(argc){
        *val = atom_getfloat(argv++);
        *dur = 0.0;
    }
    dur++; val++; argc--;
    for (;argc > 0; argc--){
        tdur += atom_getfloat(argv++);
        *dur++ = tdur;
        argc--;
        if(argc > 0){
            *val = atom_getfloat(argv++);
            *val++;
        }
        else{
            *val = 0;
            *val++;
        }
    }
}

static void function_list(t_function *x,t_symbol* s, int argc,t_atom* argv){
    t_symbol *dummy = s;
    dummy = NULL;
    function_init(x, argc, argv);
    function_norm_dur(x);
}

static void *function_new(t_symbol *s,int argc,t_atom* argv){
    t_function *x = (t_function *)pd_new(function_class);
    t_symbol *dummy = s;
    dummy = NULL;
    x->x_f = 0;
    x->args = STATES;
    x->finalvalues = getbytes( x->args*sizeof(t_float));
    x->duration = getbytes( x->args*sizeof(t_float));
    x->x_val = 0.0;
    x->x_state = NONE;
    if(argc){
        if(argc < 3)
            post("[function~] needs at least 3 arguments");
        if(argc >= 3)
            function_init(x, argc, argv);
    }
    function_norm_dur(x);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void function_tilde_setup(void){
    function_class = class_new(gensym("function~"), (t_newmethod)function_new, 0,
    	sizeof(t_function), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(function_class, t_function, x_f);
    class_addmethod(function_class, (t_method)functionsig_dsp, gensym("dsp"), 0);
    class_addlist(function_class, function_list);
}