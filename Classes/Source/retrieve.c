// stolen from cyclone/grab

#include "m_pd.h"
#include "m_imp.h"

struct _outlet{
    t_object        *o_owner;
    struct _outlet  *o_next;
    t_outconnect    *o_connections;
    t_symbol        *o_sym;
};

t_outconnect *outlet_connections(t_outlet *o){ // obj_starttraverseoutlet() replacement
    return(o ? o->o_connections : 0); // magic
}

t_outconnect *outlet_nextconnection(t_outconnect *last, t_object **destp, int *innop){
    t_inlet *dummy;
    return(obj_nexttraverseoutlet(last, destp, &dummy, innop));
}

static t_class *bindlist_class = 0; // global variable

typedef struct _bindelem{
    t_pd                *e_who;
    struct _bindelem    *e_next;
}t_bindelem;

typedef struct _bindlist{
    t_pd b_pd;
    t_bindelem          *b_list;
}t_bindlist;

typedef struct _retrieve{
    t_object        x_ob;
    t_symbol       *x_target;
    int             x_noutlets;   // not counting right one
    t_object	   *x_receiver;   // object containing the receiver
    int				x_maxobs;	  // maximum number of connections from receiver
    t_object      **x_retrieved;    // array of retrieved objects
    t_outconnect  **x_retrievecons;   // array of retrieved connections
    int            *x_nretrieveout;   // array; number of retrieved object's outlets
    t_outlet       *x_rightout;   // right outlet
// traversal helpers:
    t_outconnect   *x_toretrieved;  // a connection to retrieved object
    t_bindelem     *x_bindelem;
    int             x_nonreceive; //* flag to signal whether processed non-[receive] object
}t_retrieve;

static t_class *retrieve_class;

static int retrieve_prep(t_retrieve *x, t_object *ob){
	t_outlet *op;
	t_outconnect *ocp;
	t_object *dummy;
	int ncons, inno;
	if(x->x_target){
		x->x_receiver = ob;
		op = ob->ob_outlet;
	}
	else
        op = x->x_rightout;
	if(x->x_receiver && (x->x_receiver->te_g.g_pd->c_name != gensym("receive")
    && x->x_receiver->te_g.g_pd->c_name != gensym("receiver")))
		ncons = 1;
	else {
        if(!(x->x_toretrieved = ocp = outlet_connections(op)))
            return(0);
        for(ncons = 0; ocp ; ++ncons)
            ocp = outlet_nextconnection(ocp, &dummy, &inno);
	}
	if (!x->x_retrieved){
		if (!((x->x_retrieved = getbytes(ncons * sizeof(*x->x_retrieved))) &&
			 (x->x_nretrieveout = getbytes(ncons * sizeof(*x->x_nretrieveout))) &&
			(x->x_retrievecons = getbytes(ncons * x->x_noutlets * sizeof(*x->x_retrievecons)))))
		{
			pd_error(x, "retrieve: error allocating memory");
			return(0);
		}
		x->x_maxobs = ncons;
	}
	else if(ncons > x->x_maxobs){
		if (!((x->x_retrieved = resizebytes(x->x_retrieved,
			x->x_maxobs * sizeof(*x->x_retrieved),
			ncons * sizeof(*x->x_retrieved))) &&
			
			(x->x_nretrieveout = resizebytes(x->x_nretrieveout,
			x->x_maxobs * sizeof(*x->x_nretrieveout),
			ncons * sizeof(*x->x_nretrieveout))) &&
			
			(x->x_retrievecons = resizebytes(x->x_retrievecons,
			x->x_maxobs * x->x_noutlets * sizeof(*x->x_retrievecons),
			ncons * x->x_noutlets * sizeof(*x->x_retrievecons)))))
		{
			pd_error(x, "retrieve: error allocating memory");
			return(0);
		}
		x->x_maxobs = ncons;
	}
	return(1);
}

static void retrieve_start(t_retrieve *x){
    x->x_toretrieved = 0;
    x->x_bindelem = 0;
    x->x_receiver = 0;
    x->x_nonreceive = 0;
    if(x->x_target){
        t_pd *proxy = x->x_target->s_thing;
        t_object *ob;
        if(proxy && bindlist_class){
            if (*proxy == bindlist_class){
                x->x_bindelem = ((t_bindlist *)proxy)->b_list;
                while (x->x_bindelem){
                    if((ob = pd_checkobject(x->x_bindelem->e_who)))
                        if(retrieve_prep(x, ob))
                        	return;
                    x->x_bindelem = x->x_bindelem->e_next;
                }
            }
            else if((ob = pd_checkobject(proxy)))
                retrieve_prep(x,ob);
        }
    }
    else
        retrieve_prep(x,&x->x_ob);
}

