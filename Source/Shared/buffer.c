
#include <m_pd.h>
#include "buffer.h"
#include <string.h>
#include <stdarg.h>

////////////////////////////////   INTERPOLATION

double interp_lin(double frac, double b, double c){
    return(b + frac * (c-b));
}

double interp_cos(double frac, double b, double c){
    frac = (cos(frac * -M_PI)) * 0.5 + 0.5;
    return(c + frac * (b-c));
}

double interp_pow(double frac, double b, double c, double p){
    double dif = c-b;
    if(fabs(p) == 1) // linear
        return(b + frac * dif);
    else{
        if(p >= 0){ // positive exponential
            if(b < c) // ascending
                return(b + pow(frac, p) * dif);
            else // descending (invert)
                return(b + (1-pow(1-frac, p)) * dif);
        }
        else{ // negative exponential
            if(b < c) // ascending
                return(b + (1-pow(1-frac, -p)) * dif);
            else // descending (invert)
                return(b + pow(frac, -p) * dif);
        }
    }
}

double interp_lagrange(double frac, double a, double b, double c, double d){
    double cminusb = c-b;
    return((t_float)(b + frac * (cminusb - (1. - frac)*ONE_SIXTH *
        ((d - a - 3.0*cminusb) * frac + d + 2.0*a - 3.0*b))));
}

double interp_cubic(double frac, double a, double b, double c, double d){
    double p0 = d - a + b - c;
    double p1 = a - b - p0;
    double p2 = c - a;
    return(b + frac*(p2 + frac*(p1 + frac*p0)));
}

double interp_spline(double frac, double a, double b, double c, double d){
    double p0 = 0.5*(d - a) + 1.5*(b - c);
    double p2 = 0.5*(c - a);
    double p1 = a - b + p2 - p0;
    return(b + frac*(p2 + frac * (p1 + frac*p0)));
}

double interp_hermite(double frac, double a, double b, double c, double d,
double bias, double tension){
    double frac2 = frac*frac;
    double frac3 = frac*frac2;
    double cminusb = c - b;
    double bias1 = 1. - bias;
    bias++;
    double m0 = tension * ((b-a) * bias + cminusb * bias1);
    double m1 = tension * (cminusb*bias + (d-c) * bias1);
    double p2 = frac3 - frac2;
    double p0 = 2*p2 - frac2 + 1.;
    double p1 = p2 - frac2 + frac;
    double p3 = frac2 - 2*p2;
    return(p0*b + p1*m0 + p2*m1 + p3*c);
}

////////////////////////////////   INIT TABLES!!!! They stays allocated as long as Pd is running

static double *sintable, *partable;
static int fadetables = 0;

static double *tab_fade_sin;
static double *tab_fade_hannsin;
static double *tab_fade_hann;
static double *tab_fade_linsin;
static double *tab_fade_quartic;
static double *tab_fade_sqrt;

void init_sine_table(void){
    if(sintable)
        return;
    sintable = getbytes((ELSE_SIN_TABSIZE + 1) * sizeof(*sintable));
    double *tp = sintable;
    double inc = TWO_PI / ELSE_SIN_TABSIZE, phase = 0;
    for(int i = ELSE_SIN_TABSIZE/4 - 1; i >= 0; i--, phase += inc)
        *tp++ = sin(phase); // populate 1st quarter
    *tp++ = 1;
    for(int i = ELSE_SIN_TABSIZE/4 - 1; i >= 0; i--)
        *tp++ = sintable[i]; // mirror inverted
    for(int i = ELSE_SIN_TABSIZE/2 - 1; i >= 0; i--)
        *tp++ = -sintable[i]; // mirror back
}

void init_parabolic_table(void){
    if(partable)
        return;
    partable = getbytes((ELSE_SIN_TABSIZE + 1) * sizeof(*partable));
    double *tp = partable;
    double inc = 1.0f / ELSE_SIN_TABSIZE, phase = 0;
    for(int i = 0; i < ELSE_SIN_TABSIZE; i++, phase += inc)
        *tp++ = (1 - pow(fmod(phase * 2, 1) * 2 - 1, 2)) * (phase <= 0.5 ? 1 : -1);
}

