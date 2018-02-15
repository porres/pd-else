// porres 2018

#include <stdlib.h>
#include <string.h>
#include <m_pd.h>

#define BYTES_GET 256

static t_class *toany_class;

typedef struct _toany_bytes{
    unsigned char *b_buf;    //-- byte-string buffer
    int            b_len;    //-- current length of b_buf
    size_t         b_alloc;  //-- allocated size of b_buf
}t_toany_bytes;

typedef struct _toany_atoms{
    t_atom        *a_buf;    //-- t_atom buffer (aka argv)
    int            a_len;    //-- current length of a_buf (aka argc)
    size_t         a_alloc;  //-- allocated size of a_buf
} t_toany_atoms;


typedef struct _toany{
  t_object       x_obj;
  t_toany_bytes x_bytes; //-- byte buffer: {b_buf~x_text,b_len~?,b_alloc~x_size}
  t_float        x_eos;     //-- eos byte value
  t_binbuf      *x_binbuf;
  t_inlet       *x_eos_in;
  t_outlet      *x_outlet;
}t_toany;

////////////////////////////////////////////////////////////////////////////////////////////////////

toany_atoms_clear(t_toany_atoms *a){
    if (a->a_alloc)
        freebytes(a->a_buf, (a->a_alloc)*sizeof(t_atom));
    a->a_buf   = NULL;
    a->a_len   = 0;
    a->a_alloc = 0;
}

toany_atoms_realloc(t_toany_atoms *a, size_t n){
    toany_atoms_clear(a);
    a->a_buf   = n ? (t_atom*)getbytes(n*sizeof(t_atom)) : NULL;
    a->a_alloc = n;
}

toany_bytes_clear(t_toany_bytes *b){
    if(b->b_alloc)
        freebytes(b->b_buf, (b->b_alloc)*sizeof(unsigned char));
    b->b_buf   = NULL;
    b->b_len   = 0;
    b->b_alloc = 0;
}

toany_bytes_realloc(t_toany_bytes *b, size_t n){
    toany_bytes_clear(b);
    b->b_buf   = n ? (unsigned char*)getbytes(n*sizeof(unsigned char)) : NULL;
    b->b_alloc = n;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

convert_toany(void *x, t_toany_atoms *dst, t_toany_bytes *src, t_binbuf *x_binbuf){
    int bb_is_tmp = 0;     //-- create temporary binbuf?
    if (!x_binbuf) {
        x_binbuf = binbuf_new();
        bb_is_tmp = 1;
    }
    binbuf_clear(x_binbuf);    //-- populate binbuf
    binbuf_text(x_binbuf, (char*)src->b_buf, src->b_len);     //-- populate atom list
    if(bb_is_tmp) {         //-- temporary binbuf: copy atoms
        t_atom *argv = binbuf_getvec(x_binbuf);
        int     argc = binbuf_getnatom(x_binbuf);
        if(dst->a_alloc < (size_t)argc)         //-- reallocate?
            toany_atoms_realloc(dst, argc + BYTES_GET);
        memcpy(dst->a_buf, argv, argc*sizeof(t_atom));         //-- copy
        dst->a_len = argc;
        binbuf_free(x_binbuf);        //-- cleanup
    }
    else if(dst){ //-- permanent binbuf: clobber dst
        dst->a_buf = binbuf_getvec(x_binbuf);
        dst->a_len = binbuf_getnatom(x_binbuf);
        dst->a_alloc = 0;  //-- don't try to free this
    }
}

toany_atoms2bytes(void *x, t_toany_bytes *dst, t_toany_atoms *src, t_float x_eos){
    t_atom *argv = src->a_buf;
    int     argc = src->a_len;
    unsigned char *s;
    int     new_len = 0;
    if(dst->b_alloc <= (size_t)(argc+1)) // re-allocate?
        toany_bytes_realloc(dst, argc + 1 + BYTES_GET);
    for (s=dst->b_buf, new_len=0; argc > 0; argc--, argv++, s++, new_len++){ // get byte string
        *s = atom_getfloat(argv);
        if((x_eos<0 && !*s) || (*s==x_eos))
            break; // truncate at first EOS char
    }
    *s = '\0'; // always append terminating NUL
    dst->b_len = new_len;
    return new_len+1;
}

static void toany_atoms(t_toany *x, int argc, t_atom *argv){
// DEPENDENCY t_toany_atoms / toany_atoms2bytes
  t_toany_atoms src = {argv,argc,argc};
  toany_atoms2bytes(x, &(x->x_bytes), &src, x->x_eos);
  convert_toany(x, NULL, &(x->x_bytes), x->x_binbuf);
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
    toany_bytes_clear(&x->x_bytes);
    toany_bytes_realloc(&x->x_bytes, bufsize);
    x->x_eos_in = floatinlet_new(&x->x_obj, &x->x_eos);
    x->x_outlet = outlet_new(&x->x_obj, &s_list);
    return (void *)x;
}

static void toany_free(t_toany *x){
  toany_bytes_clear(&x->x_bytes);
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
