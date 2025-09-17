// Based on zl.thin

#include <string.h>
#include <stdlib.h>
#include <m_pd.h>

#define INISIZE 128
#define MAXSIZE 2147483647

typedef struct _uniquedata{
    int      d_size;        // as allocated
    int      d_natoms;      // as used
    int		 d_dir;         // direction
    t_atom  *d_buf;
    t_atom   d_bufini[INISIZE];
}t_uniquedata;

typedef struct _unique{
    t_object      x_obj;
    t_uniquedata  x_inbuf1;
    t_uniquedata  x_outbuf1;
    t_uniquedata  x_outbuf2;
    t_outlet     *x_out2;
}t_unique;

static t_class *unique_class;

static int unique_equal(t_atom *a1, t_atom *a2){
    if(a1->a_type == A_FLOAT && a2->a_type == A_FLOAT)
        return(a1->a_w.w_float == a2->a_w.w_float);
    else if(a1->a_type == A_SYMBOL && a2->a_type == A_SYMBOL)
        return(a1->a_w.w_symbol == a2->a_w.w_symbol);
    else
        return(0);
}

static void unique_unique(t_unique *x, int natoms, t_atom *buf){
	if(buf){
        t_atom *av = x->x_inbuf1.d_buf;
        t_atom *buf2 = x->x_outbuf2.d_buf;
        int i, j, total = 0, filtered = 0;
        for(i = 0; i < natoms ; i++){
            for(j = 0; j < total; j++){
                if(unique_equal(&av[i], &buf[j])){
                    SETFLOAT(&buf2[filtered], (t_float)i);
                    filtered++;
                    break;
                }
            }
            if(j == total)
                buf[total++] = av[i];
        }
        x->x_outbuf2.d_natoms = filtered;
        if(filtered > 0)
            outlet_list(x->x_out2, &s_list, filtered, buf2);
        outlet_list(((t_object *)x)->ob_outlet, &s_list, total, buf);
    }
}

static void doit(t_unique *x){
    if(x->x_inbuf1.d_natoms){
        t_uniquedata *d = &x->x_outbuf1;
        unique_unique(x, x->x_inbuf1.d_natoms, d->d_buf);
    }
    else
        pd_error(x, "[unique]: empty buffer, no output");
}

static void unique_realloc(t_uniquedata *d, int size){
    if(size > MAXSIZE)
        size = MAXSIZE;
    if(d->d_buf == d->d_bufini) // !heaped
        d->d_buf = getbytes(size*sizeof(t_atom));
    else // heaped
        d->d_buf = (t_atom *)resizebytes(d->d_buf, d->d_size*sizeof(t_atom), size*sizeof(t_atom));
    d->d_size = size;
}

static void set_size(t_unique *x, t_uniquedata *d, int ac){
    if(ac > d->d_size){
        unique_realloc(&x->x_inbuf1, ac);
        unique_realloc(&x->x_outbuf1, ac);
        unique_realloc(&x->x_outbuf2, ac);
    }
    d->d_natoms = ac > d->d_size ? d->d_size : ac;
}

static void unique_list(t_unique *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        return;
    set_size(x, &x->x_inbuf1, ac);
    memcpy(x->x_inbuf1.d_buf, av, x->x_inbuf1.d_natoms*sizeof(*x->x_inbuf1.d_buf));
    doit(x);
}

static void unique_anything(t_unique *x, t_symbol *s, int ac, t_atom *av){
    set_size(x, &x->x_inbuf1, ac+1);
    SETSYMBOL(x->x_inbuf1.d_buf, s);
    if(ac)
        memcpy(x->x_inbuf1.d_buf+1, av, (x->x_inbuf1.d_natoms-1)*sizeof(*x->x_inbuf1.d_buf));
    doit(x);
}

static void uniquedata_free(t_uniquedata *d){
    if(d->d_buf != d->d_bufini)
        freebytes(d->d_buf, d->d_size*sizeof(*d->d_buf));
}

static void unique_free(t_unique *x){
    uniquedata_free(&x->x_inbuf1);
    uniquedata_free(&x->x_outbuf1);
    uniquedata_free(&x->x_outbuf2);
}

static void uniquedata_init(t_uniquedata *d){
    d->d_natoms = 0;
    d->d_size = INISIZE;
    d->d_buf = d->d_bufini;
}

static void *unique_new(t_floatarg f){
    t_unique *x = (t_unique *)pd_new(unique_class);
    uniquedata_init(&x->x_inbuf1);
    uniquedata_init(&x->x_outbuf1);
    uniquedata_init(&x->x_outbuf2);
    outlet_new((t_object *)x, &s_list);
    x->x_out2 = outlet_new((t_object *)x, &s_list);
    return(x);
}

void unique_setup(void){
    unique_class = class_new(gensym("unique"), (t_newmethod)unique_new,
        (t_method)unique_free, sizeof(t_unique), 0, A_DEFFLOAT, 0);
    class_addlist(unique_class, unique_list);
    class_addanything(unique_class, unique_anything);
}