static int retrieve_next(t_retrieve *x){
	if (!(x->x_retrieved && x->x_retrievecons && x->x_nretrieveout))
		return(0);
	t_object **retrievedp = x->x_retrieved;
	t_outconnect **retrieveconsp = x->x_retrievecons;
	int *nretrieveoutp = x->x_nretrieveout;
	t_object *gr;
	int inno;
	t_outlet *op;
	t_outlet *goutp;
nextremote:
	// post("%s", x->x_receiver->te_g.g_pd->c_name->s_name);
	if(x->x_receiver && !(x->x_nonreceive) &&
    (x->x_receiver->te_g.g_pd->c_name != gensym("receive") &&
    x->x_receiver->te_g.g_pd->c_name != gensym("receiver"))){
		// post("nonreceive");
		*retrievedp = x->x_receiver;
		*nretrieveoutp = 1;
		*retrieveconsp = obj_starttraverseoutlet(x->x_receiver, &goutp, 0);
		goutp->o_connections = obj_starttraverseoutlet((t_object *)x, &op, 0);
		x->x_nonreceive = 1;
		return(1);
	}
    else if (x->x_toretrieved) {
		while(x->x_toretrieved){
			//post("entering retrieve_next while loop");
			x->x_toretrieved = outlet_nextconnection(x->x_toretrieved, &gr, &inno);
			if(gr){
				if(inno){
					if(x->x_rightout)
						pd_error(x, "retrieve: right outlet must feed leftmost inlet");
					else
						pd_error(x, "retrieve: remote proxy must feed leftmost inlet");
				}
				else{
					*retrievedp++ = gr;
					int goutno = obj_noutlets(gr);
					if (goutno > x->x_noutlets) goutno = x->x_noutlets;
					// post ("retrieve_next goutno: %d", goutno);
					*nretrieveoutp++ = goutno;
					for(int i = 0; i < x->x_noutlets; i++){
						if(i < goutno){
							*retrieveconsp++ = obj_starttraverseoutlet(gr, &goutp, i);
							goutp->o_connections = obj_starttraverseoutlet((t_object *)x, &op, i);
						}
					}
				}
			}
		}
		return(retrievedp-x->x_retrieved); // return number of objects stored
    }
    if(x->x_bindelem) while ((x->x_bindelem = x->x_bindelem->e_next)){
        t_object *ob;
        if((ob = pd_checkobject(x->x_bindelem->e_who))){
        	x->x_nonreceive = 0;
            retrieve_prep(x,ob);
            retrievedp = x->x_retrieved;
            retrieveconsp = x->x_retrievecons;
            nretrieveoutp = x->x_nretrieveout;
            goto
                nextremote;
        }
    }
    return(0);
}

static void retrieve_restore(t_retrieve *x, int nobs){
	t_object **retrievedp = x->x_retrieved;
	t_outconnect **retrieveconsp = x->x_retrievecons;
	int *nretrieveoutp = x->x_nretrieveout;
	int goutno;
	t_object *gr;
	t_outlet *goutp;
	while(nobs--){
		gr = *retrievedp++;
		goutno = *nretrieveoutp++;
		for(int i = 0; i < goutno ; i++){
			obj_starttraverseoutlet(gr, &goutp, i);
			goutp->o_connections = *retrieveconsp++;
		}
	}
}

static void retrieve_bang(t_retrieve *x){
	int nobs;
    retrieve_start(x);
    while((nobs = retrieve_next(x))){
        if(x->x_receiver)
        	pd_bang(&x->x_receiver->ob_pd);
        else
        	outlet_bang(x->x_rightout);
        retrieve_restore(x, nobs);
    }
}

static void retrieve_set(t_retrieve *x, t_symbol *s){
    if(x->x_target && s && s != &s_)
        x->x_target = s;
}

static void *retrieve_new(t_symbol *s){
    t_retrieve *x = (t_retrieve *)pd_new(retrieve_class);
    x->x_noutlets = 1;
    x->x_maxobs = 0;
    outlet_new((t_object *)x, &s_anything);
    x->x_target = (s && s != &s_) ? s : 0;
    x->x_rightout = 0;
    return(x);
}

static void retrieve_free(t_retrieve *x){
    if(x->x_retrieved)
        freebytes(x->x_retrieved, x->x_maxobs * sizeof(*x->x_retrieved));
    if(x->x_nretrieveout)
    	freebytes(x->x_nretrieveout, x->x_maxobs * sizeof(*x->x_nretrieveout));
    if(x->x_retrievecons)
    	freebytes(x->x_retrievecons, x->x_maxobs * x->x_noutlets * sizeof(*x->x_retrievecons));
}

void retrieve_setup(void){
    t_symbol *s = gensym("retrieve");
    retrieve_class = class_new(s, (t_newmethod)retrieve_new, (t_method)retrieve_free,
        sizeof(t_retrieve), 0, A_DEFSYMBOL, 0);
    class_addbang(retrieve_class, retrieve_bang);
    //    class_addfloat(retrieve_class, retrieve_float);
    //    class_addsymbol(retrieve_class, retrieve_symbol);
    //    class_addpointer(retrieve_class, retrieve_pointer);
    //    class_addlist(retrieve_class, retrieve_list);
    //    class_addanything(retrieve_class, retrieve_anything);
    class_addmethod(retrieve_class, (t_method)retrieve_set, gensym("set"), A_SYMBOL, 0);
    if(!bindlist_class){
        t_class *c = retrieve_class;
        pd_bind(&retrieve_class, s);
        pd_bind(&c, s);
        if (!s->s_thing || !(bindlist_class = *s->s_thing)
	    || bindlist_class->c_name != gensym("bindlist"))
            error("retrieve: failure to initialize remote retrievebing feature");
	pd_unbind(&c, s);
	pd_unbind(&retrieve_class, s);
    }
}
