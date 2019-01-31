
#include <stdio.h>
#include <string.h>
#include "m_pd.h"

// Pattern types
#define MAKESYMBOL_UNSUPPORTED  0
#define MAKESYMBOL_LITERAL      1
#define MAKESYMBOL_MINSLOTTYPE  2
#define MAKESYMBOL_INT          2
#define MAKESYMBOL_FLOAT        3
#define MAKESYMBOL_CHAR         4
#define MAKESYMBOL_STRING       5

/* Numbers:  assuming max 62 digits preceding a decimal point in any
 fixed-point representation of a t_float (39 in my system)
 -- need to be sure, that using max precision would never produce
 a representation longer than max width.  If this is so, then no number
 representation would exceed max width (presumably...).
 Strings:  for the time being, any string longer than max width would
 be truncated (somehow compatible with Str256, but LATER warn-and-allow). */
/* LATER rethink it all */
#define MAKESYMBOL_MAXPRECISION  192
#define MAKESYMBOL_MAXWIDTH      512

typedef struct _makesymbol{
    t_object  x_ob;
    int       x_nslots;
    int       x_nproxies;  /* as requested (and allocated) */
    t_pd    **x_proxies;
    int       x_fsize;     /* as allocated (i.e. including a terminating 0) */
    char     *x_fstring;
}t_makesymbol;

typedef struct _makesymbol_proxy{
    t_object        p_ob;
    t_makesymbol   *p_master;
    int             p_id;
    int             p_type;  // value #defined above
    char           *p_pattern;
    char           *p_pattend;
    t_atom          p_atom;  // input
    int             p_size;
    int             p_valid;
}t_makesymbol_proxy;

static t_class *makesymbol_class;
static t_class *makesymbol_proxy_class;

// LATER: use snprintf (should be available on all systems)
static void makesymbol_proxy_checkit(t_makesymbol_proxy *x, char *buf){
    int result = 0, valid = 0;
    char *pattend = x->p_pattend;
    if(pattend){
        char tmp = *pattend;
        *pattend = 0;
        if(x->p_atom.a_type == A_FLOAT){
            t_float f = x->p_atom.a_w.w_float;
            if(x->p_type == MAKESYMBOL_INT) // CHECKME large/negative values
                result = sprintf(buf, x->p_pattern, (long)f);
            else if(x->p_type == MAKESYMBOL_FLOAT)
                result = sprintf(buf, x->p_pattern, f);
            else if(x->p_type == MAKESYMBOL_CHAR) // a 0 input into %c nulls output
    // float into %c is truncated, but CHECKME large/negative values */
                result = sprintf(buf, x->p_pattern, (unsigned char)f);
            else if(x->p_type == MAKESYMBOL_STRING){ // a float into %s is ok
                char tmp[64];  // rethink
                sprintf(tmp, "%g", f);
                result = sprintf(buf, x->p_pattern, tmp);
            }
            else pd_error(x, "[makesymbol]: can't convert float to type of argument %d", x->p_id + 1);
            if(result > 0)
                valid = 1;
        }
        else if (x->p_atom.a_type == A_SYMBOL){
            t_symbol *s = x->p_atom.a_w.w_symbol;
            if(x->p_type == MAKESYMBOL_STRING){
                if (strlen(s->s_name) > MAKESYMBOL_MAXWIDTH){
                    strncpy(buf, s->s_name, MAKESYMBOL_MAXWIDTH);
                    buf[MAKESYMBOL_MAXWIDTH] = 0;
                    result = MAKESYMBOL_MAXWIDTH;
                }
                else result = sprintf(buf, x->p_pattern, s->s_name);
                if (result >= 0)
                    valid = 1;
            }
            else pd_error(x, "[makesymbol]: can't convert symbol to type of argument %d", x->p_id + 1);
        }
        *pattend = tmp;
    }
    else pd_error(x, "makesymbol_proxy_checkit");
    if((x->p_valid = valid))
        x->p_size = result;
    else
        x->p_size = 0;
}

static void makesymbol_dooutput(t_makesymbol *x){
    int i, outsize;
    char *outstring;
    outsize = x->x_fsize;  // strlen() + 1
    // LATER consider subtracting format pattern sizes
    for(i = 0; i < x->x_nslots; i++){
        t_makesymbol_proxy *y = (t_makesymbol_proxy *)x->x_proxies[i];
        if (y->p_valid)
            outsize += y->p_size;
        else // invalid input
            return;
    }
    if(outsize > 0 && (outstring = getbytes(outsize))){
        char *inp = x->x_fstring;
        char *outp = outstring;
        for (i = 0; i < x->x_nslots; i++){
            t_makesymbol_proxy *y = (t_makesymbol_proxy *)x->x_proxies[i];
            int len = y->p_pattern - inp;
            if (len > 0){
                strncpy(outp, inp, len);
                outp += len;
            }
            makesymbol_proxy_checkit(y, outp);
            outp += y->p_size;  /* p_size is never negative */
            inp = y->p_pattend;
        }
        strcpy(outp, inp);
        outp = outstring;
        outlet_symbol(((t_object *) x)->ob_outlet, gensym(outstring));
        freebytes(outstring, outsize);
    }
}

