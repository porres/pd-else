
#include <string.h>
#include "buffer.h"
#include "g_canvas.h"

#define XTRASAMPS 4
#define SAMPBLK 4

extern int ugen_getsortno(void);

typedef struct delinctl{
    int         c_n;
    int         c_nchans;
    t_sample   *c_vec;
    int         c_phase;
}t_delinctl;

typedef struct _delin{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_float     x_deltime; // delay in msec
    t_delinctl  x_cspace;
    int         x_sortno;  // DSP sort number at which this was last put on chain
    int         x_rsortno; // DSP sort # for first delread or write in chain
    int         x_vecsize; // vector size for delout~ to use
    int         x_nchans;
    int         x_n;
    int         x_freeze;
    int         x_ms;
    t_float     x_sr;
}t_delin;

typedef struct _delout{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_float     x_sr;       // samples per msec
    int         x_zerodel;  // 0 or vecsize depending on read/write order
    int         x_ms;
    t_float     x_f;
}t_delout;

static void del_update(t_delin *x){
    int nsamps = x->x_deltime;
    if(x->x_ms)
        nsamps *= (x->x_sr * (t_float)(0.001f));
    if(nsamps < 1)
        nsamps = 1;
    nsamps += ((- nsamps) & (SAMPBLK - 1));
    nsamps += x->x_vecsize;
    if(x->x_cspace.c_n != nsamps || x->x_cspace.c_nchans != x->x_nchans){
        int oldsize = (x->x_cspace.c_n + XTRASAMPS) * x->x_cspace.c_nchans;
        int newsize = (nsamps + XTRASAMPS) * x->x_nchans;
        x->x_cspace.c_vec = (t_sample *)resizebytes(x->x_cspace.c_vec,
             oldsize * sizeof(t_sample), newsize * sizeof(t_sample));
        memset(x->x_cspace.c_vec, 0, newsize * sizeof(t_sample));
        x->x_cspace.c_n = nsamps;
        x->x_cspace.c_nchans = x->x_nchans;
        x->x_cspace.c_phase = XTRASAMPS;
    }
}

// check that all delins/outs have the same vecsize
static void del_check(t_delin *x, int vecsize, t_float sr){
    // the first object in the DSP chain sets the vecsize
    if(x->x_rsortno != ugen_getsortno()){
        x->x_vecsize = vecsize;
        x->x_sr = sr;
        x->x_rsortno = ugen_getsortno();
    }
    // Subsequent objects are only allowed to increase the vector size/samplerate
    else{
        if(vecsize > x->x_vecsize)
            x->x_vecsize = vecsize;
        if(sr > x->x_sr)
            x->x_sr = sr;
    }
}
