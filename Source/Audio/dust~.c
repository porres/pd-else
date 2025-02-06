// Porres 2017-2024

#include <m_pd.h>
#include <stdlib.h>
#include "random.h"
#include "magic.h"

#define MAXLEN 1024

static t_class *dust_class;

typedef struct _dust{
    t_object       x_obj;
    t_float        x_sample_dur;
    t_random_state x_rstate;
    float         *x_density_list;
    t_int          x_list_size;
    t_int          x_sig1;
    int            x_id;
    int            x_nchans;
    int            x_ch;
    int            x_n;
    t_glist    *x_glist; // object list
    t_symbol      *x_ignore;
}t_dust;

static void dust_seed(t_dust *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
}

static void dust_ch(t_dust *x, t_floatarg f){
    x->x_ch = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static void dust_set(t_dust *x, t_symbol *s, int ac, t_atom *av){
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
    x->x_density_list[i] = f;
}

static void dust_list(t_dust *x, t_symbol *s, int ac, t_atom * av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_list_size != ac){
        x->x_list_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_density_list[i] = atom_getfloat(av+i);
}

static t_int *dust_perform(t_int *w){
    t_dust *x = (t_dust *)(w[1]);
    int chs = (t_int)(w[2]); // number of channels in main input signal (density)
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_sample *)(w[4]);
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    for(int i = 0; i < x->x_n; i++){
        for(int j = 0; j < x->x_nchans; j++){ // for 'n' out channels
//            t_float density = chs == 1 ? in[i] : in[j*x->x_n + i];
            t_float density;
            if(x->x_sig1){
                if(chs == 1)
                    density = in[i];
                else
                    density = in[j*x->x_n + i];
            }
            else{
                if(x->x_list_size > 1)
                    density = x->x_density_list[j];
                else
                    density = x->x_density_list[0];
            }
            t_float thresh = density * x->x_sample_dur;
            t_float scale = thresh > 0 ? 1./thresh : 0;
            t_float random = (t_float)(random_frand(s1, s2, s3) * 0.5 + 0.5);
            t_float output = random < thresh ? random * scale : 0;
            out[j*x->x_n + i] = output;
        }
    }
    return(w+5);
}

static void *dust_free(t_dust *x){
    free(x->x_density_list);
    return(void *)x;
}

static void dust_dsp(t_dust *x, t_signal **sp){
    x->x_sample_dur = 1./sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int chs = x->x_sig1 ? sp[0]->s_nchans : x->x_list_size;
    if(chs == 1)
        chs = x->x_ch;
    x->x_nchans = chs;
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(dust_perform, 4, x, sp[0]->s_nchans, sp[0]->s_vec, sp[1]->s_vec);
}

static void *dust_new(t_symbol *s, int ac, t_atom *av){
    t_dust *x = (t_dust *)pd_new(dust_class);
    x->x_id = random_get_id();
    x->x_nchans = 1;
    dust_seed(x, s, 0, NULL);
    x->x_ch = 1;
    x->x_density_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_density_list[0] = 0;
    x->x_list_size = 1;
    if(ac){
        while(av->a_type == A_SYMBOL){
            t_symbol *sym = atom_getsymbol(av);
            if(sym == gensym("-seed")){
                if(ac >= 2){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    dust_seed(x, s, 1, at);
                }
                else{
                    pd_error(x, "[dust~]: -seed needs a seed value");
                    return(NULL);
                }
            }
            else if(sym == gensym("-ch")){
                if(ac >= 2){
                    int n = atom_getint(av+1);
                    dust_ch(x, n < 1 ? 1 : n);
                    ac-=2, av+=2;
                }
                else{
                    pd_error(x, "[dust~]: -ch needs a channel number value");
                    return(NULL);
                }
            }
            else if(sym == gensym("-mc")){
                ac--, av++;
                if(!ac || av->a_type != A_FLOAT)
                    goto errstate;
                int n = 0;
                while(ac && av->a_type == A_FLOAT){
                    x->x_density_list[n] = atom_getfloat(av);
                    ac--, av++, n++;
                }
                x->x_list_size = n;
            }
            else{
            errstate:
                pd_error(x, "[dust~]: improper args");
                return(NULL);
            }
        }
    }
    if(ac)
        x->x_density_list[0] = atom_getfloat(av);
    x->x_glist = canvas_getcurrent();
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void dust_tilde_setup(void){
    dust_class = class_new(gensym("dust~"), (t_newmethod)dust_new,
        0, sizeof(t_dust), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(dust_class, nullfn, gensym("signal"), 0);
    class_addmethod(dust_class, (t_method)dust_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(dust_class, dust_list);
    class_addmethod(dust_class, (t_method)dust_set, gensym("set"), A_GIMME, 0);
    class_addmethod(dust_class, (t_method)dust_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(dust_class, (t_method)dust_ch, gensym("ch"), A_DEFFLOAT, 0);
}
