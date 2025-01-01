
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "m_pd.h"

/* Pattern types.  These are the parsing routine's return values.
   If returned value is >= FORMAT_MINSLOTTYPE, then another slot
   is created (i.e. an inlet, and a proxy handling it). */
#define FORMAT_UNSUPPORTED  0
#define FORMAT_LITERAL      1
#define FORMAT_MINSLOTTYPE  2
#define FORMAT_INT          2
#define FORMAT_FLOAT        3
#define FORMAT_CHAR         4
#define FORMAT_STRING       5

/* Numbers:  assuming max 62 digits preceding a decimal point in any
   fixed-point representation of a t_float (39 in my system)
   -- need to be sure, that using max precision would never produce
   a representation longer than max width.  If this is so, then no number
   representation would exceed max width (presumably...).
   Strings:  for the time being, any string longer than max width would
   be truncated (somehow compatible with Str256, but LATER warn-and-allow). */
/* LATER rethink it all */
#define FORMAT_MAXPRECISION  192
#define FORMAT_MAXWIDTH      256

typedef struct _format{
    t_object  x_ob;
    int       x_nslots;
    int       x_nproxies;  /* as requested (and allocated) */
    t_pd    **x_proxies;
    t_symbol *x_ignore;
    int       x_fsize;     /* as allocated (i.e. including a terminating 0) */
    char     *x_fstring;
}t_format;

typedef struct _format_proxy{
    t_object    p_ob;
    t_format  *p_master;
    int         p_id;
    int         p_type;  /* a value #defined above */
    char       *p_pattern;
    char       *p_pattend;
    t_atom      p_atom;  /* current input */
    int         p_size;
    int         p_valid;
}t_format_proxy;

static t_class *format_class, *format_proxy_class;

/* LATER use snprintf, if it is available on other systems (should be...) */
static void format_proxy_checkit(t_format_proxy *x, char *buf){
    int result = 0, valid = 0;
    char *pattend = x->p_pattend;
    if(pattend){
        char tmp = *pattend;
        *pattend = 0;
        if(x->p_atom.a_type == A_FLOAT){
            t_float f = x->p_atom.a_w.w_float;
            if(x->p_type == FORMAT_INT)
            /* CHECKME large/negative values */
                result = sprintf(buf, x->p_pattern, (long)f);
            else if(x->p_type == FORMAT_FLOAT)
                result = sprintf(buf, x->p_pattern, f);
            else if(x->p_type == FORMAT_CHAR)
            /* CHECKED: if 0 is input into a %c-slot, the whole output
		   string is null-terminated */
            /* CHECKED: float into a %c-slot is truncated,
		   but CHECKME large/negative values */
                result = sprintf(buf, x->p_pattern, (unsigned char)f);
            else if(x->p_type == FORMAT_STRING){
                /* CHECKED: any number input into a %s-slot is ok */
                char temp[64];  /* LATER rethink */
                sprintf(temp, "%g", f);
                result = sprintf(buf, x->p_pattern, temp);
            }
            else
                pd_error(x, "[format]: can't convert float to type of argument %d", x->p_id + 1);
            if(result > 0)
                valid = 1;
        }
        else if(x->p_atom.a_type == A_SYMBOL){
            t_symbol *s = x->p_atom.a_w.w_symbol;
            if(x->p_type == FORMAT_STRING){
                if(strlen(s->s_name) > FORMAT_MAXWIDTH){
                    strncpy(buf, s->s_name, FORMAT_MAXWIDTH);
                    buf[FORMAT_MAXWIDTH] = 0;
                    result = FORMAT_MAXWIDTH;
                }
                else
                    result = sprintf(buf, x->p_pattern, s->s_name);
                if(result >= 0)
                    valid = 1;
            }
            else
                pd_error(x, "[format]: can't convert symbol to type of argument %d", x->p_id + 1);
        }
        *pattend = tmp;
    }
    else
        pd_error(x, "format_proxy_checkit");
    if((x->p_valid = valid))
        x->p_size = result;
    else
        x->p_size = 0;
}

