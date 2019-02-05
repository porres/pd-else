// 25

// This is a modified version of the code of creb/fdn~ by Alexandre Porres

// [fdn.rev~] is a Feedback Delay Network with a householder reflection feedback matrix
//    (In - 2/n 11T).

// each delay line is filtered with a 1st order filter:
//        H(z) = 2 gl gh / (gl + gh - z^-1 (gl - gh)) - where gl: dc gain & gh: ny gain
//        Difference equation: yk = (2 gl gh ) / (gl + gh) x + (gl - gh) / (gl + gh) yk-1

// TODO (from original code) - Add: delay time generation code / prime calculation for delay lengths
//       / more diffuse feedback matrix (hadamard) & check filtering code

/*   Copyright (c) 2000-2003 by Tom Schouten                                *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 2 of the License, or      *
 *   (at your option) any later version.                                    *
 *                                                                          *
 *   This program is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU General Public License for more details.                           *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program; if not, write to the Free Software            *
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.              */

#include "m_pd.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef union{
    unsigned int i;
    float f;
}t_flint;

#define IS_DENORMAL(f) (((((t_flint)(f)).i) & 0x7f800000) == 0)

typedef struct fdnctl{
    t_int    c_order;
    t_int    c_maxorder;
    t_float  c_leak;
    t_float  c_input;
    t_float  c_output;
    t_float *c_buf;
    t_float *c_gain_in;
    t_float *c_gain_state;
    t_float  c_t60_lo;
    t_float  c_t60_hi;
    t_int   *c_tap;         // cirular feed: N+1 pointers: 1 read, (N-1)r/w, 1 write
    t_float *c_time_ms;
    t_int    c_bufsize;
    t_float *c_vector[2];
    t_float *c_vectorbuffer;
    t_int    c_curvector;
}t_fdnctl;

typedef struct fdn{
    t_object x_obj;
    t_fdnctl x_ctl;
    t_int    x_exp;
}t_fdn;

t_class *fdn_class;

// Delay lines are filtered by a 1 pole filter (gl: DC gain, gh: Nyquist gain):
// The equation is: yn = (2*gl*gh ) / (gl+gh) x + (gl-gh) / (gl+gh) y[n-1]
// Source: https://ccrma.stanford.edu/~jos/pasp/First_Order_Delay_Filter_Design.html
static void fdn_setgain(t_fdn *x){
    t_float gl, gh;
    t_float t60_l = x->x_ctl.c_t60_lo * 1000;
    t_float t60_h = x->x_ctl.c_t60_hi * 1000;
    for(t_int i = 0; i < x->x_ctl.c_order; i++){
        gl = pow(10, -3.f * x->x_ctl.c_time_ms[i] / t60_l);
        gh = pow(10, -3.f * x->x_ctl.c_time_ms[i] / t60_h);
        x->x_ctl.c_gain_in[i] = 2.0f * gl * gh / (gl + gh);
        x->x_ctl.c_gain_state[i] = (gl - gh) / (gl + gh);
    }
}

static void fdn_decaylow(t_fdn *x, t_float f){
    x->x_ctl.c_t60_lo = f < .01f ? .01f : f;
    fdn_setgain(x);
}

static void fdn_decayhigh(t_fdn *x, t_float f){
    x->x_ctl.c_t60_hi = f < .01f ? .01f : f;
    fdn_setgain(x);
}

static void fdn_delsizes(t_fdn *x){
    t_int mask = x->x_ctl.c_bufsize - 1;
    t_int start = x->x_ctl.c_tap[0];
    t_int *tap = x->x_ctl.c_tap;
    tap[0] = (start & mask);
    float *length = x->x_ctl.c_time_ms;
    float scale = sys_getsr() * .001f;
    t_int sum = 0;
    for(t_int t = 1; t <= x->x_ctl.c_order; t++){
        sum += (t_int)(length[t-1] * scale); // delay time in samples
        tap[t] = (start+sum)&mask;
    }
    if(sum > mask)
        post("[fdn.rev~]: not enough delay memory (this could lead to instability)");
    fdn_setgain(x);
}

static void fdn_order(t_fdn *x, t_int order){
    x->x_ctl.c_order = order;
    x->x_ctl.c_leak = -2./ order;
    x->x_ctl.c_input = 1./ sqrt(order); // ???
}

