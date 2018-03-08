// Porres 2018

#include "m_pd.h"
#include "m_imp.h"
#include "g_canvas.h"
#include <string.h>

// * from m_obj.c: * //

union inletunion{
    t_symbol    *iu_symto;
    t_gpointer  *iu_pointerslot;
    t_float     *iu_floatslot;
    t_symbol   **iu_symslot;
    t_sample     iu_floatsignalvalue;
};

struct _inlet{
    t_pd             i_pd;
    struct _inlet   *i_next;
    t_object        *i_owner;
    t_pd            *i_dest;
    t_symbol        *i_symfrom;
    union inletunion i_un;
};

void signal_setborrowed(t_signal *sig, t_signal *sig2);
void signal_makereusable(t_signal *sig);

// make a variant of inlet~ (original code in g_io.c)

t_class *vinlet_class;

typedef struct _vinlet{
    t_object    x_obj;
    t_canvas   *x_canvas;
    t_inlet    *x_inlet;
    int         x_bufsize;
    t_float    *x_buf; 	    // signal buffer; zero if not a signal
    t_float    *x_endbuf;
    t_float    *x_fill;
    t_float    *x_read;
    int         x_hop;
    /* if not reblocking, the next slot communicates the parent's inlet
    signal from the prolog to the DSP routine: */
    t_signal   *x_directsignal;
}t_vinlet;

static void *inlet2_new(t_floatarg f){
    t_vinlet *x = (t_vinlet *)pd_new(vinlet_class);
    x->x_canvas = canvas_getcurrent();
    x->x_inlet = canvas_addinlet(x->x_canvas, &x->x_obj.ob_pd, &s_signal);
    x->x_endbuf = x->x_buf = (t_float *)getbytes(0);
    x->x_bufsize = 0;
    x->x_directsignal = 0;
    x->x_inlet->i_un.iu_floatsignalvalue = f;
    floatinlet_new((t_object *)x, &x->x_inlet->i_un.iu_floatsignalvalue);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void inlet2_tilde_setup(void){
    class_addcreator((t_newmethod)inlet2_new, gensym("inlet2~"), A_DEFFLOAT, 0);
    class_addcreator((t_newmethod)inlet2_new, gensym("else/inlet2~"), A_DEFFLOAT, 0);

}
