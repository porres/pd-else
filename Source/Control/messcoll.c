// based on cyclone/coll Copyright (c) 2002-2005 krzYszcz and others.
#include <stdio.h>
#include <string.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "elsefile.h"

#include <pthread.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

// LATER make sure that ``reentrancy protection hack'' is really working...

enum{COLL_HEADRESET, COLL_HEADNEXT, COLL_HEADPREV,  // distinction not used, currently
    COLL_HEADDELETED};

typedef struct _collelem{
    int                e_hasnumkey;
    int                e_numkey;
    t_symbol          *e_symkey;
    struct _collelem  *e_prev;
    struct _collelem  *e_next;
    int                e_size;
    t_atom            *e_data;
}t_messcollelem;

typedef struct _collcommon{
    t_pd            c_pd;
    struct _coll   *c_refs;         // used in read-banging and dirty flag handling
    int             c_increation;
    int             c_volatile;
    int             c_selfmodified; // ????????
    int             c_fileoninit;   // file loaded on initialization
    int             c_entered;      // a counter, LATER rethink
    int             c_keepflag;     // common field (CHECKED in 'TEXT' files)
    t_symbol       *c_filename;     // CHECKED common for all, read and write
    t_canvas       *c_lastcanvas;
    t_elsefile     *c_filehandle;
    t_messcollelem *c_first;
    t_messcollelem *c_last;
    t_messcollelem *c_head;
    int             c_headstate;
}t_messcollcommon;

typedef struct _coll_q{ // element in a linked list of stored messages waiting to be sent out
    struct _coll_q *q_next;  // next in list
    char           *q_s;     // the string
}t_messcoll_q;

typedef struct _coll{
    t_object            x_obj;
    t_canvas           *x_canvas;
    t_symbol           *x_name;
    t_messcollcommon   *x_common;
    t_elsefile         *x_filehandle;
    t_outlet           *x_keyout;
    t_outlet           *x_info_out;
    t_symbol           *x_bindsym;
    int                 x_is_opened;
    int                 x_threaded;
    int                 x_initread; // if we're reading a file for the first time
    int                 x_filebang; // if we're expecting to bang out 3rd outlet
    struct _coll       *x_next;
// for thread-unsafe file i/o operations added by Ivica Ico Bukvic
    t_clock            *x_clock;
    pthread_t           unsafe_t;
    pthread_mutex_t     unsafe_mutex;
    pthread_cond_t      unsafe_cond;
    t_symbol           *x_s;
    t_int               x_keep;
    t_int               unsafe;
    t_int               init;       // used to make sure that the secondary thread is ready to go
    t_int               threaded;   // used to decide whether this should be a threaded instance
    t_messcoll_q       *x_q;        // a list of error messages to be processed
}t_messcoll;

typedef struct _msg{
	int m_flag;
	int m_line;
}t_msg;

typedef struct _threadedFunctionParams{
	t_messcoll *x;
}t_threadedFunctionParams;

static t_class *messcoll_class;
static t_class *messcollcommon_class;

/// Porres: de-louding in a lazy way
void coll_messarg(t_pd *x, t_symbol *s){
    pd_error(x, "[messcoll]: bad arguments for message \"%s\"", s->s_name);
}

int coll_checkint(t_pd *x, t_float f, int *valuep, t_symbol *mess){
    if((*valuep = (int)f) == f)
        return (1);
    else{
        if(mess == &s_float)
            pd_error(x, "[messcoll]: doesn't understand \"noninteger float\"");
        else if(mess)
            pd_error(x, "[messcoll]: \"noninteger float\" argument invalid for message \"%s\"",
                     mess->s_name);
        return (0);
    }
}
///////////

static void coll_q_free(t_messcoll *x){
    t_messcoll_q *q2;
    while(x->x_q){
		q2 = x->x_q->q_next;
		t_freebytes(x->x_q->q_s, strlen(x->x_q->q_s) + 1);
		t_freebytes(x->x_q, sizeof(*x->x_q));
		x->x_q = q2;
    }
	x->x_q = NULL;
}

static void coll_q_post(t_messcoll_q *q){
    t_messcoll_q *qtmp;
    for(qtmp = q; qtmp; qtmp = qtmp->q_next)
		post("%s", qtmp->q_s);
}

static void coll_q_enqueue(t_messcoll *x, const char *s){
	t_messcoll_q *q, *q2 = NULL;
	q = (t_messcoll_q *)(getbytes(sizeof(*q)));
	q->q_next = NULL;
	q->q_s = (char *)getbytes(strlen(s) + 1);
	strcpy(q->q_s, s);
	if(!x->x_q)
		x->x_q = q;
	else{
		q2 = x->x_q;
		while(q2->q_next)
			q2 = q2->q_next;
		q2->q_next = q;
	}
}

static void coll_enqueue_threaded_msgs(t_messcoll *x, t_msg *m){
	char s[MAXPDSTRING];
	if(m->m_flag & 1)
		coll_q_enqueue(x, s);
	if(m->m_flag & 2){
        sprintf(s, "[messcoll]: can't find file '%s'", x->x_s->s_name);
        coll_q_enqueue(x, s);
	}
	if(m->m_flag & 4){
		//sprintf(s, "[messcoll]: finished reading %d lines from text file '%s'", m->m_line, x->x_s->s_name);
		//coll_q_enqueue(x, s);
	}
	if(m->m_flag & 8) {
		sprintf(s, "[messcoll]: error in line %d of text file '%s'", m->m_line, x->x_s->s_name);
		coll_q_enqueue(x, s);
	}
	if(m->m_flag & 16){
		sprintf(s, "[messcoll]: can't find file '%s'", x->x_s->s_name);
		coll_q_enqueue(x, s);
	}
	if(m->m_flag & 32) {
		sprintf(s, "[messcoll]: error writing text file '%s'", x->x_s->s_name);
		coll_q_enqueue(x, s);
	}
}

