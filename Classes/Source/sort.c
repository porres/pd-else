// stolen from cyclone (coded by matt barber)

#include <string.h>
#include <stdlib.h>
#include "m_pd.h"

#define MAXSIZE    32768

typedef struct _sortdata{
    int      d_size;    // as allocated
    int      d_max;     // max size allowed, must be <= d_size
    int      d_natoms;  // as used
    int		 d_sorted;  // -1 for sorted descending, otherwise sorted ascending
    t_atom  *d_buf;
    t_atom   d_bufini[MAXSIZE];
}t_sortdata;

typedef struct _sort{
    t_object    x_ob;
    t_sortdata  x_inbuf1;
    t_sortdata  x_outbuf1;
    t_sortdata  x_outbuf2;
    float       x_dir;
    t_outlet   *x_out2;
}t_sort;

static t_class *sort_class;

// ************************* SORT *********************************

static void sort_swap(t_atom *av, int i, int j){
	t_atom temp = av[j];
	av[j] = av[i];
	av[i] = temp; 
}

static int sort_sort_cmp(t_atom *a1, t_atom *a2){
	if(a1->a_type == A_FLOAT && a2->a_type == A_SYMBOL)
		return (-1);
	if(a1->a_type == A_SYMBOL && a2->a_type == A_FLOAT)
		return (1);
	if(a1->a_type == A_FLOAT && a2->a_type == A_FLOAT){
		if(a1->a_w.w_float < a2->a_w.w_float)
			return (-1);
		if(a1->a_w.w_float > a2->a_w.w_float)
			return (1);
		return (0);
	}
	if(a1->a_type == A_SYMBOL && a2->a_type == A_SYMBOL)
		return (strcmp(a1->a_w.w_symbol->s_name, a2->a_w.w_symbol->s_name));
	if(a1->a_type == A_POINTER)
		return (1);
	if(a2->a_type == A_POINTER)
		return (-1);	
}

static void sort_qsort(t_sort *x, t_atom *av1, t_atom *av2, int left, int right, float dir){
    if(left >= right)
        return;
    sort_swap(av1, left, (left + right)/2);
    if(av2)
    	sort_swap(av2, left, (left + right)/2);
    int last = left;
    for(int i = left+1; i <= right; i++){
        if((dir * sort_sort_cmp(av1 + i, av1 + left)) < 0){
            sort_swap(av1, ++last, i);
            if(av2)
            	sort_swap(av2, last, i);
        }
    }
    sort_swap(av1, left, last);
    if(av2)
    	sort_swap(av2, left, last);
    sort_qsort(x, av1, av2, left, last-1, dir);
    sort_qsort(x, av1, av2, last+1, right, dir);
}

static void sort_sort_rev(int natoms, t_atom *av){
	for(int i = 0, j = natoms - 1; i < natoms/2; i++, j--)
		sort_swap(av, i, j);
}

static void sort_sort(t_sort *x, int natoms, t_atom *buf, int banged){
    if(x->x_dir >= 0)
        x->x_dir = 1;
    else
        x->x_dir = -1;
	if(buf){
    	t_atom *buf2 = x->x_outbuf2.d_buf;
    	x->x_outbuf2.d_natoms = natoms;
    	if(banged){
    		if(x->x_inbuf1.d_sorted != x->x_dir){
    			x->x_inbuf1.d_sorted = x->x_dir;
    			sort_sort_rev(natoms, buf2);
    			sort_sort_rev(natoms, buf);
    		}
            outlet_list(x->x_out2, &s_list, natoms, buf2);
            outlet_list(((t_object *)x)->ob_outlet, &s_list, natoms, buf);
    	}
    	else{
			memcpy(buf, x->x_inbuf1.d_buf, natoms * sizeof(*buf));
			for(int i = 0; i < natoms; i++)
    			SETFLOAT(&buf2[i], i);
    		sort_qsort(x, buf, buf2, 0, natoms - 1, x->x_dir);
    		x->x_inbuf1.d_sorted = x->x_dir;
            outlet_list(x->x_out2, &s_list, natoms, buf2);
            outlet_list(((t_object *)x)->ob_outlet, &s_list, natoms, buf);
		}
    }
}

static void sort_doit(t_sort *x, int banged){
    int natoms = x->x_inbuf1.d_natoms;
    if(natoms < 0)
        return;
    if(natoms){
        t_sortdata *d = &x->x_outbuf1;
        if(natoms > d->d_max) // giving this a shot...
            natoms = d->d_max;
        sort_sort(x, natoms, d->d_buf, banged);
    }
    else
        sort_sort(x, 0, 0, banged);
}