static void format_dooutput(t_format *x){
    int i, outsize;
    char *outstring;
    outsize = x->x_fsize;  /* this is strlen() + 1 */
    /* LATER consider subtracting format pattern sizes */
    for(i = 0; i < x->x_nslots; i++){
        t_format_proxy *y = (t_format_proxy *)x->x_proxies[i];
        if(y->p_valid)
            outsize += y->p_size;
        else{
            /* slot i has received an invalid input -- CHECKME if this
	       condition blocks all subsequent output requests? */
            return;
        }
    }
    if(outsize > 0 && (outstring = getbytes(outsize))){
        char *inp = x->x_fstring;
        char *outp = outstring;
        for(i = 0; i < x->x_nslots; i++){
            t_format_proxy *y = (t_format_proxy *)x->x_proxies[i];
            int len = y->p_pattern - inp;
            if(len > 0){
                strncpy(outp, inp, len);
                outp += len;
            }
            format_proxy_checkit(y, outp);
            outp += y->p_size;  /* p_size is never negative */
            inp = y->p_pattend;
        }
        strcpy(outp, inp);
        outp = outstring;
        outlet_symbol(((t_object *) x)->ob_outlet, gensym(outstring));
        freebytes(outstring, outsize);
    }
}

static void format_proxy_float(t_format_proxy *x, t_float f){
    char buf[FORMAT_MAXWIDTH + 1];  /* LATER rethink */
    SETFLOAT(&x->p_atom, f);
    format_proxy_checkit(x, buf);
    if(x->p_id == 0 && x->p_valid)
	format_dooutput(x->p_master);  /* CHECKED: only first inlet */
}

static void format_proxy_symbol(t_format_proxy *x, t_symbol *s){
    char buf[FORMAT_MAXWIDTH + 1];
/*    if(s && *s->s_name)
        SETSYMBOL(&x->p_atom, s);
    else
        SETFLOAT(&x->p_atom, 0);*/
    SETSYMBOL(&x->p_atom, s);
    format_proxy_checkit(x, buf);
    if(x->p_id == 0 && x->p_valid)
	format_dooutput(x->p_master);  /* CHECKED: only first inlet */
}

static void format_dolist(t_format *x, t_symbol *s, int ac, t_atom *av, int startid){
    x->x_ignore = s;
    int cnt = x->x_nslots - startid;
    if(ac > cnt)
        ac = cnt;
    if(ac-- > 0){
        int id;
        for(id = startid + ac, av += ac; id >= startid; id--, av--){
            if(av->a_type == A_FLOAT)
                format_proxy_float((t_format_proxy *)x->x_proxies[id], av->a_w.w_float);
            else if(av->a_type == A_SYMBOL)
                format_proxy_symbol((t_format_proxy *)x->x_proxies[id], av->a_w.w_symbol);
        }
    }
}

static void format_doanything(t_format *x, t_symbol *s, int ac, t_atom *av, int startid){
    if(s && s != &s_){
        format_dolist(x, &s_list, ac, av, startid + 1);
        format_proxy_symbol((t_format_proxy *)x->x_proxies[startid], s);
    }
    else
        format_dolist(x, &s_list, ac, av, startid);
}

static void format_proxy_anything(t_format_proxy *x, t_symbol *s, int ac, t_atom *av){
    format_doanything(x->p_master, s, ac = 0, av = NULL, x->p_id);
}

static void format_float(t_format *x, t_float f){
    if(x->x_nslots)
        format_proxy_float((t_format_proxy *)x->x_proxies[0], f);
    else
        pd_error(x, "[format]: no variables given");
}

static void format_symbol(t_format *x, t_symbol *s){
    if(x->x_nslots)
        format_proxy_symbol((t_format_proxy *)x->x_proxies[0], s);
    else
        pd_error(x, "[format]: no variables given");
}

static void format_list(t_format *x, t_symbol *s, int ac, t_atom *av){
    if(x->x_nslots)
        format_dolist(x, s, ac, av, 0);
    else
        pd_error(x, "[format]: no variables given");
}

