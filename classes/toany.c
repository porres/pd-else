#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <string.h>
#include <m_pd.h>
#include "charstring.h"

static t_class *toany_class;

typedef struct _toany{
  t_object       x_obj;
  t_pdstring_bytes x_bytes; //-- byte buffer: {b_buf~x_text,b_len~?,b_alloc~x_size}
  t_float        x_eos;     //-- eos byte value
  t_binbuf      *x_binbuf;
  t_inlet       *x_eos_in;
  t_outlet      *x_outlet;
} t_toany;

static void toany_atoms(t_toany *x, int argc, t_atom *argv){
  t_pdstring_atoms src = {argv,argc,argc};
  pdstring_atoms2bytes(x, &(x->x_bytes), &src, x->x_eos);
  pdstring_toany(x, NULL, &(x->x_bytes), x->x_binbuf);
  int x_argc;
  t_atom *x_argv;
  x_argc = binbuf_getnatom(x->x_binbuf);
  x_argv = binbuf_getvec(x->x_binbuf);
  if(x_argc && x_argv->a_type == A_SYMBOL)
    outlet_anything(x->x_outlet, x_argv->a_w.w_symbol, x_argc-1, x_argv+1);
  else
    outlet_anything(x->x_outlet, &s_list, x_argc, x_argv);
}

static void toany_list(t_toany *x, t_symbol *sel, int argc, t_atom *argv){
  int i0 = 0, i;
  for(i = 0; i < argc; i++){
      if((argv+i)->a_type != A_FLOAT){
          pd_error(x, "toany: takes only floats in a list");
          return;
      }
  }
  if(x->x_eos >= 0){
    for(i = i0; i < argc; i++){
        if(((int)atom_getfloatarg(i, argc, argv)) == ((int)x->x_eos)){
            toany_atoms(x, i - i0, argv + i0);
            i0 = i + 1;
        }
    }
  }
  if(i0 < argc)
    toany_atoms(x, argc - i0, argv + i0);
}

static void *toany_new(t_symbol *sel, int argc, t_atom *argv){
    t_toany *x = (t_toany *)pd_new(toany_class);
    int bufsize = 256;
    x->x_binbuf = binbuf_new();
    x->x_eos  = -1;
    if(argc)
      x->x_eos = atom_getfloatarg(0, argc, argv);
    pdstring_bytes_init(&x->x_bytes, bufsize);
    x->x_eos_in = floatinlet_new(&x->x_obj, &x->x_eos);
    x->x_outlet = outlet_new(&x->x_obj, &s_list);
    return (void *)x;
}

static void toany_free(t_toany *x){
  pdstring_bytes_clear(&x->x_bytes);
  binbuf_free(x->x_binbuf);
  inlet_free(x->x_eos_in);
  outlet_free(x->x_outlet);
  return;
}

void toany_setup(void){
    toany_class = class_new(gensym("toany"), (t_newmethod)toany_new, (t_method)toany_free,
        sizeof(t_toany), CLASS_DEFAULT, A_GIMME, 0);
    class_addlist(toany_class, (t_method)toany_list);
}