void init_fade_tables(void){
    if(fadetables)
        return;
    fadetables = 1;
    int i;
    double *tp;
    double inc1, inc2, v, ph1, ph2;
    
    inc1 = 1.0 / ELSE_FADE_TABSIZE;
    inc2 = HALF_PI * inc1;
    
    tab_fade_quartic = getbytes((ELSE_FADE_TABSIZE + 1) * sizeof(*tab_fade_quartic));
    tp = tab_fade_quartic;
    for(i = 0, ph1 = 0; i <= ELSE_FADE_TABSIZE; i++, ph1 += inc1)
        *tp++ = pow(ph1, 4);

    tab_fade_sqrt = getbytes((ELSE_FADE_TABSIZE + 1) * sizeof(*tab_fade_sqrt));
    tp = tab_fade_sqrt;
    for(i = 0, ph1 = 0; i <= ELSE_FADE_TABSIZE; i++, ph1 += inc1)
        *tp++ = sqrt(ph1);
    
    tab_fade_sin = getbytes((ELSE_FADE_TABSIZE + 1) * sizeof(*tab_fade_sin));
    tp = tab_fade_sin;
    for(i = 0, ph1 = 0; i <= ELSE_FADE_TABSIZE; i++, ph1 += inc2)
        *tp++ = sin(ph1);
    
    tab_fade_hann = getbytes((ELSE_FADE_TABSIZE + 1) * sizeof(*tab_fade_hann));
    tp = tab_fade_hann;
    for(i = 0, ph1 = 0; i <= ELSE_FADE_TABSIZE; i++, ph1 += inc2){
        v = sin(ph1);
        *tp++ = v*v;
    }
    
    tab_fade_hannsin = getbytes((ELSE_FADE_TABSIZE + 1) * sizeof(*tab_fade_hannsin));
    tp = tab_fade_hannsin;
    for(i = 0, ph1 = 0; i <= ELSE_FADE_TABSIZE; i++, ph1 += inc2){
        v = sin(ph1);
        *tp++ = v*sqrt(v);
    }
    
    tab_fade_linsin = getbytes((ELSE_FADE_TABSIZE + 1) * sizeof(*tab_fade_linsin));
    tp = tab_fade_linsin;
    for(i = 0, ph1 = 0, ph2 = 0; i <= ELSE_FADE_TABSIZE; i++, ph1 += inc1, ph2 += inc2)
        *tp++ = sqrt(ph1 * sin(ph2));
}

double read_fadetab(double phase, int tab){
    double tabphase = phase * ELSE_FADE_TABSIZE;
    int i = (int)tabphase;
    int i_next = i == ELSE_FADE_TABSIZE ? i : i + 1;
    double frac = tabphase - i, p1 = 0, p2 = 0;
    if(tab == 0)
        p1 = tab_fade_quartic[i], p2 = tab_fade_quartic[i_next];
    else if(tab == 1)
        return(phase);
    else if(tab == 2)
        p1 = tab_fade_linsin[i], p2 = tab_fade_linsin[i_next];
    else if(tab == 3)
        p1 = tab_fade_sqrt[i], p2 = tab_fade_sqrt[i_next];
    else if(tab == 4)
        p1 = tab_fade_sin[i], p2 = tab_fade_sin[i_next];
    else if(tab == 5)
        p1 = tab_fade_hannsin[i], p2 = tab_fade_hannsin[i_next];
    else if(tab == 6)
        p1 = tab_fade_hann[i], p2 = tab_fade_hann[i_next];
    return(interp_lin(frac, p1, p2));
}

double read_sintab(double phase){
    double tabphase = phase * ELSE_SIN_TABSIZE;
    int i = (int)tabphase;
    double frac = tabphase - i, p1 = sintable[i], p2 = sintable[i+1];
    return(interp_lin(frac, p1, p2));
}

double read_partab(double phase){
    double tabphase = phase * ELSE_SIN_TABSIZE;
    int i = (int)tabphase;
    double frac = tabphase - i, p1 = partable[i], p2 = partable[i+1];
    return(interp_lin(frac, p1, p2));
}


// on failure *bufsize is not modified
t_word *buffer_get(t_buffer *c, t_symbol * name, int *bufsize, int indsp, int complain){
//in dsp = used in dsp,
    if(name && name != &s_){
        t_garray *ap = (t_garray *)pd_findbyclass(name, garray_class);
        if(ap){
            int bufsz;
            t_word *vec;
            if(garray_getfloatwords(ap, &bufsz, &vec)){
                if(indsp)
                    garray_usedindsp(ap);
                if(bufsize)
                    *bufsize = bufsz;
                return(vec);
            }
            else // always complain
                pd_error(c->c_owner, "bad template of array '%s'", name->s_name);
        }
        else if(complain)
            pd_error(c->c_owner, "no such array '%s'", name->s_name);
    }
    return(0);
}

