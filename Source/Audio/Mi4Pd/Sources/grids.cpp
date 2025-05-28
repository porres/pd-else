#include "m_pd.h"
#include "grids_resources.h"
#include <math.h>
#include <stdlib.h>

static t_class *grids_class;

typedef struct _grids{
    t_object ob;
    
    t_int kNumParts;
    t_int kStepsPerPattern;
    //parameters
    t_int mode;
    t_float map_x;
    t_float map_y;
    t_float randomness;
    t_float euclidean_length[3];
    t_float density[3];
    t_float euclidean_step[3];
    
    //running vars
    t_int part_perturbation[3];
    t_int step;
    
    //output
    t_int velocities[3];
    t_int state;
    
    //outlets
    t_outlet *outlet_kick_gate;
    t_outlet *outlet_snare_gate;
    t_outlet *outlet_hihat_gate;
    t_outlet *outlet_kick_accent_gate;
    t_outlet *outlet_snare_accent_gate;
    t_outlet *outlet_hihat_accent_gate;
    
    //inlets
    t_inlet *grids_in_kick_density;
    t_inlet *grids_in_snare_density;
    t_inlet *grids_in_hihat_density;
    t_inlet *grids_in_map_x;
    t_inlet *grids_in_map_y;
    t_inlet *grids_in_randomness;
    t_inlet *grids_in_kick_euclidian_length;
    t_inlet *grids_in_snare_euclidian_length;
    t_inlet *grids_in_hihat_euclidian_length;
    
} t_grids;

extern "C" {
void grids_free(t_grids *x);
void *grids_new(t_symbol *s, long argc, t_atom *argv);
void grids_setup(void);

//inlet methods

void grids_in_mode_and_clock(t_grids *grids, t_floatarg n);

//grids
void grids_run(t_grids *grids, long playHead);
void grids_reset(t_grids *grids);
void grids_evaluate(t_grids *grids);
void grids_evaluate_drums(t_grids *grids);
t_int grids_read_drum_map(t_grids *grids, t_int instrument);
void grids_evaluate_euclidean(t_grids *grids);
void grids_output(t_grids *grids);
}

void grids_setup(void)
{
    grids_class = class_new(gensym("grids"), (t_newmethod)grids_new, (t_method)grids_free, sizeof(t_grids), CLASS_DEFAULT, A_GIMME, A_NULL);
    
    //Method space definition
    class_addfloat(grids_class, grids_in_mode_and_clock);

}

void *grids_new(t_symbol *s, long argc, t_atom *argv){
    
    // create class
    t_grids *grids = (t_grids*)pd_new(grids_class);
    
    // create inlets
    grids->grids_in_kick_density = floatinlet_new(&grids->ob, &grids->density[0]);
    grids->grids_in_snare_density = floatinlet_new(&grids->ob, &grids->density[1]);
    grids->grids_in_hihat_density = floatinlet_new(&grids->ob, &grids->density[2]);
    grids->grids_in_map_x = floatinlet_new(&grids->ob, &grids->map_x);
    grids->grids_in_map_y = floatinlet_new(&grids->ob, &grids->map_y);
    grids->grids_in_randomness = floatinlet_new(&grids->ob, &grids->randomness);
    grids->grids_in_kick_euclidian_length = floatinlet_new(&grids->ob, &grids->euclidean_length[0]);
    grids->grids_in_snare_euclidian_length = floatinlet_new(&grids->ob, &grids->euclidean_length[1]);
    grids->grids_in_hihat_euclidian_length = floatinlet_new(&grids->ob, &grids->euclidean_length[2]);
    
    // create outlets
    grids->outlet_kick_gate = outlet_new(&grids->ob, &s_float);
    grids->outlet_snare_gate = outlet_new(&grids->ob, &s_float);
    grids->outlet_hihat_gate = outlet_new(&grids->ob, &s_float);
    grids->outlet_kick_accent_gate = outlet_new(&grids->ob, &s_float);
    grids->outlet_snare_accent_gate = outlet_new(&grids->ob, &s_float);
    grids->outlet_hihat_accent_gate = outlet_new(&grids->ob, &s_float);
    
    //configuration
    grids->kNumParts = 3;
    grids->kStepsPerPattern = 32;
    
    //parameters
    grids->map_x = 64;
    grids->map_y = 64;
    grids->randomness = 10;
    grids->mode = 0;
    grids->euclidean_length[0] = 5;
    grids->euclidean_length[1] = 7;
    grids->euclidean_length[2] = 11;
    grids->density[0] = 32;
    grids->density[1] = 32;
    grids->density[2] = 32;
    
    //runing vars
    grids->part_perturbation[0] = 0;
    grids->part_perturbation[1] = 0;
    grids->part_perturbation[2] = 0;
    grids->euclidean_step[0] = 0;
    grids->euclidean_step[1] = 0;
    grids->euclidean_step[2] = 0;
    grids->step = 0;
    
    //output
    grids->state = 0;
    grids->velocities[0] = 0;
    grids->velocities[1] = 0;
    grids->velocities[2] = 0;
    
    return (void*)grids;
}


