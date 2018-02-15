#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <string.h>
#include <m_pd.h>
#include "charstring.h"

static t_class *fromany_class;

typedef struct _fromany{
  t_object       x_obj;
  t_pdstring_bytes x_bytes;   //-- byte buffer
  t_pdstring_atoms x_atoms;   //-- atom buffer (for output)
  t_binbuf        *x_binbuf;
  t_outlet        *x_outlet;
}t_fromany;

static void fromany_anything(t_fromany *x, t_symbol *sel, int argc, t_atom *argv){
  t_pdstring_atoms arg_atoms = {argv,argc,0};
  pdstring_bytes_clear(&x->x_bytes);
  pdstring_fromany(x, &x->x_bytes, sel, &arg_atoms, x->x_binbuf);
  pdstring_bytes2atoms(x, &x->x_atoms, &x->x_bytes, -1); // -1 is EOS
  outlet_list(x->x_outlet, &s_list, x->x_atoms.a_len, x->x_atoms.a_buf);
}

static void *fromany_new(void){
    t_fromany *x = (t_fromany *)pd_new(fromany_class);
    int bufsize = 256;
    pdstring_bytes_init(&x->x_bytes, 0); //-- x_bytes gets clobbered by binbuf_gettext()
    pdstring_atoms_init(&x->x_atoms, bufsize);
    x->x_binbuf = binbuf_new();
    x->x_outlet = outlet_new(&x->x_obj, &s_list);
    return (void *)x;
}

static void fromany_free(t_fromany *x){
  pdstring_bytes_clear(&x->x_bytes);
  pdstring_atoms_clear(&x->x_atoms);
  binbuf_free(x->x_binbuf);
  outlet_free(x->x_outlet);
  return;
}

void fromany_setup(void){
    fromany_class = class_new(gensym("fromany"), (t_newmethod)fromany_new,
            (t_method)fromany_free, sizeof(t_fromany), CLASS_DEFAULT, 0);
    class_addanything(fromany_class, (t_method)fromany_anything);
}