static void coll_tick(t_messcoll *x){
	if(x->x_q){
		coll_q_post(x->x_q);
		coll_q_free(x);
	};
    if(x->x_filebang && x->x_initread){
        x->x_initread = 0;
        outlet_anything(x->x_info_out, gensym("doneread"), 0, NULL);
        x->x_filebang = 0;
    };
}

static t_messcollelem *collelem_new(int ac, t_atom *av, int *np, t_symbol *s){
    t_messcollelem *ep = (t_messcollelem *)getbytes(sizeof(*ep));
    if((ep->e_hasnumkey = (np != 0)))
        ep->e_numkey = *np;
    ep->e_symkey = s;
    ep->e_prev = ep->e_next = 0;
    if((ep->e_size = ac)){
        t_atom *ap = getbytes(ac * sizeof(*ap));
        ep->e_data = ap;
        if(av) while(ac--)
            *ap++ = *av++;
        else while(ac--){
            SETFLOAT(ap, 0);
            ap++;
        }
    }
    else
        ep->e_data = 0;
    return(ep);
}

static void collelem_free(t_messcollelem *ep){
    if(ep->e_data)
        freebytes(ep->e_data, ep->e_size * sizeof(*ep->e_data));
    freebytes(ep, sizeof(*ep));
}

static t_messcollelem *collcommon_numkey(t_messcollcommon *cc, int numkey){
    t_messcollelem *ep;
    for(ep = cc->c_first; ep; ep = ep->e_next)
	if(ep->e_hasnumkey && ep->e_numkey == numkey)
	    return(ep);
    return(0);
}

static t_messcollelem *collcommon_symkey(t_messcollcommon *cc, t_symbol *symkey){
    t_messcollelem *ep;
    for(ep = cc->c_first; ep; ep = ep->e_next)
	if(ep->e_symkey == symkey)
        return(ep);
    return(0);
}

static void collcommon_dirty(t_messcollcommon *cc){
    if(cc->c_keepflag){
        t_messcoll *x;
        for(x = cc->c_refs; x; x = x->x_next)
            if(x->x_canvas)
                canvas_dirty(x->x_canvas, 1);
    }
}

static void collcommon_modified(t_messcollcommon *cc, int relinked){
    if(cc->c_increation)
        return;
    if(relinked)
        cc->c_volatile = 1;
    collcommon_dirty(cc);
}

// atomic collcommon modifiers: clearall, delete, replace, putafter
static void collcommon_clearall(t_messcollcommon *cc){
    if(cc->c_first){
        t_messcollelem *ep1 = cc->c_first, *ep2;
        do{
            ep2 = ep1->e_next;
            collelem_free(ep1);
        }
        while((ep1 = ep2));
            cc->c_first = cc->c_last = 0;
        cc->c_head = 0;
        cc->c_headstate = COLL_HEADRESET;
        collcommon_modified(cc, 1);
    }
}

static void collcommon_delete(t_messcollcommon *cc, t_messcollelem *ep){
    if(ep->e_prev)
        ep->e_prev->e_next = ep->e_next;
    else
        cc->c_first = ep->e_next;
    if(ep->e_next)
        ep->e_next->e_prev = ep->e_prev;
    else
        cc->c_last = ep->e_prev;
    if(cc->c_head == ep){
        cc->c_head = ep->e_next;  // asymmetric, LATER rethink
        cc->c_headstate = COLL_HEADDELETED;
    }
    collelem_free(ep);
    collcommon_modified(cc, 1);
}

static void collcommon_replace(t_messcollcommon *cc, t_messcollelem *ep, int ac, t_atom *av, int *np, t_symbol *s){
    if((ep->e_hasnumkey = (np != 0)))
	ep->e_numkey = *np;
    ep->e_symkey = s;
    if(ac){
        int i = ac;
        t_atom *ap;
        if(ep->e_data){
            if(ep->e_size != ac)
                ap = resizebytes(ep->e_data, ep->e_size * sizeof(*ap), ac * sizeof(*ap));
            else ap = ep->e_data;
        }
        else
            ap = getbytes(ac * sizeof(*ap));
        ep->e_data = ap;
        if(av) while(i --)
            *ap++ = *av++;
        else while(i --){
            SETFLOAT(ap, 0);
            ap++;
        }
    }
    else{
        if(ep->e_data)
            freebytes(ep->e_data, ep->e_size * sizeof(*ep->e_data));
        ep->e_data = 0;
    }
    ep->e_size = ac;
    collcommon_modified(cc, 0);
}

static void collcommon_putafter(t_messcollcommon *cc, t_messcollelem *ep, t_messcollelem *prev){
    if(prev){
        ep->e_prev = prev;
        if((ep->e_next = prev->e_next))
            ep->e_next->e_prev = ep;
        else
            cc->c_last = ep;
        prev->e_next = ep;
    }
    else if(cc->c_first || cc->c_last)
        bug("collcommon_putafter");
    else
        cc->c_first = cc->c_last = ep;
    collcommon_modified(cc, 1);
}

static t_messcollelem *collcommon_tonumkey(t_messcollcommon *cc, int numkey, int ac, t_atom *av, int replace){
    // collcommon_numkey checks existence of key
    t_messcollelem *old = collcommon_numkey(cc, numkey), *new;
    if(old && replace)
        collcommon_replace(cc, new = old, ac, av, &numkey, 0);
    else{
        new = collelem_new(ac, av, &numkey, 0);
        if(old){
            do
                if(old->e_hasnumkey)
                    //CHECKED incremented up to the last one; incompatible:
                    //  elements with numkey == 0 not incremented (a bug?)
                    old->e_numkey++;
                while((old = old->e_next));
        };
        // CHECKED negative numkey always put before the last element,
        // zero numkey always becomes the new head
        t_messcollelem *closest = 0, *ep;
        for(ep = cc->c_first; ep; ep = ep->e_next)
            closest = ep;
        collcommon_putafter(cc, new, closest);
	}
    return(new);
}

