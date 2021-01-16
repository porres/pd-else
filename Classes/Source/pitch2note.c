
#include "m_pd.h"
 
typedef struct pitch2note{
    t_object x_obj;
    t_outlet *x_out_name;    // pitch2note name, e.g. "C1"
    t_int     x_last;        // last input
    t_int     x_last_dir;    // last direction
}t_pitch2note;

static void pitch2note_float(t_pitch2note *x, t_floatarg f){
    int pitch2note = f < 0 ? 0 : f > 127 ? 127 : (t_int)f;
    int octave = (pitch2note / 12) - 1;
	char* notes_up[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	char* notes_down[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
	int idx = pitch2note % 12;
    t_int interval = pitch2note - x->x_last;
    t_int dir;
    if(interval > 0)
        dir = 1;
    else if(interval < 0)
        dir = -1;
    else
        dir = x->x_last_dir;
    char buf[8];
    sprintf(buf, "%s%d", dir == 1 ? notes_up[idx] : notes_down[idx], octave);
	outlet_symbol(x->x_out_name, gensym(buf));
    x->x_last = pitch2note;
    x->x_last_dir = dir;
}

static t_class *pitch2note_class;

static void *pitch2note_new(void){
    t_pitch2note *x = (t_pitch2note *)pd_new(pitch2note_class);
    x->x_last = 0;
    x->x_last_dir = 1;
	x->x_out_name = outlet_new(&x->x_obj, gensym("symbol"));
    return(void *)x;
}

void pitch2note_setup(void){
    pitch2note_class = class_new(gensym("pitch2note"), (t_newmethod)pitch2note_new,
    	0, sizeof(t_pitch2note), 0, 0);
    class_addfloat(pitch2note_class, pitch2note_float);
}