void grids_in_mode_and_clock(t_grids *grids, t_floatarg n)
{
 
    n=(t_int)n;
    if (n >= 0) {
        grids_run(grids, (t_int)n);
    } else {
        if (n == -1) {
            grids->mode = 0;
        }if (n == -2) {
            grids->mode = 1;
        }if (n == -3) {
            grids_reset(grids);
        }
    }
}

void grids_run(t_grids *grids, t_int playHead) {
    grids->step = playHead % 32;
    grids->state = 0;
    if (grids->mode == 1) {
        grids_evaluate_euclidean(grids);
    }
    else {
        grids_evaluate_drums(grids);
    }
    grids_output(grids);
    //increment euclidian clock.
    for (int i = 0; i < grids->kNumParts; i++)
        grids->euclidean_step[i] = (t_int)(grids->euclidean_step[i] + 1) % (t_int)grids->euclidean_length[i];
    
}

void grids_reset(t_grids *grids) {
    grids->euclidean_step[0] = 0;
    grids->euclidean_step[1] = 0;
    grids->euclidean_step[2] = 0;
    grids->step = 0;
    grids->state = 0;
}

void grids_output(t_grids *grids) {
    if ((grids->state & 1) > 0)
        outlet_float(grids->outlet_kick_gate, grids->velocities[0]);
    if ((grids->state & 2) > 0)
        outlet_float(grids->outlet_snare_gate, grids->velocities[1]);
    if ((grids->state & 4) > 0)
        outlet_float(grids->outlet_hihat_gate, grids->velocities[2]);
    
    if ((grids->state & 8) > 0)
        outlet_float(grids->outlet_kick_accent_gate, grids->velocities[0]);
    if ((grids->state & 16) > 0)
        outlet_float(grids->outlet_snare_accent_gate, grids->velocities[1]);
    if ((grids->state & 32) > 0)
        outlet_float(grids->outlet_hihat_accent_gate, grids->velocities[2]);
}
void grids_evaluate_drums(t_grids *grids) {
    // At the beginning of a pattern, decide on perturbation levels
    
    if (grids->step == 0) {
        for (int i = 0; i < grids->kNumParts; ++i) {
            t_int randomness = (t_int)grids->randomness >> 2;
#ifdef WIN32
            t_int randN = rand();
#else
            t_int randN = random();
#endif
            t_int rand2 = (t_int)(randN%256);
            grids->part_perturbation[i] = (rand2*randomness) >> 8;
        }
    }
    
    t_int instrument_mask = 1;
    t_int accent_bits = 0;
    for (int i = 0; i < grids->kNumParts; ++i) {
        t_int level = grids_read_drum_map(grids, i);
        if (level < 255 - grids->part_perturbation[i]) {
            level += grids->part_perturbation[i];
        } else {
            // The sequencer from Anushri uses a weird clipping rule here. Comment
            // this line to reproduce its behavior.
            level = 255;
        }
        t_int threshold = 255 - grids->density[i] * 2;
        if (level > threshold) {
            if (level > 192) {
                accent_bits |= instrument_mask;
            }
            grids->velocities[i] = level / 2;
            grids->state |= instrument_mask;
        }
        instrument_mask <<= 1;
    }
    grids->state |= accent_bits << 3;
}

t_int grids_read_drum_map(t_grids *grids, t_int instrument) {
    
    t_int x = grids->map_x;
    t_int y = grids->map_y;
    t_int step = grids->step;
    
    int i = (int)floor(x*3.0 / 127);
    int j = (int)floor(y*3.0 / 127);
    t_int* a_map = drum_map[i][j];
    t_int* b_map = drum_map[i + 1][j];
    t_int* c_map = drum_map[i][j + 1];
    t_int* d_map = drum_map[i + 1][j + 1];
    
    int offset = (instrument * grids->kStepsPerPattern) + step;
    t_int a = a_map[offset];
    t_int b = b_map[offset];
    t_int c = c_map[offset];
    t_int d = d_map[offset];
    t_int maxValue = 127;
    t_int r = (
               ( a * x	+ b * (maxValue - x) ) * y
               + ( c * x	+ d * (maxValue - x) ) * ( maxValue - y )
               ) / maxValue / maxValue;
    return r;
}

void grids_evaluate_euclidean(t_grids *grids) {
    t_int instrument_mask = 1;
    t_int reset_bits = 0;
    // Refresh only on sixteenth notes.
    if (!(grids->step & 1)) {
        for (int i = 0; i < grids->kNumParts; ++i) {
            grids->velocities[i] = 100;
            t_int density = (t_int)grids->density[i] >> 2;
            t_int address = (grids->euclidean_length[i] - 1) * 32 + density;
            t_int step_mask = 1L << (t_int)grids->euclidean_step[i];
            t_int pattern_bits = address < 1024 ? grids_res_euclidean[address] : 0;
            if (pattern_bits & step_mask)
                grids->state |= instrument_mask;
            if (grids->euclidean_step[i] == 0)
                reset_bits |= instrument_mask;
            instrument_mask <<= 1;
        }
    }
    grids->state |= reset_bits << 3;
}
void grids_free(t_grids *x)
{
}