static t_messcollelem *collcommon_tosymkey(t_messcollcommon *cc, t_symbol *symkey, int ac, t_atom *av, int replace){
    t_messcollelem *old = collcommon_symkey(cc, symkey), *new;
    if(old && replace)
        collcommon_replace(cc, new = old, ac, av, 0, symkey);
    else
        collcommon_putafter(cc, new = collelem_new(ac, av, 0, symkey), cc->c_last);
    return(new);
}

// get data from editor window or file
static int collcommon_fromatoms(t_messcollcommon *cc, int ac, t_atom *av){
    int hasnumkey = 0, numkey;
    t_symbol *symkey = 0;
    int size = 0;
    t_atom *data = 0;
    int nlines = 0;
    cc->c_increation = 1;
    collcommon_clearall(cc);
    while(ac--){
        if(data){
            if(av->a_type == A_SEMI){
                t_messcollelem *ep = collelem_new(size, data, hasnumkey ? &numkey : 0, symkey);
                collcommon_putafter(cc, ep, cc->c_last);
                hasnumkey = 0;
                symkey = 0;
                data = 0;
                nlines++;
            } // let's try to ignore this and see what happens ????
/*            if(av->a_type == A_COMMA){ // CHECKED rejecting a comma
                collcommon_clearall(cc);  // LATER rethink
                cc->c_increation = 0;
                return (-nlines);
            }*/
            else
                size++;
        }
        else if(av->a_type == A_COMMA){
            size = 0;
            data = av + 1;
        }
        else if(av->a_type == A_SYMBOL)
            symkey = av->a_w.w_symbol;
        else if(av->a_type == A_FLOAT &&
            coll_checkint(0, av->a_w.w_float, &numkey, 0))
	    hasnumkey = 1;
        else{
            post("[messcoll]: bad atom");
            collcommon_clearall(cc);  // LATER rethink
            cc->c_increation = 0;
            return (-nlines);
        }
        av++;
    }
    if(data){
        post("[messcoll]: incomplete");
        collcommon_clearall(cc);  // LATER rethink
        cc->c_increation = 0;
        return (-nlines);
    }
    cc->c_increation = 0;
    return(nlines);
}

static t_msg *collcommon_doread(t_messcollcommon *cc, t_symbol *fn, t_canvas *cv, int threaded){
    t_binbuf *bb;
	t_msg *m = (t_msg *)(getbytes(sizeof(*m)));
	m->m_flag = 0;
	m->m_line = 0;
    if(!fn && !(fn = cc->c_filename))  // !fn: 'readagain'
        return(m);
    char buf[MAXPDSTRING];
    char *bufptr;
    int fd = canvas_open(cv, fn->s_name, "", buf, &bufptr, MAXPDSTRING, 1);
    if(fd > 0){
        buf[strlen(buf)]='/';
        sys_close(fd);
    }
    else{
        post("[messcoll] file '%s' not found", fn->s_name);
        return(m);
    }
    if(!cc->c_refs){
		// loading during object creation --
        //  avoid binbuf_read()'s complaints, LATER rethink */
		FILE *fp;
		char fname[MAXPDSTRING];
		sys_bashfilename(buf, fname);
		if(!(fp = fopen(fname, "r"))){
			m->m_flag |= 0x01;
			return(m);
		};
		fclose(fp);
    }
    bb = binbuf_new();
    if(binbuf_read(bb, buf, "", 0)){
		m->m_flag |= 0x02;
		if(!threaded)
			post("[messcoll]: can't find file '%s'", fn->s_name);
	}
    else if(!binbuf_read(bb, buf, "", 0)){
        int nlines = collcommon_fromatoms(cc, binbuf_getnatom(bb), binbuf_getvec(bb));
		if(nlines > 0){
			t_messcoll *x;
			// LATER consider making this more robust
            // now taken care of by coll_read for obj specificity
            // leaving here so i remember how to do this o/wise - DK
            if(1){ // COLL_ALLBANG ???????
                for(x = cc->c_refs; x; x = x->x_next)
                    outlet_anything(x->x_info_out, gensym("doneread"), 0, NULL);
            };
			cc->c_lastcanvas = cv;
			cc->c_filename = fn;
			m->m_flag |= 0x04;
			m->m_line = nlines;
			/* if(!threaded)
				post("[messcoll]: finished reading %d lines from text file '%s'",
					nlines, fn->s_name); */
		}
		else if(nlines < 0){
			m->m_flag |= 0x08;
			m->m_line = 1 - nlines;
			if(!threaded)
			    post("[messcoll]: error in line %d of text file '%s'", 1 - nlines, fn->s_name);
		}
		else{
			m->m_flag |= 0x16;
			if(!threaded)
				post("[messcoll]: can't find file '%s'", fn->s_name);
		}
		if(cc->c_refs)
			collcommon_modified(cc, 1);
	}
    binbuf_free(bb);
	return(m);
}

static void collcommon_readhook(t_pd *z, t_symbol *fn, int ac, t_atom *av){
    (void)ac;
    (void)av;
    collcommon_doread((t_messcollcommon *)z, fn, ((t_messcoll *)z)->x_canvas, 0);
}

static void collcommon_tobinbuf(t_messcollcommon *cc, t_binbuf *bb){
    t_messcollelem *ep;
    t_atom at[3];
    for(ep = cc->c_first; ep; ep = ep->e_next){
        t_atom *ap = at;
        int cnt = 1;
        if(ep->e_hasnumkey){
            SETFLOAT(ap, ep->e_numkey);
            ap++; cnt++;
        }
        if(ep->e_symkey){
            SETSYMBOL(ap, ep->e_symkey);
            ap++; cnt++;
        }
        SETCOMMA(ap);
        binbuf_add(bb, cnt, at);
        binbuf_add(bb, ep->e_size, ep->e_data);
        binbuf_addsemi(bb);
    }
}