// METHODS **********************************

/* static void sortdata_realloc(t_sortdata *d, int reqsz){
    int cursz = d->d_size;
    int curmax = d->d_max;
    int heaped = d->d_buf != d->d_bufini;
    if(reqsz > MAXSIZE)
        reqsz = MAXSIZE;
    else if(reqsz < 1)
        reqsz = 1;
    if(reqsz <= MAXSIZE && heaped){
        memcpy(d->d_bufini, d->d_buf, MAXSIZE * sizeof(t_atom));
        freebytes(d->d_buf, cursz * sizeof(t_atom));
        d->d_buf = d->d_bufini;
    }
    else if(reqsz > MAXSIZE && !heaped){
        d->d_buf = getbytes(reqsz * sizeof(t_atom));
        memcpy(d->d_buf, d->d_bufini, curmax * sizeof(t_atom));
    }
    else if(reqsz > MAXSIZE && heaped)
        d->d_buf = (t_atom *)resizebytes(d->d_buf, cursz*sizeof(t_atom), reqsz*sizeof(t_atom));
    d->d_max = reqsz;
    if(reqsz < MAXSIZE)
        reqsz = MAXSIZE;
    if(d->d_natoms > d->d_max)
        d->d_natoms = d->d_max;
    d->d_size = reqsz;
}

static void sort_size(t_sort *x, t_floatarg f){
    int sz = (int)f;
    sortdata_realloc(&x->x_inbuf1,sz);
    sortdata_realloc(&x->x_outbuf1,sz);
    sortdata_realloc(&x->x_outbuf2,sz);
} */

static void sort_bang(t_sort *x){
    sort_doit(x, 1);
}

static void sortdata_setlist(t_sortdata *d, int ac, t_atom *av){
    if (ac > d->d_max) ac = d->d_max;
    memcpy(d->d_buf, av, ac * sizeof(*d->d_buf));
    d->d_natoms = ac;
}

static void sort_list(t_sort *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    sortdata_setlist(&x->x_inbuf1, ac, av);
    sort_doit(x, 0);
}

static void sortdata_set_any(t_sortdata *d, t_symbol *s, int ac, t_atom *av){
    if(s && s != &s_){
        int nrequested = ac + 1;
        if(nrequested > d->d_max) {
            ac = d->d_max - 1;
            if (ac < 0) ac = 0;
        }
        if(d->d_max >= 1){
            SETSYMBOL(d->d_buf, s);
            if(ac > 0)
                memcpy(d->d_buf + 1, av, ac * sizeof(*d->d_buf));
            d->d_natoms = ac + 1;
        }
    }
    else
        sortdata_setlist(d, ac, av);
}

static void sort_anything(t_sort *x, t_symbol *s, int ac, t_atom *av){
    sortdata_set_any(&x->x_inbuf1, s, ac, av);
    sort_doit(x, 0);
}

// FREE/NEW/SETUP **********************************
static void sortdata_free(t_sortdata *d){
    if(d->d_buf != d->d_bufini)
        freebytes(d->d_buf, d->d_size * sizeof(*d->d_buf));
}

static void sort_free(t_sort *x){
    sortdata_free(&x->x_inbuf1);
    sortdata_free(&x->x_outbuf1);
    sortdata_free(&x->x_outbuf2);
}

static void sortdata_init(t_sortdata *d){
    d->d_size = MAXSIZE;
    d->d_natoms = 0;
    d->d_buf = d->d_bufini;
}

static void *sort_new(t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    ac = 0;
    t_sort *x = (t_sort *)pd_new(sort_class);
    int sz = MAXSIZE;
    x->x_inbuf1.d_max = sz;
    x->x_outbuf1.d_max = sz;
    x->x_outbuf2.d_max = sz;
    sortdata_init(&x->x_inbuf1);
    sortdata_init(&x->x_outbuf1);
    sortdata_init(&x->x_outbuf2);
    x->x_dir = av->a_w.w_float;
    floatinlet_new(&x->x_ob, &x->x_dir);
    outlet_new((t_object *)x, &s_anything);
    x->x_out2 = outlet_new((t_object *)x, &s_anything);
    return(x);
}

void sort_setup(void){
    sort_class = class_new(gensym("sort"), (t_newmethod)sort_new,
            (t_method)sort_free, sizeof(t_sort), 0, A_GIMME, 0);
    class_addbang(sort_class, sort_bang);
    class_addlist(sort_class, sort_list);
    class_addanything(sort_class, sort_anything);
}
