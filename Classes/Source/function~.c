// porres 2018

#include <m_pd.h>

#define MAX_SIZE  1024

typedef struct _function{
    t_object    x_obj;
    float       x_f;
    float      *x_finalvalues;
    float      *x_duration;
    int         x_state;
    int         x_last_state;
}t_function;

static t_class *function_class;

static t_sample function_interpolate(t_function* x, t_sample f){
    return(x->x_finalvalues[x->x_state-1] +
    (f - x->x_duration[x->x_state-1]) *
    (x->x_finalvalues[x->x_state] - x->x_finalvalues[x->x_state-1]) /
    (x->x_duration[x->x_state] - x->x_duration[x->x_state-1]));
}
    
static t_int *functionsig_perform(t_int *w){
	t_function *x = (t_function *)(w[1]);
    t_sample *in = (t_float *)(w[2]);
    t_sample *out = (t_float *)(w[3]);
    int n = (int)(w[4]);
    if(x->x_state > x->x_last_state)
        x->x_state = x->x_last_state;
    while(n--){
        t_sample f = *in++;
        t_sample val;
        while((x->x_state > 0) && (f < x->x_duration[x->x_state-1]))
            x->x_state--;
        while((x->x_state <  x->x_last_state) && (x->x_duration[x->x_state] < f))
            x->x_state++;
        if(x->x_state == 0 || f >= x->x_duration[x->x_last_state])
            val = x->x_finalvalues[x->x_state];
        else
            val = function_interpolate(x, f);
        *out++ = val;
    }
    return(w+5);
}

static void functionsig_dsp(t_function *x, t_signal **sp){
    dsp_add(functionsig_perform, 4, x,sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

static void function_init(t_function *x, int ac, t_atom* av){ // ???
    if(ac < 3){
        post("[function~] list needs at least 3 floats");
        return;
    }
    float *dur;
    float *val;
    float tdur = 0;
    x->x_duration[0] = 0;
    x->x_last_state = ac >> 1; // = (int)ac/2
    if(x->x_last_state > MAX_SIZE){
        post("[function]: too many lines, maximum is %d", MAX_SIZE);
        return;
    }
    dur = x->x_duration;
    val = x->x_finalvalues;
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
    for(int i = 1; i <= x->x_last_state; i++)
        x->x_duration[i] /= x->x_duration[x->x_last_state];
}

static void function_list(t_function *x,t_symbol* s, int ac,t_atom* av){
    s = NULL;
    function_init(x, ac, av);
    function_norm_dur(x);
}

static void *function_new(t_symbol *s,int ac,t_atom* av){
    t_function *x = (t_function *)pd_new(function_class);
    s = NULL;
    x->x_f = 0;
    x->x_finalvalues = getbytes(MAX_SIZE*sizeof(float));
    x->x_duration = getbytes(MAX_SIZE*sizeof(float));
    if(ac){
        if(ac < 3)
            pd_error(x, "[function~]: needs at least 3 float arguments");
        else
            function_init(x, ac, av);
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