static void format_anything(t_format *x, t_symbol *s, int ac, t_atom *av){
    if(x->x_nslots)
        format_doanything(x, s, ac, av, 0);
    else
        pd_error(x, "[format]: no variables given");
}

/* Called twice:  1st pass (with x == 0) is used for counting valid patterns;
   2nd pass (after object allocation) -- for initializing the proxies.
   If there is a "%%" pattern, then the buffer is shrunk in the second pass
   (LATER rethink). */
static int format_parsepattern(t_format *x, char **patternp){
    post("format_parsepattern patternp = %s", patternp);
    int type = FORMAT_UNSUPPORTED;
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
                if(x) sprintf(errstring, "extra number field");
                    break;
            }
            *numfield = 10 * *numfield + *ptr - '0';
            if(dotseen){
                if(precision > FORMAT_MAXPRECISION){
                    if(x) sprintf(errstring, "precision field too large");
                        break;
                }
            }
            else{
                if(width > FORMAT_MAXWIDTH){
                    if(x) sprintf(errstring, "width field too large");
                        break;
                }
            }
            continue;
        }
        if(*numfield)
            numfield = 0;
        if(strchr("diouxX", *ptr)){
            type = FORMAT_INT;
            break;
        }
        else if(strchr("eEfFgG", *ptr)){
/*            if(modifier)
                {
                if(x) sprintf(errstring, "\'%c\' modifier not supported", modifier);
                    break;
                }*/
            type = FORMAT_FLOAT;
            break;
        }
        else if(strchr("c", *ptr)){
            if(modifier){
                if(x) sprintf(errstring, "\'%c\' modifier not supported", modifier);
                    break;
            }
            type = FORMAT_CHAR;
            break;
        }
        else if(strchr("s", *ptr)){
            if(modifier){
                if(x) sprintf(errstring, "\'%c\' modifier not supported", modifier);
                    break;
            }
            type = FORMAT_STRING;
            break;
            }
        else if(*ptr == '%'){
            type = FORMAT_LITERAL;
            if(x){  // buffer-shrinking hack, LATER rethink
                char *p1 = ptr, *p2 = ptr + 1;
                do
                    *p1++ = *p2;
                    while(*p2++);
                        ptr--;
            }
            break;
        }
        else if(strchr("CSnm", *ptr)){
            if(x) sprintf(errstring, "\'%c\' type not supported", *ptr);
                break;
        }
        else if(strchr("l", *ptr)){
            if(modifier){
                if(x) sprintf(errstring, "only single modifier is supported");
                    break;
            }
            modifier = *ptr;
        }
/* added this for new formats but it did not work (porres)
        else if(strchr("L", *ptr)){
                if(modifier){
                if(x)
                    sprintf(errstring, "only single modifier is supported");
                break;
            }
            modifier = *ptr;
        }*/
        else if(strchr("h", *ptr)){
            if(modifier){
                if(x) sprintf(errstring, "only single modifier is supported");
                break;
            }
            modifier = *ptr;
        }
        else if(strchr("aAhjLqtzZ", *ptr)){
            if(x) sprintf(errstring, "\'%c\' modifier not supported", *ptr);
                break;
        }
        else if(*ptr == '.'){
            if(dotseen){
                if(x) sprintf(errstring, "multiple dots");
                    break;
            }
            numfield = &precision;
            dotseen = 1;
        }
        else if(*ptr == '$'){
            if(x) sprintf(errstring, "parameter number field not supported");
                break;
        }
        else if(*ptr == '*'){
            if(x) sprintf(errstring, "%s parameter not supported", (dotseen ? "precision" : "width"));
                break;
        }
        else if(!strchr("-+ #\'", *ptr)){
            if(x) sprintf(errstring, "\'%c\' format character not supported", *ptr);
                break;
        }
    }
    if(*ptr)
        ptr++;  /* LATER rethink */
    else
        if(x) sprintf(errstring, "type not specified");
            if(x && type == FORMAT_UNSUPPORTED){
                if(*errstring)
                    pd_error(x, "[format]: slot skipped (%s %s)", errstring, "in a format pattern");
                else
                    pd_error(x, "[format]: slot skipped");
            }
    *patternp = ptr;
    return(type);
}