static void makesymbol_proxy_bang(t_makesymbol_proxy *x){
    if(x->p_id == 0)
        makesymbol_dooutput(x->p_master);  // CHECKED (in any inlet)
    else
        pd_error(x, "[makesymbol]: can't convert bang to type of argument %d", (x->p_id) + 1);
}

static void makesymbol_proxy_float(t_makesymbol_proxy *x, t_float f){
    char buf[MAKESYMBOL_MAXWIDTH + 1];  /* LATER rethink */
    SETFLOAT(&x->p_atom, f);
    makesymbol_proxy_checkit(x, buf);
    if (x->p_id == 0 && x->p_valid)
        makesymbol_dooutput(x->p_master);  // CHECKED: only first inlet
}

static void makesymbol_proxy_symbol(t_makesymbol_proxy *x, t_symbol *s){
    char buf[MAKESYMBOL_MAXWIDTH + 1];  // LATER rethink
    if (s && *s->s_name)
        SETSYMBOL(&x->p_atom, s);
    else
        SETFLOAT(&x->p_atom, 0);
    makesymbol_proxy_checkit(x, buf);
    if (x->p_id == 0 && x->p_valid)
        makesymbol_dooutput(x->p_master);  // CHECKED: only first inlet
}

static void makesymbol_dolist(t_makesymbol *x, t_symbol *s, int ac, t_atom *av, int startid){
    t_symbol *dummy = s;
    dummy = NULL;
    int cnt = x->x_nslots - startid;
    if(ac > cnt)
        ac = cnt;
    if(ac-- > 0){
        int id;
        for(id = startid + ac, av += ac; id >= startid; id--, av--){
            if(av->a_type == A_FLOAT)
                makesymbol_proxy_float((t_makesymbol_proxy *)x->x_proxies[id], av->a_w.w_float);
            else if(av->a_type == A_SYMBOL)
                makesymbol_proxy_symbol((t_makesymbol_proxy *)x->x_proxies[id], av->a_w.w_symbol);
        }
    }
}

static void makesymbol_doanything(t_makesymbol *x, t_symbol *s, int ac, t_atom *av, int startid){
    if(s && s != &s_){
        makesymbol_dolist(x, 0, ac, av, startid + 1);
        makesymbol_proxy_symbol((t_makesymbol_proxy *)x->x_proxies[startid], s);
    }
    else
        makesymbol_dolist(x, 0, ac, av, startid);
}

static void makesymbol_proxy_list(t_makesymbol_proxy *x, t_symbol *s, int ac, t_atom *av){
    makesymbol_dolist(x->p_master, s, ac, av, x->p_id);
}

static void makesymbol_proxy_anything(t_makesymbol_proxy *x, t_symbol *s, int ac, t_atom *av){
    makesymbol_doanything(x->p_master, s, ac, av, x->p_id);
}

static void makesymbol_bang(t_makesymbol *x){
    if (x->x_nslots)
        makesymbol_proxy_bang((t_makesymbol_proxy *)x->x_proxies[0]);
    else if(x->x_fsize >= 2)
        outlet_symbol(((t_object *) x)->ob_outlet, gensym(x->x_fstring));
    else
        pd_error(x, "[makesymbol]: no arguments given");
}

static void makesymbol_float(t_makesymbol *x, t_float f){
    if (x->x_nslots)
        makesymbol_proxy_float((t_makesymbol_proxy *)x->x_proxies[0], f);
    else
        pd_error(x, "[makesymbol]: can't convert float to type of argument 1");
}

static void makesymbol_symbol(t_makesymbol *x, t_symbol *s){
    if(x->x_nslots)
        makesymbol_proxy_symbol((t_makesymbol_proxy *)x->x_proxies[0], s);
    else
        pd_error(x, "[makesymbol]: can't convert symbol to type of argument 1");
}

static void makesymbol_list(t_makesymbol *x, t_symbol *s, int ac, t_atom *av){
    if (x->x_nslots)
        makesymbol_dolist(x, s, ac, av, 0);
    else
        pd_error(x, "[makesymbol]: can't convert list to type of argument 1");
}

static void makesymbol_anything(t_makesymbol *x, t_symbol *s, int ac, t_atom *av){
    if (x->x_nslots)
        makesymbol_doanything(x, s, ac, av, 0);
    else
        pd_error(x, "[makesymbol]: can't convert anything to type of argument 1");
}