static t_msg *collcommon_dowrite(t_messcollcommon *cc, t_symbol *fn, t_canvas *cv, int threaded){
    t_binbuf *bb;
	t_msg *m = (t_msg *)(getbytes(sizeof(*m)));
	m->m_flag = 0;
	m->m_line = 0;
    char buf[MAXPDSTRING];
    if(!fn && !(fn = cc->c_filename)) //!fn: 'writeagain'
		return(0);
    if(cv || (cv = cc->c_lastcanvas))  // !cv: 'write' w/o arg, 'writeagain'
        canvas_makefilename(cv, fn->s_name, buf, MAXPDSTRING);
    else{
    	strncpy(buf, fn->s_name, MAXPDSTRING);
    	buf[MAXPDSTRING-1] = 0;
    }
    bb = binbuf_new();
    collcommon_tobinbuf(cc, bb);
    if(binbuf_write(bb, buf, "", 0)) {
		m->m_flag |= 0x32;
		if(!threaded)
			post("[messcoll]: error writing text file '%s'", fn->s_name);
	}
    else if(!binbuf_write(bb, buf, "", 0)){
        cc->c_lastcanvas = cv;
        cc->c_filename = fn;
    }
    binbuf_free(bb);
	return(m);
}

static void collcommon_writehook(t_pd *z, t_symbol *fn, int ac, t_atom *av){
    (void)ac;
    (void)av;
    collcommon_dowrite((t_messcollcommon *)z, fn, 0, 0);
}

static void coll_keephook(t_pd *z, t_binbuf *bb, t_symbol *bindsym){
    t_messcoll *x = (t_messcoll *)z;
    t_messcollcommon *cc = x->x_common;
    cc->c_keepflag = x->x_keep;
    if(cc->c_keepflag){
        t_messcollelem *ep;
        t_atom at[6];
        SETSYMBOL(at, bindsym);
        for(ep = cc->c_first; ep; ep = ep->e_next){
            t_atom *ap = at + 1;
            int cnt = 3;
            SETSYMBOL(ap, gensym("store"));
            ap++;
            if(ep->e_symkey)
                SETSYMBOL(ap, ep->e_symkey);
            else
                SETFLOAT(ap, ep->e_numkey);
            binbuf_add(bb, cnt, at);
            for(int i = 0; i < ep->e_size; i++){
                if(ep->e_data[i].a_type == A_COMMA){
                    t_atom a;
                    SETSYMBOL(&a, gensym(","));
                    binbuf_add(bb, 1, &a);
                }
                else
                    binbuf_add(bb, 1, &ep->e_data[i]);
            }
            binbuf_addsemi(bb);
        }
        binbuf_addv(bb, "ssi;", bindsym, gensym("keep"), 1);
    }
    obj_saveformat((t_object *)x, bb);
}

static void collcommon_editorhook(t_pd *z, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_messcollcommon *cc = (t_messcollcommon *)z;
    int nlines = collcommon_fromatoms(cc, ac, av);
    if(nlines < 0){
        post("[messcoll]: editing error in line %d", 1 - nlines);
        return;
    }
    collcommon_dirty(cc);
}

static void collcommon_free(t_messcollcommon *cc){
    t_messcollelem *ep1, *ep2 = cc->c_first;
    while((ep1 = ep2)){
        ep2 = ep1->e_next;
        collelem_free(ep1);
    }
}

static void coll_unbind(t_messcoll *x, int flag){
    t_messcollcommon *cc = x->x_common;
    t_messcoll *prev, *next;
    if((prev = cc->c_refs) == x){
        if(!(cc->c_refs = x->x_next)){
            elsefile_free(cc->c_filehandle);
            collcommon_free(cc);
            if(x->x_name){
                pd_unbind(&cc->c_pd, x->x_name);
                if(flag)
                    post("[messcoll]: last \"%s\" named object deleted - data lost", x->x_name->s_name);
            }
            pd_free(&cc->c_pd);
        }
    }
    else if(prev){
        while((next = prev->x_next)){
            if(next == x){
                prev->x_next = next->x_next;
                break;
            }
	    prev = next;
        }
    }
    x->x_common = 0;
    x->x_name = 0;
    x->x_next = 0;
}

static void *collcommon_init(void){
    t_messcollcommon *cc = (t_messcollcommon *)pd_new(messcollcommon_class);
    cc->c_keepflag = 0;
    cc->c_first = 0;
    cc->c_last = 0;
    cc->c_head = 0;
    cc->c_refs = 0;
    cc->c_increation = 0;
    cc->c_fileoninit = 0;
    cc->c_headstate = COLL_HEADRESET;
    return(cc);
}

static void coll_dooutput(t_messcoll *x, int ac, t_atom *av){
    if(!ac)
        return;
    else if(ac == 1){
        if(av->a_type == A_FLOAT)
            outlet_float(((t_object *)x)->ob_outlet, av->a_w.w_float);
        else if(av->a_type == A_SYMBOL)
            outlet_anything(((t_object *)x)->ob_outlet, av->a_w.w_symbol, 0, NULL);
    }
    else{
        int i = 0, start = 0;
        while(i <= ac){
            if(i == ac || av[i].a_type == A_COMMA){
                int n = i - start;
                if(n > 0){
                    t_atom *chunk = av + start;
                    if(n == 1){
                        if(chunk->a_type == A_FLOAT)
                            outlet_float(((t_object *)x)->ob_outlet, chunk->a_w.w_float);
                        else if(chunk->a_type == A_SYMBOL)
                            outlet_anything(((t_object *)x)->ob_outlet, chunk->a_w.w_symbol, 0, NULL);
                    }
                    else{
                        if(chunk->a_type == A_FLOAT)
                            outlet_list(((t_object *)x)->ob_outlet, &s_list, n, chunk);
                        else if(chunk->a_type == A_SYMBOL)
                            outlet_anything(((t_object *)x)->ob_outlet, chunk->a_w.w_symbol, n-1, chunk+1);
                    }
                }
                start = i + 1;
            }
            i++;
        }
    }
}