static void format_free(t_format *x){
    if(x->x_proxies){
        int i = x->x_nslots;
        while(i--){
            t_format_proxy *y = (t_format_proxy *)x->x_proxies[i];
            pd_free((t_pd *)y);
        }
        freebytes(x->x_proxies, x->x_nproxies * sizeof(*x->x_proxies));
    }
}

/* adjusted binbuf_gettext(), LATER do it right */
static char *format_gettext(int ac, t_atom *av, int *sizep){
    char *buf = getbytes(1);
    int size = 1;
    char atomtext[MAXPDSTRING];
    while (ac--){
        char *newbuf;
        int newsize;
        if(buf[size-1] == 0 || av->a_type == A_SEMI || av->a_type == A_COMMA)
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
    return(buf);
}

static void *format_new(t_symbol *s){
    t_format *x = (t_format *)pd_new(format_class);
    outlet_new((t_object *)x, &s_symbol);
    
    char *fstring, *p1, *p2;
    int i, nslots, nproxies = 0;
    t_pd **proxies;
    
    int fsize;
    t_atom at[1];
    SETSYMBOL(at, s);
    fstring = format_gettext(1, at, &fsize);
    
    p1 = fstring;
    
    while((p2 = strchr(p1, '%'))){
        int type;
        p1 = p2 + 1;
        type = format_parsepattern(0, &p1);
        if(type >= FORMAT_MINSLOTTYPE)
            nproxies++;
    }
    if(!nproxies){
        x->x_nslots = 0;
        x->x_nproxies = 0;
        x->x_proxies = 0;
        x->x_fsize = fsize;
        x->x_fstring = fstring;
        p1 = fstring;
        while((p2 = strchr(p1, '%'))){
            p1 = p2 + 1;
            format_parsepattern(x, &p1);
        };
        return(x);
    }
    if(!(proxies = (t_pd **)getbytes(nproxies * sizeof(*proxies)))){
        freebytes(fstring, fsize);
        return(0);
    }
    // creates as many inlets as %-signs, no matter if valid or not -- if not,
    // it prints `can't convert'' errors for any input
    for(nslots = 0; nslots < nproxies; nslots++)
        if(!(proxies[nslots] = pd_new(format_proxy_class)))
            break;
    if(!nslots){
        freebytes(fstring, fsize);
        freebytes(proxies, nproxies * sizeof(*proxies));
        return(0);
    }
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
        type = format_parsepattern(x, &p1);
        if(type >= FORMAT_MINSLOTTYPE){
            if(i < nslots){
                char buf[FORMAT_MAXWIDTH + 1];  // LATER rethink
                t_format_proxy *y = (t_format_proxy *)proxies[i];
                y->p_master = x;
                y->p_id = i;
                y->p_type = type;
                y->p_pattern = p2;
                y->p_pattend = p1;
                if(type == FORMAT_STRING)
                    SETSYMBOL(&y->p_atom, &s_);
                else
                    SETFLOAT(&y->p_atom, 0);
                y->p_size = 0;
                y->p_valid = 0;
                if(i)
                    inlet_new((t_object *)x, (t_pd *)y, 0, 0);
                format_proxy_checkit(y, buf);
                i++;
            }
        }
    }
    return(x);
}

void format_setup(void){
    format_class = class_new(gensym("format"), (t_newmethod)format_new,
        (t_method)format_free, sizeof(t_format), 0, A_DEFSYM, 0);
    class_addfloat(format_class, format_float);
    class_addsymbol(format_class, format_symbol);
    class_addlist(format_class, format_list);
    class_addanything(format_class, format_anything);
    format_proxy_class = class_new(gensym("_format_proxy"), 0, 0,
        sizeof(t_format_proxy), CLASS_PD | CLASS_NOINLET, 0);
    class_addfloat(format_proxy_class, format_proxy_float);
    class_addsymbol(format_proxy_class, format_proxy_symbol);
    class_addanything(format_proxy_class, format_proxy_anything);
}
