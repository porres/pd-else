// Porres 2017 from mask~

#include "m_pd.h"

static t_class *sequencer_class;

#define MAXLEN 1024
#define MAXsequencerS 1024
#define MAXSEQ 1024

typedef struct{
    float *pat; // sequencer pattern
    int length;// length of pattern
} t_sequencerpat;

typedef struct{
    int *seq; // sequencer pattern
    int length;// length of pattern
    int phase; // keep track of where we are in sequence
} t_sequence;

typedef struct _sequencer{
    t_object x_obj;
    float x_f;
    float x_lastin;
    int x_bang;
    short mute; // stops all computation (try z-disable)
    short gate; // continues sequencering but inhibits all output
    short phaselock; // indicates all patterns are the same size and use the same phase count
    short indexmode; //special mode where input clicks are also sequencer indicies (+ 1)
    int phase; //phase of current pattern
    int current_sequencer; // currently selected pattern
    t_sequencerpat *sequencers; // contains the sequencer patterns
    t_sequence sequence; // contains an optional sequencer sequence
    int *stored_sequencers; // a list of patterns stored
    int pattern_count; //how many patterns are stored
    short noloop; // flag to play pattern only once
    float *in_vec; // copy space for input to avoid dreaded vector sharing override
} t_sequencer;

static void sequencer_bang(t_sequencer *x){
    x->x_bang = 1;
}

void sequencer_float(t_sequencer *x, t_floatarg f){
        x->sequencers[0].pat = (float *) malloc(MAXLEN * sizeof(float));
        x->sequencers[0].length = 1;
        x->sequencers[0].pat[0] = f;
        x->current_sequencer = 0; // now we use the sequencer we read from the arguments
        x->stored_sequencers[0] = 0;
        x->pattern_count = 1;
        x->x_bang = 1;
}

static void sequencer_list(t_sequencer *x, t_symbol *s, int argc, t_atom * argv){
    int i;
    x->sequencers[0].pat = (float *) malloc(MAXLEN * sizeof(float));
    x->sequencers[0].length = argc;
    for(i = 0; i < argc; i++){
        x->sequencers[0].pat[i] = atom_getfloatarg(i,argc,argv);
    }
    x->current_sequencer = 0; // now we use the sequencer we read from the arguments
    x->stored_sequencers[0] = 0;
    x->pattern_count = 1;
    x->phase = 0;
    x->x_bang = 1;
}

static void sequencer_set(t_sequencer *x, t_symbol *s, int argc, t_atom * argv){
    x->phase = 0;
    if(!argc){
        x->sequencers[0].pat = (float *) malloc(MAXLEN * sizeof(float));
        x->sequencers[0].length = 1;
        x->sequencers[0].pat[0] = 1;
        x->current_sequencer = 0; // now we use the sequencer we read from the arguments
        x->stored_sequencers[0] = 0;
        x->pattern_count = 1;
    }
    else {
        int i;
        x->sequencers[0].pat = (float *) malloc(MAXLEN * sizeof(float));
        x->sequencers[0].length = argc;
        for(i = 0; i < argc; i++){
            x->sequencers[0].pat[i] = atom_getfloatarg(i,argc,argv);
        }
        x->current_sequencer = 0; // now we use the sequencer we read from the arguments
        x->stored_sequencers[0] = 0;
        x->pattern_count = 1;
    }
}

void sequencer_indexmode(t_sequencer *x, t_floatarg t){
	x->indexmode = (short)t;
}

void sequencer_gozero(t_sequencer *x){
    x->phase = 0;
}

void sequencer_mute(t_sequencer *x, t_floatarg f){
    x->mute = (short)f;
}

void sequencer_noloop(t_sequencer *x, t_floatarg f){
    x->noloop = (short)f;
}

void sequencer_phaselock(t_sequencer *x, t_floatarg f){
    x->phaselock = (short)f;
}

void sequencer_gate(t_sequencer *x, t_floatarg f){
    x->gate = (short)f;
}

void sequencer_showsequencer(t_sequencer *x, t_floatarg p){
    int location = p;
    short found = 0;
    int i;
    int len;
    
    for(i = 0; i<x->pattern_count; i++){
        if(location == x->stored_sequencers[i]){
            found = 1;
            break;
        }
    }
    if(found){
        len = x->sequencers[location].length;
        post("pattern length is %d",len);
        for(i = 0; i < len; i++){
            post("%d: %f",i,x->sequencers[location].pat[i]);
        }
        
    }
    else {
        error("no pattern stored at location %d",location);
    }
}

