// from polygate

#include "m_pd.h"
#include <math.h>
#include <string.h>

#define HALF_PI M_PI  * 0.5

static t_class *select2_class;

#define INPUTLIMIT 256

#define TIMEUNITPERSEC (32.*441000.) // ???

typedef struct _ip // keeps track of each signal input
{
  int active[INPUTLIMIT]; 
  int counter[INPUTLIMIT];
  double timeoff[INPUTLIMIT];
  float fade[INPUTLIMIT];
  float *in[INPUTLIMIT];
} t_ip;

typedef struct _select2
{
  t_object x_obj;
  float x_f;
  int choice;
  int lastchoice;
  int actuallastchoice;
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
} t_select2;

static void *select2_new(t_symbol *s, int argc, t_atom *argv)
{
  int usedefault = 0, i;
  t_select2 *x = (t_select2 *)pd_new(select2_class);
  x->srate = sys_getsr();	
  if(argc == 0 || argc > 3)
    usedefault = 1;
  else if(argc == 1 && argv[0].a_type != A_FLOAT)
    usedefault = 1;
  else if(argc >= 2)
    if(argv[0].a_type != A_FLOAT || argv[1].a_type != A_FLOAT)
      usedefault = 1;
  if(usedefault)
    {
      x->ninlets = 1;
      x->fadetime = 1;
    } 
  else
    {
      x->ninlets = argv[0].a_w.w_float < 1 ? 1 : argv[0].a_w.w_float;
      if(x->ninlets > INPUTLIMIT)
	{
	  x->ninlets = INPUTLIMIT;
	  post("select2~: maximum of %d inlets", INPUTLIMIT);
	}
      x->fadetime = argv[1].a_w.w_float > 0 ? argv[1].a_w.w_float : 1;
    }
  for(i = 0; i < x->ninlets - 1; i++)
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  outlet_new(&x->x_obj, gensym("signal"));
  x->choice = 0; x->lastchoice = x->actuallastchoice = 0;
  x->fadecount = 0;
  x->fadeticks = (int)(x->srate / 1000 * x->fadetime); // no. of ticks to reach specified fade 'rate'
  x->firsttick = 1;
  x->fadealert = 0;
  x->x_f = 0;
  for(i = 0; i < INPUTLIMIT; i++) 
    {
      x->ip.active[i] = 0;
      x->ip.counter[i] = 0;
      x->ip.timeoff[i] = 0;
      x->ip.fade[i] = 0;
    }
  return (x);
}


void select2_f(t_select2 *x, t_floatarg f)
{
  f = (int)f;
  f = f > x->ninlets ? x->ninlets : f;
  f = f < 0 ? 0 : f;
  if(f != x->lastchoice)
    {
      if(f == x->actuallastchoice)
	x->fadecount = x->fadeticks - x->fadecount;
      else
	x->fadecount = 0;	
      x->choice = f;
      if(x->choice)
	{
	  x->ip.active[x->choice - 1] = 1;
	}
      if(x->lastchoice)
	{
	  x->ip.active[x->lastchoice - 1] = 0;
	  x->ip.timeoff[x->lastchoice - 1] = clock_getlogicaltime();
	}
      x->actuallastchoice = x->lastchoice;
      x->lastchoice = x->choice;
    }
}


static void checkswitchstatus(t_select2 *x) // checks to see which input feeds ought to be "switch~"ed off 
{
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
}

static void updatefades(t_select2 *x)
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


static void outputfades(t_int *w, int flag)
{
  t_select2 *x = (t_select2 *)(w[1]);
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
	      sum += *x->ip.in[i]++ * epower(x->ip.fade[i]);
	  }
      *out++ = sum;
    }
}

static t_int *select2_perform(t_int *w)
{
  t_select2 *x = (t_select2 *)(w[1]);
  int n = (int)(w[2]);
  float *out = (t_float *)(w[3+x->ninlets]);
  if (x->actuallastchoice == 0 && x->choice == 0 && x->lastchoice == 0) { // init state
      if(x->firsttick) {
          int i;
          x->firsttick = 0;
          }
      while (n--)
      *out++ = 0;
      }
  else if (x->actuallastchoice == 0 && x->choice != 0) outputfades(w, x->fadetype); // change from 0 to non-0
  else if(x->choice != 0) outputfades(w, 1); // change from non-0 to another non-0
  else if (x->actuallastchoice != 0 && x->choice == 0) outputfades(w, x->fadetype); // change from non-0 to 0
  checkswitchstatus(x);
  return (w+4+x->ninlets);
}

static void select2_dsp(t_select2 *x, t_signal **sp)
{
  int n = sp[0]->s_n, i; // there must be a smarter way....
  switch (x->ninlets) 
    {
    case 1: dsp_add(select2_perform, 4, x, n, sp[0]->s_vec, sp[1]->s_vec);
      break;
    case 2: dsp_add(select2_perform, 5, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
      break;
    case 3: dsp_add(select2_perform, 6, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
      break;
    case 4: dsp_add(select2_perform, 7, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
      break;
    case 5: dsp_add(select2_perform, 8, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
      break;
    case 6: dsp_add(select2_perform, 9, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec);
      break;
    case 7: dsp_add(select2_perform, 10, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec);
      break;
    case 8: dsp_add(select2_perform, 11, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, 
		    sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec);
    break;
    case 9: dsp_add(select2_perform, 12, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, 
		    sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec, sp[9]->s_vec);
    break;
    case 10: dsp_add(select2_perform, 13, x, n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, 
		     sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec, sp[9]->s_vec, sp[10]->s_vec);
    break;
    }
}

static void shortcheck(t_select2 *x, int newticks, int shorter)
{
  int i;
  for(i = 0; i < x->ninlets; i++)
    {
      if(shorter && x->ip.timeoff[i]) // correct active timeoffs for new x->fadeticks (newticks)
	x->ip.timeoff[i] = clock_getlogicaltime() - ((newticks - x->ip.counter[i]) / (x->srate / 1000.) - 1) * (TIMEUNITPERSEC / 1000.);
    }
}
static void adjustcounters_ftimechange(t_select2 *x, int newticks, int shorter)
{
  int i;
  shortcheck(x, newticks, shorter);
  for(i = 0; i < x->ninlets; i++)
    {
      if(x->ip.counter[i])
	x->ip.counter[i] = x->ip.fade[i] * (float)newticks;
    }
}

void select2_time(t_select2 *x, t_floatarg ftime)
{
  int newticks, i, shorter;
  ftime = ftime < 1 ? 1 : ftime;
  shorter = ftime < x->fadetime ? 1 : 0;
  x->fadetime = (int)ftime;
  newticks = (int)(x->srate / 1000 * x->fadetime); // no. of ticks to reach specified fade time
  x->fadeticks = newticks;
  adjustcounters_ftimechange(x, x->fadeticks, shorter);
}

void select2_tilde_setup(void)
{
  select2_class = class_new(gensym("select2~"), (t_newmethod)select2_new, 0,
			     sizeof(t_select2), 0, A_GIMME, 0);
  class_addmethod(select2_class, nullfn, gensym("signal"), 0);
  class_addmethod(select2_class, (t_method)select2_dsp, gensym("dsp"), 0);
  CLASS_MAINSIGNALIN(select2_class, t_select2, x_f);
  class_addmethod(select2_class, (t_method)select2_f, gensym("choice"), A_FLOAT, 0);  
  class_addmethod(select2_class, (t_method)select2_time, gensym("time"), A_FLOAT, (t_atomtype) 0);
}
