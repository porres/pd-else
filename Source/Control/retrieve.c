// based from cyclone/grab and simplified

#include <m_pd.h>
#include <m_imp.h>
#include <string.h>

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
    t_bindelem  *b_list;
}t_bindlist;

/* pdlua binds a receive name to a small proxy, not to the Lua object itself.
   Its class is registered as patchable, so pd_checkobject() takes it for a
   whole t_object although it has no inlets or outlets, and using it as one
   writes past it. What we want is the Lua object (a GUI) that owns it; a bang
   to it has to go through the proxy, as the object takes no messages itself. */
typedef struct _pdlua_proxyreceive{
    t_pd        p_pd;
    t_object   *p_owner; // the pdlua object, which starts with its t_object
    t_symbol   *p_name;
}t_pdlua_proxyreceive;

static int retrieve_islua(t_pd *x){
    return(!strcmp(class_getname(*x), "pdlua proxy receive"));
}

static t_object *retrieve_checkobject(t_pd *x){
    if(retrieve_islua(x))
        return(((t_pdlua_proxyreceive *)x)->p_owner);
    return(pd_checkobject(x));
}

/* A Lua GUI is asked with "retrieve" rather than a bang. It answers with its
   whole state in one message, as [presets] needs it ([multi.vsl] outputs one
   message per slider on a bang), and not to its send name; the abstractions
   these GUIs replace did the same with a [receiver] and a [default] inside. */
static void retrieve_ask(t_pd *x){
    if(retrieve_islua(x))
        pd_typedmess(x, gensym("retrieve"), 0, 0);
    else
        pd_bang(x);
}

typedef struct _retrieve{
    t_object        x_obj;
    t_symbol       *x_rcv_name;     // the receive name to retrieve from
//    t_outlet       *x_rightout;   // right outlet (if there's no receive name)
    t_object	   *x_receiver;     // object containing the receiver
    t_pd           *x_bangto;       // where to bang it: what is bound to the name
    int				x_maxcons;	    // maximum number of connections from receiver
    t_object      **x_retrieved;    // array of retrieved objects
    t_outconnect  **x_retrievecons; // array of retrieved connections
// traversal helpers:
    t_outconnect   *x_to_retr_con;  // a connection to retrieved object
    t_bindelem     *x_bindelem;
    int             x_nonreceive;   // flag to signal whether processed non-[receive] object
}t_retrieve;

static t_class *retrieve_class;

// assign the receiver and init buffers for storing objects & connections
static int retrieve_prep(t_retrieve *x, t_object *obj){
	t_outlet *op;       // outlet pointer
	t_outconnect *ocp;  // outlet connection pointer
	t_object *dummy;
	int ncons, inno;
	if(x->x_rcv_name){
		x->x_receiver = obj;
		op = obj->ob_outlet;
	}
//	else op = x->x_rightout;
    /* if receiver isn't a [receive] object, we want to retrieve its outlet
       so there's only one "connection." Otherwise we need to count the
       connections to the objects we're going to retrieve. If there aren't
       any, there's nothing left to do. */
	if(x->x_receiver && (x->x_receiver->te_g.g_pd->c_name != gensym("receive")
    && x->x_receiver->te_g.g_pd->c_name != gensym("receiver")))
		ncons = 1;
	else{ // not a [receive] object, retrieve connections
        if(!(x->x_to_retr_con = ocp = outlet_connections(op)))
            return(0);
        for(ncons = 0; ocp; ++ncons)
            ocp = outlet_nextconnection(ocp, &dummy, &inno);
	}
	/* initialize three buffers.
	   x_retrieved stores pointers to the objects we're going to retrieve.
	   x_retrievecons stores pointers to the t_outconnect connection lists for each
	      retrieved outlet. The max we would need to store is number of retrieved
	      objects times the number of [retrieve]'s outlets.*/
	if(!x->x_retrieved){
		if(!((x->x_retrieved = getbytes(ncons * sizeof(*x->x_retrieved))) &&
			(x->x_retrievecons = getbytes(ncons * sizeof(*x->x_retrievecons)))))
		{
			pd_error(x, "retrieve: error allocating memory");
			return(0);
		}
		x->x_maxcons = ncons;
	}
	// get more memory if we need to because of the number of connecctions
	else if(ncons > x->x_maxcons){
		if(!((x->x_retrieved = resizebytes(x->x_retrieved,
			x->x_maxcons * sizeof(*x->x_retrieved),
			ncons * sizeof(*x->x_retrieved))) &&
			
			(x->x_retrievecons = resizebytes(x->x_retrievecons,
			x->x_maxcons * sizeof(*x->x_retrievecons),
			ncons * sizeof(*x->x_retrievecons)))))
		{
			pd_error(x, "retrieve: error allocating memory");
			return(0);
		}
		x->x_maxcons = ncons;
	}
	return(1);
}

