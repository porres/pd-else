//	based on [ipoke~] by Pierre Alexandre Tremblay

#include <stdbool.h>
#include "m_pd.h"

void *tabwriter2_class;

typedef struct _tabwriter2{
    t_object    x_obj;
    t_word     *x_buf;              // pointer to external array
    t_symbol   *x_sym;              // pointer to struct holding the name of external array
    long        x_bufsize;
	bool        x_overdub;
	long        x_idx_precedent;
	long        x_nb_val;
	t_float     x_value;
}t_tabwriter2;

static inline long wrap_idx(size_t idx, size_t arrayLength){ // why wrap????
	while(idx >= arrayLength)
        idx -= arrayLength;
    return(idx);
}

static void tabwriter2_overdub(t_tabwriter2 *x, t_floatarg f){
    x->x_overdub = f != 0;
}

void tabwriter2_set(t_tabwriter2 *x, t_symbol *s){
    t_garray *b;
    int bufsize;
    x->x_sym = s;
    if(!(b = (t_garray*)pd_findbyclass(x->x_sym, garray_class))){
        if(*x->x_sym->s_name) pd_error(x, "tabwriter2~: %s, no such array", x->x_sym->s_name);
        x->x_buf = 0;
    }
    else if(!garray_getfloatwords(b, &bufsize, &x->x_buf)){
        pd_error(x, "%s: bad template for tabwriter2~", x->x_sym->s_name);
        x->x_buf = 0;
    }
    else{
        x->x_bufsize = bufsize;
        garray_usedindsp(b);
    }
}

static t_int *tabwriter2_perform(t_int *w){
    t_tabwriter2 *x = (t_tabwriter2 *)(w[1]);
    if(!x->x_buf)
        goto out; // skip if buffer does not exist
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    int n = (int)(w[4]);
    t_word *tab = x->x_buf;
    t_float value = x->x_value, coeff;
    long idx = 0, pos, i;
    long bufsize = x->x_bufsize;
    long half = (long)(bufsize * 0.5);
    long idx_precedent = x->x_idx_precedent;
    long nb_val = x->x_nb_val;
    while(n--){
        t_float input = *in1++;
        t_float index = *in2++;
        if(index < 0.0){ // if writing is stopped
            if(idx_precedent >= 0){ // and if it is the 1st one to be stopped
                // write the average value at the last given idx
                tab[idx_precedent].w_float = (tab[idx_precedent].w_float * x->x_overdub) + value/nb_val;
                value = 0.0;
                idx_precedent = -1;
            }
        }
        else{ // if writing
            idx = wrap_idx((long)(index + 0.5), bufsize);  // round next idx and make sure it's within boundaries
            if(idx_precedent < 0){ // if it's the first idx to write, resets the averaging and the values
                idx_precedent = idx;
                nb_val = 0;
            }
            if(idx == idx_precedent){ // if the idx has not moved, accumulate the value to average later
                value += input;
                nb_val += 1;
            }
            else{ // if it moves
                if(nb_val != 1){ // if there more than one values to average
                    value = value/nb_val; // calculate the average
                    nb_val = 1;
                }
                tab[idx_precedent].w_float = (tab[idx_precedent].w_float * x->x_overdub) + value; // write average at last idx
                pos = idx - idx_precedent; // calculate the step to do
                if(pos > 0){ // if we're we going up
                    if(pos > half){ // is it faster to go the other way round?
                        pos -= bufsize; // calculate the new number of steps
                        coeff = (input - value) / pos; // calculate the interpolation coefficient
                        for(i = (idx_precedent-1); i >= 0; i--){ // fill the gap to zero
                            value -= coeff;
                            tab[i].w_float = (tab[i].w_float * x->x_overdub) + value;
                        }
                        for(i = (bufsize-1); i > idx; i--){ // fill the gap from the top
                            value -= coeff;
                            tab[i].w_float = (tab[i].w_float * x->x_overdub) + value;
                        }
                    }
                    else{  // if not, just fill the gaps
                        coeff = (input - value) / pos; // calculate the interpolation coefficient
                        for(i = (idx_precedent+1); i < idx; i++){
                            value += coeff;
                            tab[i].w_float = (tab[i].w_float * x->x_overdub) + value;
                        }
                    }
                } // end if we are going up
                else{  // if we are going down
                    if((-pos) > half){  // is it faster to go the other way round?
                        pos += bufsize; // calculate the new number of steps
                        coeff = (input - value) / pos; // calculate the interpolation coefficient
                        for(i = (idx_precedent+1); i < bufsize; i++){ // fill the gap to the top
                            value += coeff;
                            tab[i].w_float = (tab[i].w_float * x->x_overdub) + value;
                        }
                        for(i = 0; i < idx; i++){ // fill the gap from zero
                            value += coeff;
                            tab[i].w_float = (tab[i].w_float * x->x_overdub) + value;
                        }
                    }
                    else{  // if not, just fill the gaps
                        coeff = (input - value) / pos; // calculate the interpolation coefficient
                        for(i = (idx_precedent-1); i > idx; i--){
                            value -= coeff;
                            tab[i].w_float = (tab[i].w_float * x->x_overdub) + value;
                        }
                    }
                } // end if we are going down
                value = input; // transfer the new previous value
            } // end of else (if it moves)
        } // end of else (if writing)
        idx_precedent = idx;  // transfer the new previous address
    } // end of while(n--)
    x->x_idx_precedent = idx_precedent;
    x->x_value = value;
    x->x_nb_val = nb_val;
out:
	return(w+5);
}

void tabwriter2_dsp(t_tabwriter2 *x, t_signal **sp){
	x->x_idx_precedent = -1;
    tabwriter2_set(x, x->x_sym);
    dsp_add(tabwriter2_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

static void *tabwriter2_new(t_symbol *s){
    t_tabwriter2 *x = (t_tabwriter2*)pd_new(tabwriter2_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal); // inlet for float indices
    x->x_sym = s;
    x->x_overdub = 0;
    x->x_idx_precedent = -1;
    return(x);
}

void tabwriter2_tilde_setup(void){
    tabwriter2_class = class_new(gensym("tabwriter2~"), (t_newmethod)tabwriter2_new, 0, sizeof(t_tabwriter2), 0, A_DEFSYM, 0);
    class_addmethod(tabwriter2_class, nullfn, gensym("signal"), 0);
    class_addmethod(tabwriter2_class, (t_method)tabwriter2_dsp, gensym("dsp"), 0);
    class_addmethod(tabwriter2_class, (t_method)tabwriter2_set, gensym("set"), A_SYMBOL,0);
    class_addmethod(tabwriter2_class, (t_method)tabwriter2_overdub, gensym("overdub"), A_FLOAT, 0);
}