static void fdn_set(t_fdn *x, t_float size, t_float min, t_float max){
    t_int order = ((int)size) & 0xfffffffc; // clip to powers of 2
    if(order < 4){
        post("[fdn.rev~]: order needs to be at least 4");
        return;
    }
    if(order > x->x_ctl.c_maxorder){
        post("[fdn.rev~]: order %d can't be larger than maxorder %d:", order, x->x_ctl.c_maxorder);
        return;
    }
    if(min <= 0){
        post("[fdn.rev~]: min can't be equal or less than 0");
        return;
    }
    if(max <= 0){
        post("[fdn.rev~]: max can't be equal or less than 0");
        return;
    }
    t_float inc;
    if(x->x_exp)
        inc = pow(max / min, 1.0f / ((float)(order - 1))); // EXPONENTIAL
    else
        inc = (max - min) / (float)(order - 1); // LINEAR
    t_float length = min;
    for(t_int i = 0; i < order; i++){
        x->x_ctl.c_time_ms[i] = length;
        if(x->x_exp)
            length *= inc;
        else
            length += inc;
    }
    fdn_order(x, order);
    fdn_delsizes(x);
}

static void fdn_exponential(t_fdn *x, t_float mode){
    x->x_exp = (t_int)(mode != 0);
}

static void fdn_list (t_fdn *x,  t_symbol *s, int argc, t_atom *argv){
    t_symbol *dummy = s;
    dummy = NULL;
    int order = argc & 0xfffffffc; // clip to powers of two
    if(order < 4){
        post("[fdn.rev~]: order needs to be at least 4");
        return;
    }
    if(order > x->x_ctl.c_maxorder)
        post("[fdn.rev~]: order clipped to %d:", order = x->x_ctl.c_maxorder);
    fdn_order(x, order);
    for(t_int i = 0; i < order; i++){
        if(argv[i].a_type == A_FLOAT)
            x->x_ctl.c_time_ms[i] = argv[i].a_w.w_float;
    }
    fdn_delsizes(x);
}

static void fdn_print(t_fdn *x){
    post("[fdn.rev~]: max delay lines %d, buffer size %d samples (%.2f ms)",
         x->x_ctl.c_maxorder, x->x_ctl.c_bufsize ,
         ((float)x->x_ctl.c_bufsize) * 1000 / sys_getsr());
    post("delay times:");
    for(t_int i = 0; i < x->x_ctl.c_order; i++)
        post("line %d: %.2f ms", i + 1, x->x_ctl.c_time_ms[i]);
}

static void fdn_clear(t_fdn *x){
    if(x->x_ctl.c_buf)
        memset(x->x_ctl.c_buf, 0, x->x_ctl.c_bufsize * sizeof(float));
    if(x->x_ctl.c_vectorbuffer)
        memset(x->x_ctl.c_vectorbuffer, 0, x->x_ctl.c_maxorder * 2 * sizeof(float));
}

static t_int *fdn_perform(t_int *w){
    t_fdnctl *ctl       = (t_fdnctl *)(w[1]);
    t_int n             = (t_int)(w[2]);
    t_float *in         = (float *)(w[3]);
    t_float *outl       = (float *)(w[4]);
    t_float *outr       = (float *)(w[5]);
    t_float *gain_in    = ctl->c_gain_in;
    t_float *gain_state = ctl->c_gain_state;
    t_int order         = ctl->c_order;
    t_int *tap          = ctl->c_tap;
    t_float *buf        = ctl->c_buf;
    t_int mask          = ctl->c_bufsize - 1;
    t_int i, j;
    t_float x, y, left, right, z;
    t_float *cvec, *lvec;
    t_float save;
    for(i = 0; i < n; i++){
        x = *in++;
        y = 0;
        left = 0;
        right = 0;
// get temporary vector buffers
        cvec = ctl->c_vector[ctl->c_curvector];
        lvec = ctl->c_vector[ctl->c_curvector ^ 1];
        ctl->c_curvector ^= 1;
// read input vector + get sum and left/right output
        for(j = 0; j < order;){
            z = buf[tap[j]];
            cvec[j] = z;
            y     += z;
            left  += z;
            right += z;
            j++;
            z = buf[tap[j]];
            cvec[j] = z;
            y     += z;
            left  -= z;
            right += z;
            j++;
            z = buf[tap[j]];
            cvec[j] = z;
            y     += z;
            left  += z;
            right -= z;
            j++;
            z = buf[tap[j]];
            cvec[j] = z;
            y     += z;
            left  -= z;
            right -= z;
            j++;
        }
        *outl++ = left;
        *outr++ = right;
        y *=  ctl->c_leak; // y == leak to all inputs
// perform feedback (todo: decouple feedback & permutation)
        save = cvec[0];
        for(j = 0; j < order-1; j++)
            cvec[j] = cvec[j+1] + y + x;
        cvec[order-1] = save + y + x;
// apply gain + store result vector in delay lines + increment taps
        tap[0] = (tap[0] + 1)&mask;
        for(j = 0; j < order; j++){
            save = gain_in[j] * cvec[j] + gain_state[j] * lvec[j];
            save = IS_DENORMAL(save) ? 0 : save;
            cvec[j] = save;
            buf[tap[j+1]] = save;
            tap[j+1] = (tap[j+1] + 1) & mask;
        }
    }
    return(w+6);
}