static void coll_keyoutput(t_messcoll *x, t_messcollelem *ep){
    t_messcollcommon *cc = x->x_common;
    if(!cc->c_entered++)
        cc->c_selfmodified = 0;
    cc->c_volatile = 0;
    if(ep->e_hasnumkey)
        outlet_float(x->x_keyout, ep->e_numkey);
    else if(ep->e_symkey)
        outlet_symbol(x->x_keyout, ep->e_symkey);
    else
        outlet_float(x->x_keyout, 0);
    if(cc->c_volatile)
        cc->c_selfmodified = 1;
    cc->c_entered--;
}

static t_messcollelem *coll_findkey(t_messcoll *x, t_atom *key, t_symbol *mess){
    t_messcollcommon *cc = x->x_common;
    t_messcollelem *ep = 0;
    if(key->a_type == A_FLOAT){
        int numkey;
        if(coll_checkint((t_pd *)x, key->a_w.w_float, &numkey, mess))
            ep = collcommon_numkey(cc, numkey);
        else
            mess = 0;
    }
    else if(key->a_type == A_SYMBOL)
        ep = collcommon_symkey(cc, key->a_w.w_symbol);
    else if(mess){
        coll_messarg((t_pd *)x, mess);
        mess = 0;
    }
    if(!ep && mess)
        pd_error((t_pd *)x, "[messcoll]: no such key");
    return(ep);
}

static t_messcollelem *coll_tokey(t_messcoll *x, t_atom *key, int ac, t_atom *av, int replace, t_symbol *mess){
    t_messcollcommon *cc = x->x_common;
    t_messcollelem *ep = 0;
    
    for(int i = 0; i < ac; i++){
        if(av[i].a_type == A_SYMBOL){
            if(av[i].a_w.w_symbol == gensym(","))
                av[i].a_type = A_COMMA;
        }
    }

    if(key->a_type == A_FLOAT){
        int numkey;
        if(coll_checkint((t_pd *)x, key->a_w.w_float, &numkey, mess))
            ep = collcommon_tonumkey(cc, numkey, ac, av, replace);
    }
    else if(key->a_type == A_SYMBOL)
        ep = collcommon_tosymkey(cc, key->a_w.w_symbol, ac, av, replace);
    else if(mess)
        coll_messarg((t_pd *)x, mess);
    return(ep);
}

static void coll_delete_window_contents(unsigned long handle){
    char buf[256];
    snprintf(buf, sizeof(buf),
        "if {[winfo exists .%lx]} { .%lx.text delete 1.0 end }",
        handle, handle);
    pdgui_vmess(buf, NULL);
}

static void coll_do_update(t_messcoll *x){
    if(x->x_is_opened){
        t_messcollcommon *cc = x->x_common;
        t_binbuf *bb = binbuf_new();
        int natoms, newline;
        t_atom *ap;
        char buf[MAXPDSTRING];
        collcommon_tobinbuf(cc, bb);
        natoms = binbuf_getnatom(bb);
        ap = binbuf_getvec(bb);
        newline = 1;
        coll_delete_window_contents((unsigned long)cc->c_filehandle);
        while(natoms--){
            char *ptr = buf;
            if(ap->a_type != A_SEMI && ap->a_type != A_COMMA && !newline)
                *ptr++ = ' ';
            atom_string(ap, ptr, MAXPDSTRING);
            if(ap->a_type == A_SEMI){
                strcat(buf, "\n");
                newline = 1;
            }
            else
                newline = 0;
            else_editor_append(cc->c_filehandle, buf);
            ap++;
        }
        else_editor_setdirty(cc->c_filehandle, 0);
        binbuf_free(bb);
    }
}

static void coll_show_window(unsigned long handle){
    char buf[256];
    snprintf(buf, sizeof(buf),
        "wm deiconify .%lx; raise .%lx; focus .%lx.text",
        handle, handle, handle);
    pdgui_vmess(buf, NULL);
}

// open editor window and display items
static void coll_do_open(t_messcoll *x){
    t_messcollcommon *cc = x->x_common;
    if(x->x_is_opened)
        coll_show_window((unsigned long)cc->c_filehandle);
    else{
        char buf[MAXPDSTRING];
        char *name = (char *)(x->x_name ? x->x_name->s_name : "Untitled");
        elsefile_editor_open(cc->c_filehandle, name, "messcoll");
        t_binbuf *bb = binbuf_new();
        collcommon_tobinbuf(cc, bb);
        int natoms = binbuf_getnatom(bb);
        t_atom *ap = binbuf_getvec(bb);
        int newline = 1;
        while(natoms--){
            char *ptr = buf;
            if(ap->a_type != A_SEMI && ap->a_type != A_COMMA && !newline)
                *ptr++ = ' ';
            atom_string(ap, ptr, MAXPDSTRING);
            if(ap->a_type == A_SEMI){
                strcat(buf, "\n");
                newline = 1;
            }
            else
                newline = 0;
            else_editor_append(cc->c_filehandle, buf);
            ap++;
        }
        else_editor_setdirty(cc->c_filehandle, 0);
        binbuf_free(bb);
        x->x_is_opened = 1;
    }
}

static void coll_is_opened(t_messcoll *x, t_floatarg f, t_floatarg open){
    x->x_is_opened = (int)f;
    open ? coll_do_open(x) : coll_do_update(x);
}