// adjusted binbuf_gettext(), LATER do it right
static char *hammer_gettext(int ac, t_atom *av, int *sizep){
    char *buf = getbytes(1);
    int size = 1;
    char atomtext[MAXPDSTRING];
    while (ac--){
        char *newbuf;
        int newsize;
        if (buf[size-1] == 0 || av->a_type == A_SEMI || av->a_type == A_COMMA)
            size--;
        atom_string(av, atomtext, MAXPDSTRING);
        newsize = size + strlen(atomtext) + 1;
        if (!(newbuf = resizebytes(buf, size, newsize))){
            *sizep = 1;
            return (getbytes(1));
        }
        buf = newbuf;
        strcpy(buf + size, atomtext);
        size = newsize;
        buf[size-1] = ' ';
        av++;
    }
    buf[size-1] = 0;
    *sizep = size;
    return (buf);
}

// get TYPE
/* Called twice:  1st (with x == 0) for counting valid patterns;
 2nd (after allocation) to initializing the proxies.
 If there's a "%%" pattern, buffer is shrinked in the second pass (LATER rethink). */
static int makesymbol_parsepattern(t_makesymbol *x, char **patternp){
    int type = MAKESYMBOL_UNSUPPORTED;
    char errstring[MAXPDSTRING];
    char *ptr;
    char modifier = 0;
    int width = 0;
    int precision = 0;
    int *numfield = &width;
    int dotseen = 0;
    *errstring = 0;
    for(ptr = *patternp; *ptr; ptr++){
        if(*ptr >= '0' && *ptr <= '9'){
            if(!numfield){
                if(x)
                    sprintf(errstring, "extra number field");
                break;
            }
            *numfield = 10 * *numfield + *ptr - '0';
            if(dotseen){
                if(precision > MAKESYMBOL_MAXPRECISION){
                    if(x)
                        sprintf(errstring, "precision field too large");
                    break;
                }
            }
            else{
                if(width > MAKESYMBOL_MAXWIDTH){
                    if(x)
                        sprintf(errstring, "width field too large");
                    break;
                }
            }
            continue;
        }
        if(*numfield)
            numfield = 0;
        if(strchr("pdiouxX", *ptr)){
            type = MAKESYMBOL_INT;
            break;
        }
        else if(strchr("eEfFgG", *ptr)){
/*            if(modifier){
                if(x)
                    sprintf(errstring, "\'%c\' modifier not supported", modifier);
                break;
            }*/
            type = MAKESYMBOL_FLOAT;
            break;
        }
        else if (strchr("c", *ptr)){
            if(modifier){
                if(x)
                    sprintf(errstring, "\'%c\' modifier not supported", modifier);
                break;
            }
            type = MAKESYMBOL_CHAR;
            break;
        }
        else if (strchr("s", *ptr)){
            if(modifier){
                if(x)
                    sprintf(errstring, "\'%c\' modifier not supported", modifier);
                break;
            }
            type = MAKESYMBOL_STRING;
            break;
        }
        else if(*ptr == '%'){
            type = MAKESYMBOL_LITERAL;
            if(x){ // buffer-shrinking hack
                char *p1 = ptr, *p2 = ptr + 1;
                do
                    *p1++ = *p2;
                while (*p2++);
                ptr--;
            }
            break;
        }
        else if(strchr("CSnm", *ptr)){
            if(x)
                sprintf(errstring, "\'%c\' type not supported", *ptr);
            break;
        }
        else if(strchr("l", *ptr)){
            if(modifier){
                if(x)
                    sprintf(errstring, "only single modifier is supported");
                break;
            }
            modifier = *ptr;
        }
/*        else if(strchr("h", *ptr)){ // short int is stupid
            if(modifier){
                if(x)
                    sprintf(errstring, "only single modifier is supported");
                break;
            }
            modifier = *ptr;
        }*/
        else if(strchr("aAjqtzZ", *ptr)){
            if(x)
                sprintf(errstring, "\'%c\' modifier not supported", *ptr);
            break;
        }
        else if (*ptr == '.'){
            if(dotseen){
                if(x)
                    sprintf(errstring, "multiple dots");
                break;
            }
            numfield = &precision;
            dotseen = 1;
        }
        else if(*ptr == '$'){
            if(x)
                sprintf(errstring, "parameter number field not supported");
            break;
        }
        else if(*ptr == '*'){
            if(x)
                sprintf(errstring, "%s parameter not supported", (dotseen ? "precision" : "width"));
            break;
        }
        else if(!strchr("-+ #\'", *ptr)){
            if (x) sprintf(errstring, "\'%c\' format character not supported", *ptr);
            break;
        }
    }
    if(*ptr)
        ptr++; // LATER rethink
    else
        if(x)
            sprintf(errstring, "type not specified");
    if(x && type == MAKESYMBOL_UNSUPPORTED){
        if (*errstring)
            pd_error(x, "[makesymbol]: slot skipped (%s %s)", errstring, "in a format pattern");
        else
            pd_error(x, "[makesymbol]: slot skipped");
    }
    *patternp = ptr;
    return(type);
}