static void fdn_dsp(t_fdn *x, t_signal **sp){
  dsp_add(fdn_perform, 5, &x->x_ctl, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void fdn_free(t_fdn *x){
    if(x->x_ctl.c_tap)
        free( x->x_ctl.c_tap);
    if(x->x_ctl.c_time_ms)
        free( x->x_ctl.c_time_ms);
    if(x->x_ctl.c_gain_in)
        free( x->x_ctl.c_gain_in);
    if(x->x_ctl.c_gain_state)
        free( x->x_ctl.c_gain_state);
    if(x->x_ctl.c_buf)
        free (x->x_ctl.c_buf);
    if(x->x_ctl.c_vectorbuffer)
        free (x->x_ctl.c_vectorbuffer);
}

static void *fdn_new(t_floatarg size){
    t_fdn *x = (t_fdn *)pd_new(fdn_class);
    t_int order = 1024; // maximum order is 1024
/*    if(order < 4)
        order = 4; */
    if(size < 16) // size in samples
        size = 16;
    if(size > 30) // size in samples
        size = 30;
    t_int bufsize = pow(2, (t_int)size);
    
    t_int bufround = 1;
    while(bufround < bufsize)
        bufround *= 2;
    bufsize = bufround;

    t_float lo = 10;
    t_float hi = 10;
    
    x->x_exp = 0;

    x->x_ctl.c_maxorder = order;
    x->x_ctl.c_buf = (float *)malloc(sizeof(float) * bufsize);
    x->x_ctl.c_bufsize = bufsize;
    x->x_ctl.c_tap = (t_int *)malloc((order + 1) * sizeof(t_int));
    x->x_ctl.c_time_ms = (t_float *)malloc(order * sizeof(t_int));
    x->x_ctl.c_gain_in = (t_float *)malloc(order * sizeof(t_float));
    x->x_ctl.c_gain_state = (t_float *)malloc(order * sizeof(t_float));
    x->x_ctl.c_vectorbuffer = (t_float *)malloc(order * 2 * sizeof(float));
    memset(x->x_ctl.c_vectorbuffer, 0, order * 2 * sizeof(float));
    x->x_ctl.c_curvector = 0;
    x->x_ctl.c_vector[0] = &x->x_ctl.c_vectorbuffer[0];
    x->x_ctl.c_vector[1] = &x->x_ctl.c_vectorbuffer[order];
    x->x_ctl.c_t60_lo = lo < .01f ? .01f : lo;
    x->x_ctl.c_t60_hi = hi < .01f ? .01f : hi;
// default preset
    t_atom at[4];
    SETFLOAT(at, 0.f);
    SETFLOAT(at+1, 0.f);
    SETFLOAT(at+2, 0.f);
    SETFLOAT(at+3, 0.f);
    fdn_list(x, &s_list, 4, at);
// end of preset
    
    fdn_clear(x); // clear delay memory to zero
    
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("lo"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("float"), gensym("hi"));
    outlet_new(&x->x_obj, gensym("signal"));
    outlet_new(&x->x_obj, gensym("signal"));
    return(void *)x;
}

void setup_fdn0x2erev_tilde(void){
    fdn_class = class_new(gensym("fdn.rev~"), (t_newmethod)fdn_new,
    	(t_method)fdn_free, sizeof(t_fdn), 0, A_DEFFLOAT, 0);
    class_addmethod(fdn_class, nullfn, gensym("signal"), 0);
    class_addmethod(fdn_class, (t_method)fdn_dsp, gensym("dsp"), 0);
    class_addlist(fdn_class, (t_method)fdn_list);
    class_addmethod(fdn_class, (t_method)fdn_decaylow, gensym("lo"), A_DEFFLOAT, 0);
    class_addmethod(fdn_class, (t_method)fdn_decayhigh, gensym("hi"), A_DEFFLOAT, 0);
    class_addmethod(fdn_class, (t_method)fdn_set, gensym("set"),
                    A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(fdn_class, (t_method)fdn_exponential, gensym("exp"), A_DEFFLOAT, 0);
    class_addmethod(fdn_class, (t_method)fdn_clear, gensym("clear"), 0);
    class_addmethod(fdn_class, (t_method)fdn_print, gensym("print"), 0);
}