//making peek~ work with channel number choosing, assuming 1-indexed
void buffer_getchannel(t_buffer *c, int chan_num, int complain){
    int chan_idx;
    char buf[MAXPDSTRING];
    t_symbol * curname; //name of the current channel we want
    int vsz = c->c_npts;  
    t_word *retvec = NULL;//pointer to the corresponding channel to return
    //1-indexed bounds checking
    chan_num = chan_num < 1 ? 1 : (chan_num > buffer_MAXCHANS ? buffer_MAXCHANS : chan_num);
    c->c_single = chan_num;
    //convert to 0-indexing, separate steps and diff variable for sanity's sake
    chan_idx = chan_num - 1;
    //making the buffer channel name string we'll be looking for
    if(c->c_bufname != &s_){
        if(chan_idx == 0){
            //if channel idx is 0, check for just plain bufname as well
            //since checking for 0-bufname as well, don't complain here
            retvec = buffer_get(c, c->c_bufname, &vsz, 1, 0);
            if(retvec){
                c->c_vectors[0] = retvec;
                if (vsz < c->c_npts) c->c_npts = vsz;
                return;
            };
        };
        // Now setting buffername according to bufnamemode: 0 - <ch>-<bufname> [default/legacy], 1 - <bufname>-<ch> 
        if(c->c_bufnamemode == 0){
            sprintf(buf, "%d-%s", chan_idx, c->c_bufname->s_name);
        }
        else if(c->c_bufnamemode == 1){
            sprintf(buf, "%s-%d", c->c_bufname->s_name, chan_idx);
        };
        curname =  gensym(buf);
        retvec = buffer_get(c, curname, &vsz, 1, complain);
        //if channel found and less than c_npts, reset c_npts
        if (vsz < c->c_npts) c->c_npts = vsz;
        c->c_vectors[0] = retvec;
        return;
    };

}

void buffer_bug(char *fmt, ...){ // from loud.c
    char buf[MAXPDSTRING];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
    va_end(ap);
    fprintf(stderr, "miXed consistency check failed: %s\n", buf);
#ifdef _WIN32
    fflush(stderr);
#endif
    bug("%s", buf);
}

void buffer_clear(t_buffer *c){
    c->c_npts = 0;
    memset(c->c_vectors, 0, c->c_numchans * sizeof(*c->c_vectors));
}

void buffer_redraw(t_buffer *c){
    if(!c->c_single){
        if(c->c_numchans <= 1 && c->c_bufname != &s_){
            t_garray *ap = (t_garray *)pd_findbyclass(c->c_bufname, garray_class);
            if (ap) garray_redraw(ap);
            else if (c->c_vectors[0]) buffer_bug("buffer_redraw 1");
        }
        else if (c->c_numchans > 1){
            int ch = c->c_numchans;
            while (ch--){
                t_garray *ap = (t_garray *)pd_findbyclass(c->c_channames[ch], garray_class);
                if (ap) garray_redraw(ap);
                else if (c->c_vectors[ch]) buffer_bug("buffer_redraw 2");
            }
        };
    }
    else{
        int chan_idx;
        char buf[MAXPDSTRING];
        t_symbol * curname; //name of the current channel we want
        int chan_num = c->c_single; //1-indexed channel number
        chan_num = chan_num < 1 ? 1 : (chan_num > buffer_MAXCHANS ? buffer_MAXCHANS : chan_num);
         //convert to 0-indexing, separate steps and diff variable for sanity's sake
        chan_idx = chan_num - 1;
        //making the buffer channel name string we'll be looking for
        if(c->c_bufname != &s_){
            if(chan_idx == 0){
                //if channel idx is 0, check for just plain bufname as well
                t_garray *ap = (t_garray *)pd_findbyclass(c->c_bufname, garray_class);
                if (ap){
                    garray_redraw(ap);
                    return;
                };
            };
            // Now setting buffername according to bufnamemode: 0 - <ch>-<bufname> [default/legacy], 1 - <bufname>-<ch>
            if(c->c_bufnamemode == 0){
                sprintf(buf, "%d-%s", chan_idx, c->c_bufname->s_name);
            }
            else if(c->c_bufnamemode == 1){
                sprintf(buf, "%s-%d", c->c_bufname->s_name, chan_idx);
            };
            curname =  gensym(buf);
            t_garray *ap = (t_garray *)pd_findbyclass(curname, garray_class);
            if (ap)
                garray_redraw(ap);
            // not really sure what the specific message is for, just copied single channel one - DK
            else if (c->c_vectors[0])
                buffer_bug("buffer_redraw 1");

        };
    };
}

