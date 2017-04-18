

#include "m_pd.h"
#include <math.h>
#include <string.h>

#define HALF_PI M_PI * 0.5

static t_class *select_class;

#define INPUTLIMIT 10
#define LINEAR 0
#define EPOWER 1
#define EPMIN 0 // ???
#define TIMEUNITPERSEC (32.*441000.) // ?????????????????????????????

typedef struct _ip // keeps track of each signal input
{
  int active[INPUTLIMIT]; 
  int counter[INPUTLIMIT];
  double timeoff[INPUTLIMIT];
  float fade[INPUTLIMIT];
  float *in[INPUTLIMIT];
} t_ip;

typedef struct _select
{
  t_object x_obj;
  int channel;
  int lastchannel;
  int actuallastchannel;
  int ninlets;
  int fadetime;
  double changetime;
  int fadecount;
  int fadeticks;
  int firsttick;
  int fadetype;
  int lastfadetype;
  int fadealert; 
  float srate;
  t_ip ip;
} t_select;

static void adjustcounters2epower(t_select *x);
static void adjustcounters2linear(t_select *x);

static void updatefades(t_select *x)
{
  int i;
  for(i = 0; i < x->ninlets; i++)
    {
      if(!x->ip.counter[i])
	x->ip.fade[i] = 0;
      if(x->ip.active[i] && x->ip.counter[i] < x->fadeticks)
	{
	  if(x->ip.counter[i])
	    x->ip.fade[i] = x->ip.counter[i] / (float)x->fadeticks;
	  x->ip.counter[i]++;
	}
      else if (!x->ip.active[i] && x->ip.counter[i] > 0)
	{
	  x->ip.fade[i] = x->ip.counter[i] / (float)x->fadeticks;
	  x->ip.counter[i]--;
	}
    }
}

static double epower(double rate)
{
  double tmp;
  if(rate < 0)
    rate = 0;
  if(rate > 0.999)
    rate = 0.999;
 rate *= HALF_PI;
 tmp = cos(rate - HALF_PI);
  tmp = tmp < 0 ? 0 : tmp;
  tmp = tmp > 1 ? 1 : tmp;
  return tmp;
}

static double aepower(double ep) // convert from equal power to linear rate
{
/*    double answer = (atan(2*ep*ep - 1) + 0.785398) / 1.5866; */
  double answer = (acos(ep) + HALF_PI) / HALF_PI;

  answer = 2 - answer;   // ??? - but does the trick

  answer = answer < 0 ? 0 : answer;
  answer = answer > 1 ? 1 : answer;
  return answer;
}

static void adjustcounters2epower(t_select *x) // no longer used
{
  // called when shifting from a linear fade-in (from zero) to an equal power crossfade
  // adjusts each input counter to smoothly match subsequent equal power scalings
  int i;
  double ep;
  for(i = 0; i < x->ninlets; i++)
    {
      ep = x->ip.counter[i] / (double)x->fadeticks;
      x->ip.counter[i] = aepower(ep) * (double)x->fadeticks;
    }
}

static void adjustcounters2linear(t_select *x) // no longer used
{
  // opposite of above
  int i;
  double rate;
  for(i = 0; i < x->ninlets; i++)
    {
      rate = x->ip.counter[i] / (double)x->fadeticks;
      x->ip.counter[i] = epower(rate) * (double)x->fadeticks;
    }
}

// JUNTAR OS 2 ABAIXO EM UM APENAS!!!!!!
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void outputfades(t_int *w, int flag)
{
  t_select *x = (t_select *)(w[1]);
  float *out = (t_float *)(w[3+x->ninlets]);
  int n = (int)(w[2]);
  int i;
  for(i = 0; i < x->ninlets; i++)
    x->ip.in[i] = (t_float *)(w[3+i]);
  while (n--)
    {
      float sum = 0;
      updatefades(x);
      for(i = 0; i < x->ninlets; i++)
	if(x->ip.fade[i])
	  {
	    if(flag && x->fadetype == EPOWER)
	      sum += *x->ip.in[i]++ * epower(x->ip.fade[i]);
	    else
	      sum += *x->ip.in[i]++ * x->ip.fade[i];
	  }
      *out++ = sum;
    }
}