void sequencer_recall(t_sequencer *x, t_floatarg p)
{
    int i;
    int location = p;
    short found = 0;
    for(i = 0; i < x->pattern_count; i++){
        if(location == x->stored_sequencers[i]){
            found = 1;
            break;
        }
    }
    if(found){
        x->current_sequencer = location;
        if(! x->phaselock){
            x->phase = 0;
        }
    } else {
        error("no pattern stored at location %d",location);
    }
}

void sequencer_playonce(t_sequencer *x, t_floatarg pnum){
    x->noloop = 1;
    x->mute = 0;
    sequencer_recall(x,pnum);
}

// initiate sequencer recall sequence
void sequencer_sequence(t_sequencer *x, t_symbol *msg, short argc, t_atom *argv){
    int i;
	if(argc > MAXSEQ){
		error("%d exceeds possible length for a sequence",argc);
		return;
	}
	if(argc < 1){
		error("you must sequence at least 1 sequencer");
		return;
	}
	for(i = 0; i < argc; i++){
		x->sequence.seq[i] = atom_getfloatarg(i,argc,argv);
	}
	if(x->sequence.seq[0] < 0){
		post("sequencing turned off");
		x->sequence.length = 0;
		return;
	}
	x->sequence.phase = 0;
	x->sequence.length = argc;
	// now load in first sequencer of sequence
	sequencer_recall(x, (t_floatarg)x->sequence.seq[x->sequence.phase++]);
	
	// ideally would check that each sequence number is a valid stored location
}

void sequencer_addsequencer(t_sequencer *x, t_symbol *msg, short argc, t_atom *argv){
    int location;
    int i;
    if(argc < 2){
        error("must specify location and sequencer");
        return;
    }
    if(argc > MAXLEN){
        error("sequencer is limited to length %d",MAXLEN);
        return;
    }
    location = atom_getintarg(0,argc,argv);
    if(location < 0 || location > MAXsequencerS - 1){
        error("illegal location");
        return;
    }
    if(x->sequencers[location].pat == NULL){
        x->sequencers[location].pat = (float *) malloc(MAXLEN * sizeof(float));
        x->stored_sequencers[x->pattern_count++] = location;
    }
    else {
    //    post("replacing pattern stored at location %d", location);
    }
    //  post("reading new sequencer from argument list, with %d members",argc-1);
    x->sequencers[location].length = argc-1;
    for(i=1; i<argc; i++){
        x->sequencers[location].pat[i-1] = atom_getfloatarg(i,argc,argv);
    }
    //  post("there are currently %d patterns stored",x->pattern_count);
}

t_int *sequencer_perform(t_int *w){
    t_sequencer *x = (t_sequencer *) (w[1]);
    float *inlet = (t_float *) (w[2]);
    float *outlet = (t_float *) (w[3]);
    int nblock = (int) w[4];
    t_float lastin = x->x_lastin;
    int phase = x->phase;
    short gate = x->gate;
    short indexmode = x->indexmode;
    short noloop = x->noloop;
    int current_sequencer = x->current_sequencer;
    t_sequencerpat *sequencers = x->sequencers;
    t_sequence sequence = x->sequence;
    while (nblock--){
        float input = *inlet++;
        float output;
/*        if(x->mute || current_sequencer < 0)
            *outlet++ = 0;
          else{ */
            if((input != 0 && lastin == 0) || x->x_bang){ // trigger
/*              if(indexmode){ // input controls the phase
                    phase = input - 1;
                    if(phase < 0 || phase >= sequencers[current_sequencer].length)
                        phase %= sequencers[current_sequencer].length;
                } */
                phase++; // next phase
                if(phase >= sequencers[current_sequencer].length){
                    phase = 0;
                    if(sequence.length){ // if a sequence is active, reset the current sequencer too
                        sequencer_recall(x, (t_floatarg)sequence.seq[sequence.phase++]);
                        current_sequencer = x->current_sequencer; // this was reset internally!
                        if(sequence.phase >= sequence.length)
                            sequence.phase = 0;
                    }
                }
            x->x_bang = 0;
            }
//        }
        *outlet++ = sequencers[current_sequencer].pat[phase];
        lastin = input;
    }
    x->x_lastin = lastin;
    x->phase = phase;
    x->sequence.phase = sequence.phase;
    return (w+5);
}

