// porres 2017

#include "m_pd.h"

typedef struct _loop
{
    t_object   x_obj;
    t_float    x_target;
    t_float    x_counter_start;
    int        x_count;
    int        x_running;
} t_loop;

static t_class *loop_class;

#define loop_RUNNING  1
#define loop_PAUSED   2

static void loop_dobang(t_loop *x)
{
if (!x->x_running)
    {
    int count = (int)x->x_counter_start; // ?
    int target = (int)x->x_target;
    int counter_start = (int)x->x_counter_start;
    x->x_running = loop_RUNNING;
// continue from where it stoped, even if counter_start changed
    for (count = x->x_count; count < target; count++)
        {
        outlet_float(((t_object *)x)->ob_outlet, x->x_count);
        x->x_count += 1;
        if (x->x_running == loop_PAUSED)
            {
            return;
            }
        }
    x->x_count = counter_start;
    x->x_running = 0;
    }
}

static void loop_bang(t_loop *x)
{   // always starts
    x->x_count = (int)x->x_counter_start;
    x->x_running = 0;
    loop_dobang(x);
}

static void loop_float(t_loop *x, t_float f)
{   // always sets a new value and recounter_starts
    x->x_target = f;
    loop_bang(x);
}

// BUG? 'pause, continue' send carry bang when not running ?????
static void loop_pause(t_loop *x)
{
    if (!x->x_running)
	x->x_count = (int)x->x_target;
    x->x_running = loop_PAUSED;
}

static void loop_resume(t_loop *x)
{
    if (x->x_running == loop_PAUSED)
    {
	x->x_running = 0;
	loop_dobang(x);
    }
}

static void loop_counter_start(t_loop *x, t_float f)
{
    x->x_counter_start = (int)f;
}

static void *loop_new(t_symbol *s, int ac, t_atom *av)
{
    t_loop *x = (t_loop *)pd_new(loop_class);
    t_float f1 = 0, f2 = 0;
	switch(ac){
	default:
	case 2:
		f2=atom_getfloat(av+1);
	case 1:
		f1=atom_getfloat(av);
		break;
	case 0:
		break;
	}
    if (f2)
        {
        x->x_counter_start = f1;
        x->x_target = (f2 < 0. ? 0 : f2);
        }
    else if (f1)
        x->x_target = (f1 < 0. ? 0 : f1);
    x->x_count = x->x_counter_start;
    x->x_running = 0;
    // bang inlet (until like), for 'loop' method
    outlet_new((t_object *)x, &s_float);
    return (x);
}

void loop_setup(void)
{
    loop_class = class_new(gensym("loop"),
			  (t_newmethod)loop_new, 0,
			  sizeof(t_loop), 0, A_GIMME, 0);
    class_addbang(loop_class, loop_bang);
    class_addfloat(loop_class, loop_float);
    class_addmethod(loop_class, (t_method)loop_pause, gensym("pause"), 0);
    class_addmethod(loop_class, (t_method)loop_resume, gensym("continue"), 0);
    class_addmethod(loop_class, (t_method)loop_counter_start, gensym("start"), A_DEFFLOAT, 0);
}
