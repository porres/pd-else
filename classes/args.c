#include "m_pd.h"
#include "g_canvas.h"

static t_class *args_class;

typedef struct _args{
  t_object  x_obj;
  t_canvas  *x_canvas;
} t_args;

static void args_bang(t_args *x){
  int argc = 0;
  t_atom*argv = 0;
  t_binbuf*b = 0;
  if(!x->x_canvas)
      return;
  b = x->x_canvas->gl_obj.te_binbuf;
  if(b){
    argc = binbuf_getnatom(b) - 1;
    argv = binbuf_getvec(b) + 1;
  }
  else{
    canvas_setcurrent(x->x_canvas);
    canvas_getargs(&argc, &argv);
    canvas_unsetcurrent(x->x_canvas);
  }
  if(argv)
    outlet_list(x->x_obj.ob_outlet, &s_list, argc, argv);
}

static void args_free(t_args *x){
  x->x_canvas = 0;
}

static void *args_new(void){
  t_args *x = (t_args *)pd_new(args_class);
  t_glist *glist=(t_glist *)canvas_getcurrent();
  x->x_canvas = (t_canvas*)glist_getcanvas(glist);
  outlet_new(&x->x_obj, 0);
  return (x);
}

void args_setup(void){
  args_class = class_new(gensym("args"), (t_newmethod)args_new,
        (t_method)args_free, sizeof(t_args), 0, 0);
  class_addbang(args_class, (t_method)args_bang);
}