void sequencer_dsp(t_sequencer *x, t_signal **sp){
    dsp_add(sequencer_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void sequencer_free(t_sequencer *x){
    int i;
    for(i=0;i<x->pattern_count;i++)
        free(x->sequencers[i].pat);
    free(x->sequencers);
    free(x->stored_sequencers);
    free(x->sequence.seq);
    free(x->in_vec);
}

void *sequencer_new(t_symbol *msg, short argc, t_atom *argv)
{
    int i;
    t_sequencer *x = (t_sequencer *)pd_new(sequencer_class);
    outlet_new(&x->x_obj, gensym("signal"));
    
    x->sequencers = (t_sequencerpat *) malloc(MAXsequencerS * sizeof(t_sequencerpat));
    x->stored_sequencers = (int *) malloc(MAXsequencerS * sizeof(int));
    
    x->sequence.seq = (int *) malloc(MAXSEQ * sizeof(int));
    
    x->sequence.length = 0; // no sequence by default
    x->sequence.phase = 0; //
    
    //	post("allocated %d bytes for basic sequencer holder",MAXsequencerS * sizeof(t_sequencerpat));
    
    /*    x->current_sequencer = -1; // by default no sequencer is selected
     for(i = 0; i < MAXsequencerS; i++){
     x->stored_sequencers[i] = -1; // indicates no pattern stored
     x->sequencers[i].pat = NULL;
     } */
    if(argc == 0){
        x->sequencers[0].pat = (float *) malloc(MAXLEN * sizeof(float));
        x->sequencers[0].length = 1;
        x->sequencers[0].pat[0] = 0;
        x->current_sequencer = 0; // now we use the sequencer we read from the arguments
        x->stored_sequencers[0] = 0;
        x->pattern_count = 1;
    }
    else{ // post("reading initial sequencer from argument list, with %d members",argc);
        x->sequencers[0].pat = (float *) malloc(MAXLEN * sizeof(float));
        // post("allocated %d bytes for this pattern", MAXLEN * sizeof(float));
        x->sequencers[0].length = argc;
        for(i = 0; i < argc; i++){
            x->sequencers[0].pat[i] = atom_getfloatarg(i,argc,argv);
        }
        x->current_sequencer = 0; // now we use the sequencer we read from the arguments
        x->stored_sequencers[0] = 0;
        x->pattern_count = 1;
    }
    x->indexmode = 0;
    x->mute = 0;
    x->gate = 1; //by default gate is on, and the pattern goes out (zero gate turns it off)
    x->phaselock = 0;// by default do NOT use a common phase for all patterns
    x->phase = 0;
    x->noloop = 0;
    
    return x;
}

void sequencer_tilde_setup(void){
    sequencer_class = class_new(gensym("sequencer~"), (t_newmethod)sequencer_new,
                             (t_method)sequencer_free ,sizeof(t_sequencer), 0,A_GIMME,0);
    CLASS_MAINSIGNALIN(sequencer_class, t_sequencer, x_f);
    class_addmethod(sequencer_class,(t_method)sequencer_dsp,gensym("dsp"),0);
    class_addmethod(sequencer_class,(t_method)sequencer_mute,gensym("mute"),A_FLOAT,0);
    class_addmethod(sequencer_class,(t_method)sequencer_phaselock,gensym("phaselock"),A_FLOAT,0);
    class_addmethod(sequencer_class,(t_method)sequencer_gate,gensym("gate"),A_FLOAT,0);
    class_addmethod(sequencer_class,(t_method)sequencer_addsequencer,gensym("addsequencer"),A_GIMME,0);
    class_addmethod(sequencer_class,(t_method)sequencer_sequence,gensym("sequence"),A_GIMME,0);
    class_addmethod(sequencer_class,(t_method)sequencer_recall,gensym("recall"),A_FLOAT,0);
    class_addmethod(sequencer_class,(t_method)sequencer_showsequencer,gensym("showsequencer"),A_FLOAT,0);
    class_addmethod(sequencer_class,(t_method)sequencer_indexmode,gensym("indexmode"),A_FLOAT,0);
    class_addmethod(sequencer_class,(t_method)sequencer_playonce,gensym("playonce"),A_FLOAT,0);
    class_addmethod(sequencer_class,(t_method)sequencer_noloop,gensym("noloop"),A_FLOAT,0);
    class_addmethod(sequencer_class,(t_method)sequencer_gozero,gensym("gozero"),0);
    class_addmethod(sequencer_class,(t_method)sequencer_set, gensym("set"),A_GIMME,0);
    class_addbang(sequencer_class, (t_method)sequencer_bang);
    class_addfloat(sequencer_class, (t_method)sequencer_float);
    class_addlist(sequencer_class, (t_method)sequencer_list);
}