void buffer_validate(t_buffer *c, int complain){
    buffer_clear(c);
    c->c_npts = SHARED_INT_MAX;
    if(!c->c_single){
        if (c->c_numchans <= 1 && c->c_bufname != &s_){
            c->c_vectors[0] = buffer_get(c, c->c_bufname, &c->c_npts, 1, 0);
            if(!c->c_vectors[0]){ // check for 0-bufname if bufname array isn't found
                c->c_vectors[0] = buffer_get(c, c->c_channames[0], &c->c_npts, 1, 0);
                //if neither found, post about it if complain
                if(!c->c_vectors[0] && complain)
                    pd_error(c->c_owner, "no such array '%s' (or '0-%s')",
                             c->c_bufname->s_name, c->c_bufname->s_name);
            };
        }
        else if(c->c_numchans > 1){
            int ch;
            for (ch = 0; ch < c->c_numchans ; ch++){
                int vsz = c->c_npts;  /* ignore missing arrays */
                // only complain if can't find first channel (ch = 0)
                c->c_vectors[ch] = buffer_get(c, c->c_channames[ch], &vsz, 1, !ch && complain);
                if(vsz < c->c_npts)
                    c->c_npts = vsz;
            };
        };
    }
    else
        buffer_getchannel(c, c->c_single, complain);
    if(c->c_npts == SHARED_INT_MAX)
        c->c_npts = 0;
}

void buffer_playcheck(t_buffer *c){
    c->c_playable = (!c->c_disabled && c->c_npts >= c->c_minsize);
}

void buffer_initarray(t_buffer *c, t_symbol *name, int complain){
    if(name){ // setting array names
        c->c_bufname = name;
        if(c->c_numchans >= 1){
            char buf[MAXPDSTRING];
            int ch;
            for(ch = 0; ch < c->c_numchans; ch++){
                // Now setting buffername according to bufnamemode: 0 - <ch>-<bufname> [default/legacy], 1 - <bufname>-<ch>
                if(c->c_bufnamemode == 0){
                    sprintf(buf, "%d-%s", ch, c->c_bufname->s_name);
                }
                else if(c->c_bufnamemode == 1){
                    sprintf(buf, "%s-%d", c->c_bufname->s_name, ch);
                };
                c->c_channames[ch] = gensym(buf);
            };
        };
        buffer_validate(c, complain);
    };
    buffer_playcheck(c);
}

//wrapper around buffer_initarray so you don't have to pass the complain flag each time
void buffer_setarray(t_buffer *c, t_symbol *name){
   buffer_initarray(c, name, 1); 
}

void buffer_setminsize(t_buffer *c, int i){
    c->c_minsize = i;
}

void buffer_checkdsp(t_buffer *c){
    buffer_validate(c, 1);
    buffer_playcheck(c);

}

void buffer_free(t_buffer *c){
    if (c->c_vectors)
        freebytes(c->c_vectors, c->c_numchans * sizeof(*c->c_vectors));
    if (c->c_channames)
        freebytes(c->c_channames, c->c_numchans * sizeof(*c->c_channames));
    freebytes(c, sizeof(t_buffer));
}

/* If nauxsigs is positive, then the number of signals is nchannels + nauxsigs;
   otherwise the channels are not used as signals, and the number of signals is
   nsigs -- provided that nsigs is positive -- or, if it is not, then an buffer
   is not used in dsp (peek~). */

// Added the fifth argument to set namemode: 0 = <ch>-<arrayname> [default/legacy], 1 = <arrayname>-<ch>
void *buffer_init(t_class *owner, t_symbol *bufname, int numchans, int singlemode, int bufnamemode){
// name of buffer (multichan usu, or not) and the number of channels associated with buffer 
    t_buffer *c = (t_buffer *)getbytes(sizeof(t_buffer));
    t_word **vectors;
    t_symbol **channames = 0;
    if(!bufname)
        bufname = &s_;
    c->c_bufname = bufname;
    singlemode = singlemode > 0 ? 1 : 0; // single mode forces numchans = 1
    // default namemode is 0 <ch>-<arrayname>
    if(!bufnamemode)
        bufnamemode = 0; // default: <ch>-<arrayname>
    // setting namemode
    c->c_bufnamemode = bufnamemode;
    numchans = (numchans < 1 || singlemode) ? 1 : (numchans > buffer_MAXCHANS ? buffer_MAXCHANS : numchans);
    if(!(vectors = (t_word **)getbytes(numchans* sizeof(*vectors))))
		return(0);
	if(!(channames = (t_symbol **)getbytes(numchans * sizeof(*channames)))){
		freebytes(vectors, numchans * sizeof(*vectors));
        return(0);
    };
    c->c_single = singlemode;
    c->c_owner = owner;
    c->c_npts = 0;
    c->c_vectors = vectors;
    c->c_channames = channames;
    c->c_disabled = 0;
    c->c_playable = 0;
    c->c_minsize = 1;
    c->c_numchans = numchans;
    if(bufname != &s_)
        buffer_initarray(c, bufname, 0);
    return(c);
}

void buffer_enable(t_buffer *c, t_floatarg f){
    c->c_disabled = (f == 0);
    buffer_playcheck(c);
}