static void check_open(t_messcoll *x, int open){
    char buf[512];
    snprintf(buf, sizeof(buf),
        "if {[winfo exists .%lx]} {"
        "pdsend \"%s _is_opened 1 %d\""
        "} else {"
        "pdsend \"%s _is_opened 0 %d\""
        "}",
        (unsigned long)x->x_common->c_filehandle,
        x->x_bindsym->s_name, open,
        x->x_bindsym->s_name, open);
    pdgui_vmess(buf, NULL);
}

static void coll_update(t_messcoll *x){
    check_open(x, 0);
}

// methods -------------------------------------------------------------------------------------
static void coll_wclose(t_messcoll *x){
    else_editor_close(x->x_common->c_filehandle, 1); // ask to save contents if edited
}

static void coll_open(t_messcoll *x){
    check_open(x, 1);
}

static void coll_click(t_messcoll *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)ctrl;
    if(shift)
        elsefile_panel_click_open(x->x_common->c_filehandle);
    else if(alt)
        elsefile_panel_save(x->x_common->c_filehandle, 0, 0);
    else
        check_open(x, 1);
}

static void coll_delete(t_messcoll *x, t_symbol *s, int ac, t_atom *av){
    if(!ac)
        return;
    else{
        t_messcollelem *ep;
        if((ep = coll_findkey(x, av, s))){
            collcommon_delete(x->x_common, ep);
            coll_update(x);
        }
        else
            post("[messcoll]: delete address not found");
    }
}

static void coll_clear(t_messcoll *x){
    t_messcollcommon *cc = x->x_common;
    collcommon_clearall(cc);
    coll_delete_window_contents((unsigned long)cc->c_filehandle);
}

/* CHECKED traversal direction change is consistent with the general rule:
   'next' always outputs e_next of a previous output, and 'prev' always
   outputs e_prev, whether preceded by 'prev', or by 'next'.  This is
   currently implemented by pre-updating of the head (which is inhibited
   if there was no previous output, i.e. after 'goto', 'end', or collection
   initialization).  CHECKME again. */
static void coll_bang(t_messcoll *x){
    t_messcollcommon *cc = x->x_common;
    if(cc->c_headstate != COLL_HEADRESET && cc->c_headstate != COLL_HEADDELETED){// asymmetric, LATER rethink
		if(cc->c_head)
			cc->c_head = cc->c_head->e_next;
		if(!cc->c_head && !(cc->c_head = cc->c_first))  /* CHECKED wrapping */
			return;
    }
    else if(!cc->c_head && !(cc->c_head = cc->c_first))
		return;
    cc->c_headstate = COLL_HEADNEXT;
    coll_keyoutput(x, cc->c_head);
    if(cc->c_head)
        coll_dooutput(x, cc->c_head->e_size, cc->c_head->e_data);
    else if(!cc->c_selfmodified)
		bug("messcoll_bang");  // LATER rethink
}

static void coll_float(t_messcoll *x, t_float f){
    int numkey;
    if(coll_checkint((t_pd *)x, f, &numkey, &s_float)){
        t_messcollelem *ep = collcommon_numkey(x->x_common, numkey);
        if(ep){
            coll_keyoutput(x, ep);
            coll_dooutput(x, ep->e_size, ep->e_data);
        }
        else
            post("[messcoll]: address \"%i\" not found", (int)f);
    }
    else
        post("[messcoll]: address \"%f\" not an integer", f);
}

static void coll_symbol(t_messcoll *x, t_symbol *s){
    t_messcollelem *ep = collcommon_symkey(x->x_common, s);
    if(ep){
        coll_keyoutput(x, ep);
        coll_dooutput(x, ep->e_size, ep->e_data);
    }
    else
        post("[messcoll]: address \"%s\" not found", s->s_name);
}

static void coll_store(t_messcoll *x, t_symbol *s, int ac, t_atom *av){
    if(!ac)
        return;
    if(ac >= 2){
        coll_tokey(x, av, ac-1, av+1, 1, s);
        coll_update(x);
    }
    else
        pd_error(x, "[messcoll]: bad arguments for 'store'");
}

static void coll_list(t_messcoll *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        coll_bang(x);
    if(ac == 1){
        if(av->a_type == A_FLOAT)
            coll_float(x, atom_getfloat(av));
        else if(av->a_type == A_SYMBOL)
            coll_symbol(x, atom_getsymbol(av));
    }
    else
        coll_store(x, gensym("store"), ac, av);
}

static void coll_prev(t_messcoll *x){
    t_messcollcommon *cc = x->x_common;
    if(cc->c_headstate != COLL_HEADRESET){
        if(cc->c_head)
            cc->c_head = cc->c_head->e_prev;
        if(!cc->c_head && !(cc->c_head = cc->c_last))  // CHECKED wrapping
            return;
    }
    else if(!cc->c_head && !(cc->c_head = cc->c_first))
		return;
    cc->c_headstate = COLL_HEADPREV;
    coll_keyoutput(x, cc->c_head);
    if(cc->c_head)
		coll_dooutput(x, cc->c_head->e_size, cc->c_head->e_data);
    else if(!cc->c_selfmodified)
		bug("messcoll_prev");  // LATER rethink
}

static void coll_start(t_messcoll *x){
    t_messcollcommon *cc = x->x_common;
    cc->c_head = cc->c_first;
    cc->c_headstate = COLL_HEADRESET;
}

static void coll_end(t_messcoll *x){
    t_messcollcommon *cc = x->x_common;
    cc->c_head = cc->c_last;
    cc->c_headstate = COLL_HEADRESET;
}

static void coll_goto(t_messcoll *x, t_symbol *s, int ac, t_atom *av){
    if(ac){
        t_messcollelem *ep = coll_findkey(x, av, s);
        if(ep){
            t_messcollcommon *cc = x->x_common;
            cc->c_head = ep;
            cc->c_headstate = COLL_HEADRESET;
        }
    }
    else
        coll_start(x);
}