// run prep routines and find bindlist for the send/receive symbol
static void retrieve_start(t_retrieve *x){
    x->x_to_retr_con = 0;
    x->x_bindelem = 0;
    x->x_receiver = 0;
    x->x_nonreceive = 0;
    if(x->x_rcv_name){ // if we're looking for a receive name
     // Two things can be bound to a symbol and stored in s_thing. If it's only one object, it's
     // stored as an object. If there's more, then it makes a bindlist, (a linked list of objects)
        t_pd *proxy = x->x_rcv_name->s_thing;
        t_object *obj;
        if(proxy && bindlist_class){
            if(*proxy == bindlist_class){ // if proxy is a bindlist
                // traverse the list until we find a patchable
                // object ([receive], gatom, etc) with "pd_checkobject"
                x->x_bindelem = ((t_bindlist *)proxy)->b_list;
                while(x->x_bindelem){
                	// once we find it, prep it and go from there
                    if((obj = retrieve_checkobject(x->x_bindelem->e_who))){
                        x->x_bangto = x->x_bindelem->e_who;
                        if(retrieve_prep(x, obj)) // why 'if'???
                        	return;
                    }
                    x->x_bindelem = x->x_bindelem->e_next;
                }
            }
            // it's a single object otherwise. If patchable, prep it
            else if((obj = retrieve_checkobject(proxy))){
                x->x_bangto = proxy;
                retrieve_prep(x, obj);
            }
        }
    }
//    else // not looking for receive name
//        retrieve_prep(x, &x->x_obj); // prep [retrieve] and assign x_rightout as the proxy
}

/* Point an object's outlet at whatever [retrieve]'s outlet is connected to,
   with connections of its own. Sharing [retrieve]'s connections instead (as
   this used to) shares the cords drawn for them: if the canvas redraws its
   cords while the answer comes in (an object there resizes, say), the cord
   from [retrieve] is moved over to the retrieved object and stays there.
   The object's own connections are stored by the caller, and given back by
   retrieve_giveback. An object without an outlet has nothing to lend, and a
   signal outlet carries no messages to retrieve: changing its connections
   would only rebuild the DSP graph (twice per object, on every retrieve), so
   both are left alone. */
static int retrieve_canlend(t_object *obj){
    t_outlet *objout;
    obj_starttraverseoutlet(obj, &objout, 0);
    return(objout && !obj_issignaloutlet(obj, 0));
}

static void retrieve_lend(t_retrieve *x, t_object *obj){
    t_outlet *op, *objout;
    t_object *to;
    t_inlet *ip;
    int inno;
    if(!retrieve_canlend(obj))
        return;
    t_outconnect *oc = obj_starttraverseoutlet((t_object *)x, &op, 0);
    obj_starttraverseoutlet(obj, &objout, 0);
    objout->o_connections = 0;
    while(oc){
        oc = obj_nexttraverseoutlet(oc, &to, &ip, &inno);
        obj_connect(obj, 0, to, inno);
    }
}

// take down what retrieve_lend made and give the object its own back
static void retrieve_giveback(t_object *obj, t_outconnect *own){
    t_outlet *objout;
    t_object *to;
    t_inlet *ip;
    int inno;
    t_outconnect *oc;
    if(!retrieve_canlend(obj)) // nothing was lent (see retrieve_lend)
        return;
    while((oc = obj_starttraverseoutlet(obj, &objout, 0))){
        obj_nexttraverseoutlet(oc, &to, &ip, &inno);
        obj_disconnect(obj, 0, to, inno);
        if(obj_starttraverseoutlet(obj, &objout, 0) == oc) // can't happen, but don't hang
            break;
    }
    objout->o_connections = own;
}

/* retrieve_next is where all the work gets done. For reference, obj_starttraverseoutlet
   takes three arguments: an object, an address to store a pointer to one of its outlets
   and the outlet number, indexed from zero (left to right). It looks in the object, finds
   the n'th outlet, puts a pointer to it in the address, and then returns that outlet's
   connection list. */
