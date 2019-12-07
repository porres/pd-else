// porres 2019

#include "m_pd.h"
#include "g_canvas.h"

static t_class *edit_class, *edit_proxy_class;

typedef struct _edit_proxy{
    t_object    p_obj;
    t_symbol   *p_sym;
    t_clock    *p_clock;
    struct      _edit *p_cnv;
}t_edit_proxy;

typedef struct _edit{
    t_object        x_obj;
    t_edit_proxy   *x_proxy;
    t_canvas       *x_canvas;
    int             x_edit;
}t_edit;

static void edit_loadbang(t_edit *x, t_float f){
    if((int)f == LB_LOAD){
        x->x_edit = x->x_canvas->gl_edit;
        outlet_float(x->x_obj.ob_outlet, x->x_edit);
    }
}

/* static void edit_bang(t_edit *x){
    outlet_float(x->x_obj.ob_outlet, x->x_edit);
}*/

static void edit_proxy_any(t_edit_proxy *p, t_symbol *s, int ac, t_atom *av){
    ac = 0;
    if(p->p_cnv && s == gensym("editmode"))
        outlet_float(p->p_cnv->x_obj.ob_outlet, p->p_cnv->x_edit = (int)(av->a_w.w_float));
}

static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy * edit_proxy_new(t_edit *x, t_symbol*s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

static void edit_free(t_edit *x){
    x->x_proxy->p_cnv = NULL;
    clock_delay(x->x_proxy->p_clock, 0);
}

static void *edit_new(t_floatarg f1){
    t_edit *x = (t_edit *)pd_new(edit_class);
    t_glist *glist = (t_glist *)canvas_getcurrent();
    t_canvas *canvas = (t_canvas*)glist_getcanvas(glist);
    x->x_canvas = canvas;
    int depth = f1 < 0 ? 0 : (int)f1;
    while(depth--){
        if(canvas->gl_owner){
            x->x_canvas = canvas;
            canvas = canvas->gl_owner;
        }
    }
    x->x_edit = canvas->gl_edit;
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)canvas);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    outlet_new(&x->x_obj, 0);
    return(x);
}

void setup_canvas0x2eedit(void){
    edit_class = class_new(gensym("canvas.edit"), (t_newmethod)edit_new,
        (t_method)edit_free, sizeof(t_edit), CLASS_NOINLET, A_DEFFLOAT, 0);
//    class_addbang(edit_class, edit_bang);
    class_addmethod(edit_class, (t_method)edit_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy),
        CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
}
