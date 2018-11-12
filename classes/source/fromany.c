// porres 2018

#include <stdlib.h>
#include <string.h>
#include <m_pd.h>

static t_class *fromany_class;

typedef struct _fromany_char_code{
    unsigned char *b_buf;    // byte-string buffer
    int            b_len;    // current length of b_buf
    size_t         b_alloc;  // allocated size of b_buf
}t_fromany_char_code;

typedef struct _fromany_atoms{
    t_atom        *a_buf;    // t_atom buffer (argv)
    int            a_len;    // current length (argc)
    size_t         a_alloc;  // allocated size of a_buf
}t_fromany_atoms;

typedef struct _fromany{
  t_object       x_obj;
  t_fromany_char_code x_char_code;   // byte buffer
  t_fromany_atoms x_atoms;   // atom buffer (for output)
  t_binbuf        *x_binbuf;
  t_outlet        *x_outlet;
}t_fromany;

////////////////////////////////////////////////////////////////////////////////////////////////////

static fromany_atoms_clear(t_fromany_atoms *a){
    if(a->a_alloc)
        freebytes(a->a_buf, (a->a_alloc)*sizeof(t_atom));
    a->a_buf   = NULL;
    a->a_len   = a->a_alloc = 0;
}

static fromany_atoms_realloc(t_fromany_atoms *a, size_t n){
    fromany_atoms_clear(a);
    a->a_buf   = n ? (t_atom*)getbytes(n*sizeof(t_atom)) : NULL;
    a->a_alloc = n;
}

static fromany_char_code_clear(t_fromany_char_code *b){
    if(b->b_alloc)
        freebytes(b->b_buf, (b->b_alloc)*sizeof(unsigned char));
    b->b_buf   = NULL;
    b->b_len   = b->b_alloc = 0;
}

static fromany_char_code_realloc(t_fromany_char_code *b, size_t n){
    fromany_char_code_clear(b);
    b->b_buf   = n ? (unsigned char*)getbytes(n*sizeof(unsigned char)) : NULL;
    b->b_alloc = n;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static get_atoms(void *x, t_fromany_atoms *dst, t_fromany_char_code *src){
    int i;
    if(dst->a_alloc <= (size_t)src->b_len) // re-allocate?
        fromany_atoms_realloc(dst, src->b_len + 1 + 256);
    for(i = 0; i < src->b_len; i++) // convert
        SETFLOAT((dst->a_buf+i), src->b_buf[i]);
    dst->a_len = src->b_len;
}

static convert_fromany(void *x, t_fromany_char_code *dst, t_symbol *sel, t_fromany_atoms *src, t_binbuf *x_binbuf){
    int bb_is_tmp = 0;
    if(!x_binbuf){ // create temporary binbuf
        x_binbuf = binbuf_new();
        bb_is_tmp = 1;
    }
    binbuf_clear(x_binbuf); // prepare binbuf
    if(sel && sel != &s_float && sel != &s_list && sel != &s_ && sel != &s_symbol){ // add selector
        t_atom a;
        SETSYMBOL((&a), sel);
        binbuf_add(x_binbuf, 1, &a);
    }
    binbuf_add(x_binbuf, src->a_len, src->a_buf); // binbuf_add(): src atoms
    if(bb_is_tmp){   // output: get text string
        char *text;  // temporary binbuf: copy text
        int   len;
        binbuf_gettext(x_binbuf, &text, &len);  // reallocate?
        if(dst->b_alloc < (size_t)len)
            fromany_char_code_realloc(dst, len + 256);
        memcpy(dst->b_buf, text, len*sizeof(char)); // copy
        dst->b_len = len;
        binbuf_free(x_binbuf); // cleanup
        if(text)
            freebytes(text,len);
    }
    else if(dst){ // permanent binbuf: clobber dst
        fromany_char_code_clear(dst);
        binbuf_gettext(x_binbuf, ((char**)((void*)(&dst->b_buf))), &dst->b_len);
        dst->b_alloc = dst->b_len;
    }
}

static void fromany_anything(t_fromany *x, t_symbol *sel, int argc, t_atom *argv){
  t_fromany_atoms arg_atoms = {argv, argc, 0};
  fromany_char_code_clear(&x->x_char_code);
  convert_fromany(x, &x->x_char_code, sel, &arg_atoms, x->x_binbuf);
  get_atoms(x, &x->x_atoms, &x->x_char_code);
  outlet_list(x->x_outlet, &s_list, x->x_atoms.a_len, x->x_atoms.a_buf);
}

static void *fromany_new(void){
    t_fromany *x = (t_fromany *)pd_new(fromany_class);
    int bufsize = 256;
    fromany_char_code_clear(&x->x_char_code);
    fromany_char_code_realloc(&x->x_char_code, 0);
    fromany_atoms_clear(&x->x_atoms);
    fromany_atoms_realloc(&x->x_atoms, bufsize);
    x->x_binbuf = binbuf_new();
    x->x_outlet = outlet_new(&x->x_obj, &s_list);
    return(void *)x;
}

static void fromany_free(t_fromany *x){
  fromany_char_code_clear(&x->x_char_code);
  fromany_atoms_clear(&x->x_atoms);
  binbuf_free(x->x_binbuf);
  outlet_free(x->x_outlet);
  return;
}

void fromany_setup(void){
    fromany_class = class_new(gensym("fromany"), (t_newmethod)fromany_new,
            (t_method)fromany_free, sizeof(t_fromany), CLASS_DEFAULT, 0);
    class_addanything(fromany_class, (t_method)fromany_anything);
}