static int retrieve_next(t_retrieve *x){
    // check if buffers are available and bail if they aren't
	if(!(x->x_retrieved && x->x_retrievecons))
		return(0);
	// pointers to traverse the buffers
	t_object **retrievedp = x->x_retrieved;
	t_outconnect **retrieveconsp = x->x_retrievecons;
	t_object *gr; // the retrieved object below the [receive].
	int inno;
	t_outlet *goutp; // the outlet pointer; we need to get its connections
nextremote:
	// if the receiver is not a [receive] (gatom, nbx, etc.), retrieve directly from it
    if(x->x_receiver && !(x->x_nonreceive) &&
    (x->x_receiver->te_g.g_pd->c_name != gensym("receive") &&
    x->x_receiver->te_g.g_pd->c_name != gensym("receiver"))){
		*retrievedp = x->x_receiver; // store the receiver itself in the object buffer
		*retrieveconsp = obj_starttraverseoutlet(x->x_receiver, &goutp, 0);
		/* now, we connect the outlet to whatever [retrieve] is connected to
		   instead (see retrieve_lend) */
		retrieve_lend(x, x->x_receiver);
		/* this is a flag to let us know we've done the work for this object; otherwise
		   we get an infinite loop in the retrieve_bang, routine. There's
		   probably a better way to do this... */
		x->x_nonreceive = 1;
		/* return the number of objects we stored */
		return(1);
	}
	// otherwise, it's a [receive] or [retrieve]'s right outlet
    else if(x->x_to_retr_con){
    	// traverse the connections one by one
		while(x->x_to_retr_con){
			/* get the next connection and the object it's connected to. This
			   will break the loop once there aren't any new connections stored,
			   and also it will signal the else if directly above to jump down
			   the next time retrieve_next is called */
			x->x_to_retr_con = outlet_nextconnection(x->x_to_retr_con, &gr, &inno);
			if(gr){ // if it's an object?
				// if not connected to 0th inlet, don't store the object or its connections
				if(inno){
//					if(x->x_rightout)
//						pd_error(x, "[retrieve]: right outlet must feed leftmost inlet");
//					else
						pd_error(x, "[retrieve]: remote receive object must feed leftmost inlet");
				}
				else{
					// store the object
					*retrievedp++ = gr;
					/* store the number of outlets (pd has to traverse the outlet list
					   in order to do this, which is costly for a lot of outlets).*/
					/* now we get the connection lists for the outlet */
                    *retrieveconsp++ = obj_starttraverseoutlet(gr, &goutp, 0);
                    /* now connect its outlet to whatever [retrieve]'s outlet is
                    connected to instead (see retrieve_lend) */
                    retrieve_lend(x, gr);
				}
			}
		}
		return(retrievedp-x->x_retrieved); // return number of objects stored
    }
    // once we've processed and done all of one [receive]'s objects, it's on to
    // the next receiver in the bindlist. Run prep again and then goto the top.
    if(x->x_bindelem){
        while((x->x_bindelem = x->x_bindelem->e_next)){
            t_object *obj;
            if((obj = retrieve_checkobject(x->x_bindelem->e_who))){
                x->x_nonreceive = 0;
                x->x_bangto = x->x_bindelem->e_who;
                retrieve_prep(x, obj);
                retrievedp = x->x_retrieved;
                retrieveconsp = x->x_retrievecons;
                goto
                    nextremote;
            }
        }
    }
    return(0);
}

// restore's all retrieved objects' connection lists to their outlets
static void retrieve_restore(t_retrieve *x, int nobs){
	t_object **retrievedp = x->x_retrieved;
	t_outconnect **retrieveconsp = x->x_retrievecons;
	t_object *gr;
	// retrieve_next returns the number of objects it's stored, so we can pass it to
	// retrieve_restore in order to tell it how many objects to restore from the buffers
	while(nobs--){
		gr = *retrievedp++; // get the object
        // restore the connection lists to outlet
        retrieve_giveback(gr, *retrieveconsp++);
	}
}

static void retrieve_bang(t_retrieve *x){
    int nobs;
    retrieve_start(x);
    while((nobs = retrieve_next(x))){
        /* we're retrieving from a receiver, then we've already rewired all of the connections
           from the retrieved objects to [retrieve]. Then we call that receiver's bang method,
           and the output from the retrieved objects comes through [retrieve] instead. */
        if(x->x_receiver)
            retrieve_ask(x->x_bangto);
//        else outlet_bang(x->x_rightout);
        retrieve_restore(x, nobs);
    }
}

// set receive name
static void retrieve_set(t_retrieve *x, t_symbol *s){
    if(s && s != &s_)
        x->x_rcv_name = s;
}

static void retrieve_free(t_retrieve *x){
    if(x->x_retrieved)
        freebytes(x->x_retrieved, x->x_maxcons * sizeof(*x->x_retrieved));
    if(x->x_retrievecons)
    	freebytes(x->x_retrievecons, x->x_maxcons * sizeof(*x->x_retrievecons));
}

static void *retrieve_new(t_symbol *s){
    t_retrieve *x = (t_retrieve *)pd_new(retrieve_class);
    x->x_maxcons = 0;
    outlet_new((t_object *)x, &s_anything);
    x->x_rcv_name = (s && s != &s_) ? s : 0;
// unlike grab, no right outlet option
//    x->x_rightout = 0;
//    x->x_rightout = outlet_new((t_object *)x, &s_anything);
    return(x);
}

void retrieve_setup(void){
    t_symbol *s = gensym("retrieve");
    retrieve_class = class_new(s, (t_newmethod)retrieve_new, (t_method)retrieve_free,
        sizeof(t_retrieve), 0, A_DEFSYMBOL, 0);
    class_addbang(retrieve_class, retrieve_bang);
    class_addmethod(retrieve_class, (t_method)retrieve_set, gensym("set"), A_SYMBOL, 0);
    if(!bindlist_class){
        t_class *c = retrieve_class;
        pd_bind(&retrieve_class, s);
        pd_bind(&c, s);
        if(!s->s_thing || !(bindlist_class = *s->s_thing)
	    || bindlist_class->c_name != gensym("bindlist"))
            pd_error(retrieve_class, "retrieve: failure to initialize retrieve name");
	pd_unbind(&c, s);
	pd_unbind(&retrieve_class, s);
    }
}