static void coll_size(t_messcoll *x){
    t_messcollcommon *cc = x->x_common;
    t_messcollelem *ep = cc->c_first;
    int result = 0;
    while(ep){
        result++;
        ep = ep->e_next;
    }
    t_atom at[1];
    SETFLOAT(at, result);
    outlet_anything(x->x_info_out, gensym("size"), 1, at);
}

/*static void coll_check_window(t_messcoll *x, t_symbol *name){
}*/

static void coll_set(t_messcoll *x, t_symbol *name){
    if(name == &s_)
        post("[messcoll]: empty name to set to");
    t_messcollcommon *cc = (t_messcollcommon *)pd_findbyclass(name, messcollcommon_class);
    if(cc){
        coll_unbind(x, 1);
        x->x_common = cc;
        x->x_name = name;
        x->x_next = cc->c_refs;
        cc->c_refs = x;
    }
    else
        post("[messcoll]: no object named \"%s\" found to set to", name->s_name);
}

static void coll_keep(t_messcoll *x, t_float f){
    x->x_common->c_keepflag = f != 0;    
}

static void coll_read(t_messcoll *x, t_symbol *s){
	if(!x->unsafe){
		t_messcollcommon *cc = x->x_common;
		if(s && s != &s_){
			x->x_s = s;
			if(x->x_threaded){
				x->unsafe = 1;
				pthread_mutex_lock(&x->unsafe_mutex);
				pthread_cond_signal(&x->unsafe_cond);
				pthread_mutex_unlock(&x->unsafe_mutex);
			}
			else{
				t_msg * msg = collcommon_doread(cc, s, x->x_canvas, 0);
                if(msg->m_line > 0){
                    x->x_filebang = 1;
                    clock_delay(x->x_clock, 0);
                };
			}
            coll_update(x);
		}
		else
            elsefile_panel_click_open(cc->c_filehandle);
	}
}

static void coll_write(t_messcoll *x, t_symbol *s){
	if(!x->unsafe){
        t_messcollcommon *cc = x->x_common;
		if(s && s != &s_){
            x->x_s = s;
            if(x->x_threaded == 1){
                x->unsafe = 10;
                pthread_mutex_lock(&x->unsafe_mutex);
                pthread_cond_signal(&x->unsafe_cond);
                pthread_mutex_unlock(&x->unsafe_mutex);
            }
            else
                collcommon_dowrite(cc, s, x->x_canvas, 0);
		}
		else
            elsefile_panel_save(cc->c_filehandle, 0, 0);  /* CHECKED no default name */
	}
}

static void coll_dump(t_messcoll *x){
    t_messcollcommon *cc = x->x_common;
    t_messcollelem *ep;
    for(ep = cc->c_first; ep; ep = ep->e_next){
		coll_keyoutput(x, ep);
		if(cc->c_selfmodified)
			break;
		coll_dooutput(x, ep->e_size, ep->e_data);
		/* FIXME dooutput() may invalidate ep as well as keyoutput()... */
    }
}

static void *coll_threaded_fileio(void *ptr){
	t_threadedFunctionParams *rPars = (t_threadedFunctionParams*)ptr;
	t_messcoll *x = rPars->x;
	t_msg *m = NULL;
	while(x->unsafe > -1){
		pthread_mutex_lock(&x->unsafe_mutex);
		if(x->unsafe == 0)
			if(!x->init)
                x->init = 1;
        pthread_cond_wait(&x->unsafe_cond, &x->unsafe_mutex);
		if(x->unsafe == 1) { //read
			m = collcommon_doread(x->x_common, x->x_s, x->x_canvas, 1);
			if(m->m_flag)
				coll_enqueue_threaded_msgs(x, m);
            if(m->m_line > 0){
                x->x_filebang = 1;
            };
			clock_delay(x->x_clock, 0);
		}
		else if(x->unsafe == 2) { //read
			m = collcommon_doread(x->x_common, 0, x->x_canvas, 1);
			if(m->m_flag)
				coll_enqueue_threaded_msgs(x, m);
            if(m->m_line > 0){
                x->x_filebang = 1;
            };
			clock_delay(x->x_clock, 0);
		}
		else if(x->unsafe == 10) { // write
			m = collcommon_dowrite(x->x_common, x->x_s, x->x_canvas, 1);
			if(m->m_flag)
				coll_enqueue_threaded_msgs(x, m);
		}
		else if(x->unsafe == 11) { // write
			m = collcommon_dowrite(x->x_common, 0, 0, 1);
			if(m->m_flag)
				coll_enqueue_threaded_msgs(x, m);
		};
		if(m != NULL){
			t_freebytes(m, sizeof(*m));
			m = NULL;
		}
		if(x->unsafe != -1)
            x->unsafe = 0;
		pthread_mutex_unlock(&x->unsafe_mutex);
	}
	pthread_exit(0);
}

static void coll_dothread(t_messcoll *x, int create){
    if(create){
        t_threadedFunctionParams rPars;
        x->unsafe = 0;
        rPars.x = x;
        pthread_mutex_init(&x->unsafe_mutex, NULL);
        pthread_cond_init(&x->unsafe_cond, NULL);
        pthread_create( &x->unsafe_t, NULL, (void *)&coll_threaded_fileio, (void *) &rPars);
        while(!x->init)
            sched_yield();
    }
    else{
        x->unsafe = -1;
        pthread_mutex_lock(&x->unsafe_mutex);
		pthread_cond_signal(&x->unsafe_cond);
		pthread_mutex_unlock(&x->unsafe_mutex);
		pthread_join(x->unsafe_t, NULL);
		pthread_mutex_destroy(&x->unsafe_mutex);
		if(x->x_q)
			coll_q_free(x);
        x->unsafe = 0;
    };
}

static void coll_threaded(t_messcoll *x, t_float f){
    int th = f == 0 ? 0 : 1;
    if(th != x->x_threaded)
        coll_dothread(x, th);
    x->x_threaded = th;
}