static t_int *select_perform(t_int *w)
{
  t_select *x = (t_select *)(w[1]);
  int n = (int)(w[2]);
  float *out = (t_float *)(w[3+x->ninlets]);
  if (x->actuallastchannel == 0 && x->channel == 0 && x->lastchannel == 0) { // init state
      if(x->firsttick) // stupid
          x->firsttick = 0;
      while (n--)
      *out++ = 0;
      }
  else if (x->actuallastchannel == 0 && x->channel != 0) outputfades(w, x->fadetype); // change from 0 to non-0
  else if(x->channel != 0) outputfades(w, EPOWER); // change from non-0 to another non-0 // IS IT ALWAYS EQ POWER???????????
  else if (x->actuallastchannel != 0 && x->channel == 0) outputfades(w, x->fadetype); // change from non-0 to 0
// check which input feeds ought to be "switch~"ed off (????)
    int i;
    for(i = 0; i < x->ninlets; i++)
    {
        if(!x->ip.active[i])
            if(clock_gettimesince(x->ip.timeoff[i]) > x->fadetime
               && x->ip.timeoff[i])
            {
                x->ip.timeoff[i] = 0;
                x->ip.fade[i] = 0;
            }
    }
  return (w + 4 + x->ninlets);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void select_dsp(t_select *x, t_signal **sp)
{
  int n = sp[0]->s_n, i; // there must be a smarter way....
  switch (x->ninlets) 
    {
    case 1: dsp_add(select_perform, 4, x, n, sp[0]->s_vec, sp[1]->s_vec);
      break;
    case 2: dsp_add(select_perform, 5, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
      break;
    case 3: dsp_add(select_perform, 6, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
      break;
    case 4: dsp_add(select_perform, 7, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
      break;
    case 5: dsp_add(select_perform, 8, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
      break;
    case 6: dsp_add(select_perform, 9, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec);
      break;
    case 7: dsp_add(select_perform, 10, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec);
      break;
    case 8: dsp_add(select_perform, 11, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, 
		    sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec);
    break;
    case 9: dsp_add(select_perform, 12, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, 
		    sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec, sp[9]->s_vec);
    break;
    case 10: dsp_add(select_perform, 13, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, 
		     sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec, sp[9]->s_vec, sp[10]->s_vec);
    break;
    }
}

void select_float(t_select *x, t_floatarg f) // select channel
{
    f = (int)f;
    f = f > x->ninlets ? x->ninlets : f;
    f = f < 0 ? 0 : f;
    if(f != x->lastchannel)
    {
        if(f == x->actuallastchannel)
            x->fadecount = x->fadeticks - x->fadecount;
        else
            x->fadecount = 0;
        x->channel = f;
        if(x->channel)
        {
            x->ip.active[x->channel - 1] = 1;
        }
        if(x->lastchannel)
        {
            x->ip.active[x->lastchannel - 1] = 0;
            x->ip.timeoff[x->lastchannel - 1] = clock_getlogicaltime();
        }
        x->actuallastchannel = x->lastchannel;
        x->lastchannel = x->channel;
    }
}

static void select_time(t_select *x, t_floatarg time) // time
{
    int newticks, i, shorter;
    time = time < 1 ? 1 : time;
    shorter = (time < x->fadetime);
    x->fadetime = (int)time;
    x->fadeticks = (int)(x->srate * time/1000); // no. of ticks to reach specified fade time
    for(i = 0; i < x->ninlets; i++) // shortcheck
    {
        if(shorter && x->ip.timeoff[i]) // correct active timeoffs for new x->fadeticks (newticks)
            x->ip.timeoff[i] = clock_getlogicaltime() - ((x->fadeticks - x->ip.counter[i]) / (x->srate/1000.) - 1) * (TIMEUNITPERSEC/1000.);
    }
    for(i = 0; i < x->ninlets; i++) // adjustcounters
    {
        if(x->ip.counter[i])
            x->ip.counter[i] = x->ip.fade[i] * (float)x->fadeticks;
    }
}

void select_mode(t_select *x, t_floatarg mode) // mode
{
    int i;
    if(mode == 1 && x->lastfadetype != 1) // change to equal power
    {
        for(i = 0; i < x->ninlets; i++)
        {
            float fade = x->ip.fade[i];
            int oldcounter = x->ip.counter[i];
            x->ip.counter[i] = aepower(fade) * x->fadeticks;
            x->ip.fade[i] = epower(x->ip.counter[i]/ (float)x->fadeticks); // ???
        }
        x->lastfadetype = x->fadetype = EPOWER;
    }
    else if(mode == 0 && x->lastfadetype != 0) // change to linear
    {
        for(i = 0; i < x->ninlets; i++)
        {
            float fade = x->ip.fade[i];
            int oldcounter = x->ip.counter[i];
            x->ip.counter[i] = epower(fade) * x->fadeticks;
            x->ip.fade[i] = x->ip.counter[i]/ (float)x->fadeticks; // ???
        }
        x->lastfadetype = x->fadetype = LINEAR;
    }
}

static void *select_new(t_symbol *s, int argc, t_atom *argv)
{
    t_select *x = (t_select *)pd_new(select_class);
    x->srate = sys_getsr();
    t_float ch = 1, ms = 0, mode = 1;
    int i;
    int argnum = 0;
    while(argc > 0)
    {
        if(argv -> a_type == A_FLOAT)
        { //if current argument is a float
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum)
            {
                case 0:
                    ch = argval;
                    break;
                case 1:
                    ms = argval;
                    break;
                case 2:
                    mode = (argval != 0);
                    break;
                default:
                    break;
            };
            argnum++;
            argc--;
            argv++;
        }
        else {
            goto errstate;
        };
    };
    x->ninlets = ch < 1 ? 1 : ch;
    if(x->ninlets > INPUTLIMIT)
        {
        x->ninlets = INPUTLIMIT;
        post("select~: maximum of %d inlets", INPUTLIMIT);
        }
    x->fadetime = ms > 0 ? ms : 1; // what happens with ms = 0???
    for(i = 0; i < x->ninlets - 1; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, gensym("signal"));
    x->channel = 0; x->lastchannel = x->actuallastchannel = 0;
    x->fadecount = 0;
    x->fadeticks = (int)(x->srate / 1000 * x->fadetime); // no. of ticks to reach specified fade 'rate'
    x->firsttick = 1;
    x->fadealert = 0;
    for(i = 0; i < INPUTLIMIT; i++)
    {
        x->ip.active[i] = 0;
        x->ip.counter[i] = 0;
        x->ip.timeoff[i] = 0;
        x->ip.fade[i] = 0;
    }
    return (x);
    errstate:
        pd_error(x, "select~: improper args");
        return NULL;
}

void select_tilde_setup(void)
{
    select_class = class_new(gensym("select~"), (t_newmethod)select_new, 0,
			     sizeof(t_select), 0, A_GIMME, 0);
    class_addfloat(select_class, (t_method)select_float);
    class_addmethod(select_class, nullfn, gensym("signal"), 0);
    class_addmethod(select_class, (t_method)select_dsp, gensym("dsp"), 0);
    class_addmethod(select_class, (t_method)select_time, gensym("time"), A_FLOAT, (t_atomtype) 0);
    class_addmethod(select_class, (t_method)select_mode, gensym("mode"), A_FLOAT, (t_atomtype) 0);
}
