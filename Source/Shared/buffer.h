#ifndef __buffer_H__
#define __buffer_H__

#include <limits.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(_MSC_VER)
#define isfinite _finite
#define isnan    _isnan
#endif

#define SHARED_INT_MAX INT_MAX

#define buffer_MAXCHANS 64

#define TWO_PI  (M_PI * 2.0)
#define HALF_PI (M_PI * 0.5)
#define ONE_SIXTH 0.16666666666666666666667f
#define ELSE_GEN_TABSIZE  16384
#define ELSE_FADE_TABSIZE 4096
#define GAUSS_WIDTH 3.0
#define buffer_MAXCHANS 64 //max number of channels

typedef struct _buffer{
    void       *c_owner;     // owner of buffer, note i don't know if this actually works
    int         c_npts;      // used also as a validation flag, number of samples in an array */
    int         c_numchans;
    t_word    **c_vectors;
    t_symbol  **c_channames;
    t_symbol   *c_bufname;
    int         c_playable;
    int         c_minsize;
    int         c_disabled;
    int         c_single;    // flag: 0-regular mode, 1-load this particular channel (1-idx)
                             // should be used with c_numchans == 1
}t_buffer;

double interp_lin(double frac, double b, double c);
double interp_cos(double frac, double b, double c);
double interp_pow(double frac, double b, double c, double p);
double interp_lagrange(double frac, double a, double b, double c, double d);
double interp_cubic(double frac, double a, double b, double c, double d);
double interp_spline(double frac, double a, double b, double c, double d);
double interp_hermite(double frac, double a, double b, double c, double d,
    double bias, double tension);

double read_sintab(double phase);
double read_costab(double phase);
double read_partab(double phase);
double read_sinsqrtab(double phase);
double read_gausstab(double phase);
double read_fadetab(double phase, int tab);
double read_pantab(double phase);

void init_sinsqr_table(void);
void init_sine_table(void);
void init_cosine_table(void);
void init_parabolic_table(void);
void init_gauss_table(void);
void init_fade_tables(void);

void buffer_bug(char *fmt, ...);
void buffer_clear(t_buffer *c);
void buffer_redraw(t_buffer *c);
void buffer_playcheck(t_buffer *c);

// use this function during buffer_init
// passing 0 to complain suppresses warnings
void buffer_initarray(t_buffer *c, t_symbol *name, int complain);
void buffer_validate(t_buffer *c, int complain);
// called by buffer_validate
t_word *buffer_get(t_buffer *c, t_symbol * name, int *bufsize, int indsp, int complain);

// wrap around initarray, but allow warnings (pass 1 to complain)
void buffer_setarray(t_buffer *c, t_symbol *name);

void buffer_setminsize(t_buffer *c, int i);
void buffer_enable(t_buffer *c, t_floatarg f);

// single channel mode used for poke~/peek~
void *buffer_init(t_class *owner, t_symbol *bufname, int numchans, int singlemode);
void buffer_free(t_buffer *c);
void buffer_checkdsp(t_buffer *c);
void buffer_getchannel(t_buffer *c, int chan_num, int complain);

#endif