static void coll_free(t_messcoll *x){
    coll_wclose(x);
	if(x->x_threaded == 1)
        coll_dothread(x, 0);
    pd_unbind(&x->x_obj.ob_pd, x->x_bindsym);
    clock_free(x->x_clock);
    elsefile_free(x->x_filehandle);
    coll_unbind(x, 0);
}

static void coll_init(t_messcoll *x, t_symbol *name){
    t_messcollcommon *cc = 0;
    if(name == NULL)
        name = 0;
    else // named object, look for existing instances
        cc = (t_messcollcommon *)pd_findbyclass(name, messcollcommon_class);
    if(!cc){ // not named or non existing named object found, initialize
        cc = (t_messcollcommon *)collcommon_init();
        if(!name){ // not named
            cc->c_filename = 0;
            cc->c_lastcanvas = 0;
        }
        else // named, so bind to it
            pd_bind(&cc->c_pd, name);
        cc->c_filehandle = elsefile_new((t_pd *)cc, 0, collcommon_readhook,
            collcommon_writehook, collcommon_editorhook);
    };
    x->x_common = cc;
    x->x_name = name;
    x->x_next = cc->c_refs;
    cc->c_refs = x;
}

static void *coll_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_messcoll *x = (t_messcoll *)pd_new(messcoll_class);
    t_symbol *name = NULL, *file = NULL;
    x->x_canvas = canvas_getcurrent();
    char buf[MAXPDSTRING];
    buf[MAXPDSTRING-1] = 0;
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindsym = gensym(buf));
    outlet_new((t_object *)x, &s_);
    x->x_keyout = outlet_new((t_object *)x, &s_);
    x->x_info_out = outlet_new((t_object *)x, &s_bang);
    x->x_filehandle = elsefile_new((t_pd *)x, coll_keephook, 0, 0, 0);
    int keep = 0, threaded = 0, arg = 0;
    // check arguments for filename and threaded version
    // right now, don't care about arg order (mb we should?) - DK
    if(av->a_type == A_SYMBOL){
        while(ac){
            t_symbol * cursym = atom_getsymbol(av);
            if(cursym == gensym("-k") && arg == 0)
                keep = 1;
            else if(cursym == gensym("-t") && arg == 0)
                threaded = 1;
            else if(name == NULL && arg == 0){
                name = cursym;
                arg = 1;
            }
            else if(file == NULL && arg == 1)
                file = cursym;
            else
                goto errstate;
            ac--, av++;
        }
    }
    x->x_is_opened = 0;
    x->x_filebang = 0;
	x->unsafe = 0;
	x->init = 0;
    x->x_clock = clock_new(x, (t_method)coll_tick);
	coll_threaded(x, threaded);
    coll_init(x, name);
    if(file != NULL){ // look for text file
        t_msg *msg = collcommon_doread(x->x_common, file, x->x_canvas, 0);
        if(msg->m_line > 0){
            x->x_common->c_fileoninit = 1;
            x->x_filebang = 1;
            x->x_initread = 1;
            clock_delay(x->x_clock, 0);
        };
    };
    x->x_keep = keep;
    return(x);
	errstate:
		pd_error(x, "[messcoll]: improper args");
		return(NULL);
}

void messcoll_setup(void){
    messcoll_class = class_new(gensym("messcoll"), (t_newmethod)(void*)coll_new,
        (t_method)coll_free, sizeof(t_messcoll), 0, A_GIMME, 0);
    class_addlist(messcoll_class, coll_list);
    class_addbang(messcoll_class, coll_bang);
    class_addfloat(messcoll_class, coll_float);
    class_addsymbol(messcoll_class, coll_symbol);
    class_addmethod(messcoll_class, (t_method)coll_store, gensym("store"), A_GIMME, 0);
    class_addmethod(messcoll_class, (t_method)coll_delete, gensym("delete"), A_GIMME, 0);
    class_addmethod(messcoll_class, (t_method)coll_clear, gensym("clear"), 0);
    class_addmethod(messcoll_class, (t_method)coll_bang, gensym("next"), 0);
    class_addmethod(messcoll_class, (t_method)coll_prev, gensym("prev"), 0);
    class_addmethod(messcoll_class, (t_method)coll_start, gensym("start"), 0);
    class_addmethod(messcoll_class, (t_method)coll_end, gensym("end"), 0);
    class_addmethod(messcoll_class, (t_method)coll_goto, gensym("goto"), A_GIMME, 0);
    class_addmethod(messcoll_class, (t_method)coll_size, gensym("size"), 0);
    class_addmethod(messcoll_class, (t_method)coll_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(messcoll_class, (t_method)coll_keep, gensym("keep"), A_FLOAT,0);
    class_addmethod(messcoll_class, (t_method)coll_threaded, gensym("threaded"), A_FLOAT,0);
    class_addmethod(messcoll_class, (t_method)coll_read, gensym("read"), A_DEFSYM, 0);
    class_addmethod(messcoll_class, (t_method)coll_write, gensym("write"), A_DEFSYM, 0);
    class_addmethod(messcoll_class, (t_method)coll_dump, gensym("dump"), 0);
    class_addmethod(messcoll_class, (t_method)coll_open, gensym("show"), 0);
    class_addmethod(messcoll_class, (t_method)coll_wclose, gensym("hide"), 0);
    class_addmethod(messcoll_class, (t_method)coll_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(messcoll_class, (t_method)coll_is_opened, gensym("_is_opened"), A_FLOAT , A_FLOAT, 0);
    elsefile_setup(messcoll_class, 1);
    messcollcommon_class = class_new(gensym("messcoll"), 0, 0, sizeof(t_messcollcommon), CLASS_PD, 0);
// nop (collcommon doesn't keep, file is already set), but it's good to have it just in case?
    elsefile_setup(messcollcommon_class, 0);
}