static void makesymbol_free(t_makesymbol *x){
    if (x->x_proxies){
        int i = x->x_nslots;
        while (i--){
            t_makesymbol_proxy *y = (t_makesymbol_proxy *)x->x_proxies[i];
            pd_free((t_pd *)y);
        }
        freebytes(x->x_proxies, x->x_nproxies * sizeof(*x->x_proxies));
    }
    if (x->x_fstring)
        freebytes(x->x_fstring, x->x_fsize);
}

static void *makesymbol_new(t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    t_makesymbol *x;
    int fsize;
    char *fstring;
    char *p1, *p2;
    int i = 1, nslots, nproxies = 0;
    t_pd **proxies;
    fstring = hammer_gettext(ac, av, &fsize);
    p1 = fstring;
    while((p2 = strchr(p1, '%'))){
        int type;
        p1 = p2 + 1;
        type = makesymbol_parsepattern(0, &p1);
        if(type >= MAKESYMBOL_MINSLOTTYPE)
            nproxies++;
    }
    if(!nproxies){ // no arguments creates with an inlet and prints errors
        x = (t_makesymbol *)pd_new(makesymbol_class);
        x->x_nslots = 0;
        x->x_nproxies = 0;
        x->x_proxies = 0;
        x->x_fsize = fsize;
        x->x_fstring = fstring;
        p1 = fstring;
        while ((p2 = strchr(p1, '%'))){
            p1 = p2 + 1;
            makesymbol_parsepattern(x, &p1);
        };
        outlet_new((t_object *)x, &s_symbol);
        return (x);
    }
    if (!(proxies = (t_pd **)getbytes(nproxies * sizeof(*proxies)))){
        freebytes(fstring, fsize);
        return (0);
    }
    for (nslots = 0; nslots < nproxies; nslots++)
        if (!(proxies[nslots] = pd_new(makesymbol_proxy_class))) break;
    if (!nslots){
        freebytes(fstring, fsize);
        freebytes(proxies, nproxies * sizeof(*proxies));
        return (0);
    }
    x = (t_makesymbol *)pd_new(makesymbol_class);
    x->x_nslots = nslots;
    x->x_nproxies = nproxies;
    x->x_proxies = proxies;
    x->x_fsize = fsize;
    x->x_fstring = fstring;
    p1 = fstring;
    i = 0;
    while((p2 = strchr(p1, '%'))){
        int type;
        p1 = p2 + 1;
        type = makesymbol_parsepattern(x, &p1);
        if(type >= MAKESYMBOL_MINSLOTTYPE){
            if(i < nslots){
                char buf[MAKESYMBOL_MAXWIDTH + 1];  // LATER rethink
                t_makesymbol_proxy *y = (t_makesymbol_proxy *)proxies[i];
                y->p_master = x;
                y->p_id = i;
                y->p_type = type;
                y->p_pattern = p2;
                y->p_pattend = p1;
                if(type == MAKESYMBOL_STRING)
                    SETSYMBOL(&y->p_atom, &s_);
                else
                    SETFLOAT(&y->p_atom, 0);
                y->p_size = 0;
                y->p_valid = 0;
// creates inlets for any '%' (if invalid `can't convert' errors are printed)
                if(i)
                    inlet_new((t_object *)x, (t_pd *)y, 0, 0);
                makesymbol_proxy_checkit(y, buf);
                i++;
            }
        }
    }
    outlet_new((t_object *)x, &s_symbol);
    return(x);
}

void makesymbol_setup(void){
    makesymbol_class = class_new(gensym("makesymbol"), (t_newmethod)makesymbol_new,
        (t_method)makesymbol_free, sizeof(t_makesymbol), 0, A_GIMME, 0);
    class_addbang(makesymbol_class, makesymbol_bang);
    class_addfloat(makesymbol_class, makesymbol_float);
    class_addsymbol(makesymbol_class, makesymbol_symbol);
    class_addlist(makesymbol_class, makesymbol_list);
    class_addanything(makesymbol_class, makesymbol_anything);
    makesymbol_proxy_class = class_new(gensym("_makesymbol_proxy"), 0, 0,
        sizeof(t_makesymbol_proxy), CLASS_PD | CLASS_NOINLET, 0);
    class_addbang(makesymbol_proxy_class, makesymbol_proxy_bang);
    class_addfloat(makesymbol_proxy_class, makesymbol_proxy_float);
    class_addsymbol(makesymbol_proxy_class, makesymbol_proxy_symbol);
    class_addlist(makesymbol_proxy_class, makesymbol_proxy_list);
    class_addanything(makesymbol_proxy_class, makesymbol_proxy_anything);
}

