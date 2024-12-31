// based on cyclone sprintf

#include <m_pd.h>
#include <string.h>
#include <ctype.h>

// Pattern types
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
#define FORMAT_MAXWIDTH      512

typedef struct _format{
    t_object  x_obj;
    t_symbol *x_ignore;
    int       x_nslots;     // number of "slots" (placeholders) in the format string.
    int       x_nproxies;   // as requested (and allocated)
    t_pd    **x_proxies;
    int       x_fsize;      // as allocated (i.e. including a terminating 0)
    char     *x_fstring;
}t_format;

typedef struct _format_proxy{
    t_object    p_obj;
    t_format   *p_master;
    int         p_id;
    int         p_type;  // value #defined above
    char       *p_pattern;
    char       *p_pattend;
    t_atom      p_atom;  // input
    int         p_size;
    int         p_valid;
}t_format_proxy;

static t_class *format_class;
static t_class *format_proxy_class;

// LATER: use snprintf (should be available on all systems)
static void format_proxy_checkit(t_format_proxy *x, char *buf){
    int result = 0, valid = 0;
    char *pattend = x->p_pattend;
    if(pattend){
        char tmp = *pattend;
        *pattend = 0;
        if(x->p_atom.a_type == A_FLOAT){
            t_float f = x->p_atom.a_w.w_float;
            if(x->p_type == FORMAT_INT) // CHECKME large/negative values
                result = sprintf(buf, x->p_pattern, (long)f);
            else if(x->p_type == FORMAT_FLOAT)
                result = sprintf(buf, x->p_pattern, f);
            else if(x->p_type == FORMAT_CHAR) // a 0 input into %c nulls output
    // float into %c is truncated, but CHECKME large/negative values
                result = sprintf(buf, x->p_pattern, (unsigned char)f);
            else if(x->p_type == FORMAT_STRING){ // a float into %s is ok
                char temp[64];  // rethink
                sprintf(temp, "%g", f);
                result = sprintf(buf, x->p_pattern, temp);
            }
            else
                pd_error(x, "[format]: can't convert float (this shouldn't happen)");
            if(result > 0)
                valid = 1;
        }
        else if (x->p_atom.a_type == A_SYMBOL){
            t_symbol *s = x->p_atom.a_w.w_symbol;
            if(x->p_type == FORMAT_STRING){
                if (strlen(s->s_name) > FORMAT_MAXWIDTH){
                    strncpy(buf, s->s_name, FORMAT_MAXWIDTH);
                    buf[FORMAT_MAXWIDTH] = 0;
                    result = FORMAT_MAXWIDTH;
                }
                else result = sprintf(buf, x->p_pattern, s->s_name);
                if (result >= 0)
                    valid = 1;
            }
            else
                pd_error(x, "[format]: can't convert (type mismatch)");
        }
        *pattend = tmp;
    }
    else
        pd_error(x, "format_proxy_checkit"); // ????
    if((x->p_valid = valid))
        x->p_size = result;
    else
        x->p_size = 0;
}

// Helper function to parse atoms manually, respecting escaped spaces
void parse_atoms_with_escaped_spaces(t_format *x, const char *input){
//    post("Input string before parsing: %s", input); // Debug the input string
    if (!input || !*input) return; // Ensure valid input
    char buffer[1024]; // Temporary storage for each token
    int buf_index = 0;
    int escape = 0;    // Flag to detect escaped characters
    t_atom atoms[256]; // Temporary storage for parsed atoms
    int atom_count = 0;
    while (*input) {
        if (escape) {
            // Add escaped character to buffer, print to check if it's correct
//            post("Escaped char: %c", *input); // Debug output to check the escaped character
            if (buf_index < (int)sizeof(buffer) - 1) {
                buffer[buf_index++] = *input; // Add the escaped character (including space) to the buffer
            }
            escape = 0; // Reset escape flag after processing the escaped character
        } else if (*input == '\\') {
            // If we encounter a backslash, print to debug it
//            post("Backslash found!"); // Debug output to check if we are seeing the backslash
            escape = 1; // Next character is escaped
        } else if (isspace(*input)) {
            // If it's a space and not escaped, end the current token
            if (buf_index > 0) {
                buffer[buf_index] = '\0'; // Null-terminate the token
                SETSYMBOL(&atoms[atom_count], gensym(buffer)); // Store the token as an atom
                buf_index = 0; // Reset buffer for the next token
//                post("atoms[atom_count(%d)] = %s", atom_count, atom_getsymbol(&atoms[atom_count])->s_name);
                atom_count++;
            }
        } else {
            // Add the character to the buffer (not a space or backslash)
            if (buf_index < (int)sizeof(buffer) - 1) {
                buffer[buf_index++] = *input;
            }
        }
        input++;
    }
//    post("Parsed token: %s", buffer); // Print each token as it's being processed
//    post("atom_count = %d", atom_count);
    // Handle the final token (after the last character)
    if (buf_index > 0) {
//        post("Handle the final token (after the last character)");
        buffer[buf_index] = '\0';
//        post("atom_count = %d", atom_count);
        SETSYMBOL(&atoms[atom_count], gensym(buffer));
//        post("gensym(buffer) = %s", gensym(buffer)->s_name);
//        post("atom_getsymbol(&atoms[atom_count]) = %s", atom_getsymbol(&atoms[atom_count])->s_name);
        atom_count++;
//        post("atom_count after = %d", atom_count);
    }
//    t_symbol *sym = atom_getsymbol(&atoms[0]);
//    post("sym = %s", sym->s_name);
    // Output the parsed atoms
    if (atom_count == 1) {
//        post("atom_count == 1");
        outlet_symbol(x->x_obj.ob_outlet, atom_getsymbol(&atoms[0]));
    } else if (atom_count > 1) {
//        post("atom_count > 1");
        outlet_list(x->x_obj.ob_outlet, &s_list, atom_count, atoms);
    }
}

static void format_dooutput(t_format *x) {
    int i, outsize = x->x_fsize;
    char *outstring;
    // Calculate required size for formatted string
    for (i = 0; i < x->x_nslots; i++) {
        t_format_proxy *y = (t_format_proxy *)x->x_proxies[i];
        if (y->p_valid) {
            outsize += y->p_size;
        } else {
            post("Invalid proxy input");
            return; // Abort if any proxy is invalid
        }
    }
    if (outsize <= 0) {
        post("Invalid output size");
        return;
    }
    // Allocate memory
    outstring = getbytes(outsize);
    if (!outstring) {
        post("Memory allocation failed");
        return;
    }
    // Build the formatted string
    char *inp = x->x_fstring;
    char *outp = outstring;
    for (i = 0; i < x->x_nslots; i++) {
        t_format_proxy *y = (t_format_proxy *)x->x_proxies[i];
        int len = y->p_pattern - inp;
        if (len > 0) {
            strncpy(outp, inp, len);
            outp += len;
        }
        format_proxy_checkit(y, outp);
        outp += y->p_size; // Move pointer forward
        inp = y->p_pattend;
    }
    // Copy remaining input string
    strcpy(outp, inp);
    // Parse and output atoms
    parse_atoms_with_escaped_spaces(x, outstring);
    // Free allocated memory
    freebytes(outstring, outsize);
}


static void format_proxy_float(t_format_proxy *x, t_float f){
    char buf[FORMAT_MAXWIDTH + 1];  // LATER rethink
    SETFLOAT(&x->p_atom, f);
    format_proxy_checkit(x, buf);
    if (x->p_id == 0 && x->p_valid)
        format_dooutput(x->p_master);  // CHECKED: only first inlet
}

static void format_proxy_symbol(t_format_proxy *x, t_symbol *s){
//    post("format_proxy_symbol s = %s", s->s_name);
    char buf[FORMAT_MAXWIDTH + 1];  // LATER rethink
    if(s && *s->s_name){
//        check_symbol_space_and_escape_it

        // Temporary buffer to hold the processed symbol name
        char temp[FORMAT_MAXWIDTH + 1];
        int j = 0;
        int escaped = 0;
        for(int i = 0; s->s_name[i] != '\0'; i++) {
            if(s->s_name[i] == ' ') {
                // Insert a backslash before the space
                if (j < FORMAT_MAXWIDTH - 1) {
                    temp[j++] = '\\';
                    escaped = 1;
                }
            }
            // Copy the current character
            if (j < FORMAT_MAXWIDTH - 1) {
                temp[j++] = s->s_name[i];
            }
        }
        // Null-terminate the buffer
        temp[j] = '\0';
        // Set the new processed string as the symbol's name
        if(escaped)
            s = gensym(temp);  // Create a new symbol with the modified name
        SETSYMBOL(&x->p_atom, s);
    }
    else
        SETFLOAT(&x->p_atom, 0);
    format_proxy_checkit(x, buf);
    if(x->p_id == 0 && x->p_valid)
        format_dooutput(x->p_master);  // CHECKED: only first inlet
}

static void format_proxy_anything(t_format_proxy *x, t_symbol *s, int ac, t_atom *av){
    x->p_master->x_ignore = s;
    if(ac == 0)
        return;
    if(av->a_type == A_FLOAT)
        format_proxy_float(x, atom_getfloat(av));
    else if(av->a_type == A_SYMBOL)
        format_proxy_symbol(x, atom_getsymbol(av));
}

// do we need this?
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

static void format_list(t_format *x, t_symbol *s, int ac, t_atom *av){
    if(x->x_nslots){
        if(!ac) // bang
            format_dooutput(x);
        else
            format_dolist(x, s, ac, av, 0);
    }
    else
        pd_error(x, "[format]: no variable arguments given");
}

static void format_doanything(t_format *x, t_symbol *s, int ac, t_atom *av, int startid){
    if(s && s != &s_){
        format_dolist(x, 0, ac, av, startid + 1);
        format_proxy_symbol((t_format_proxy *)x->x_proxies[startid], s);
    }
    else
        format_dolist(x, 0, ac, av, startid);
}

static void format_anything(t_format *x, t_symbol *s, int ac, t_atom *av){
    if(x->x_nslots)
        format_doanything(x, s, ac, av, 0);
    else
        pd_error(x, "[format]: no variable arguments given");
}

// adjusted binbuf_gettext(), LATER do it right
static char *makename_getstring(int ac, t_atom *av, int *sizep){
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

// used 2x, 1st (x==0) counts valid patterns, 2nd inits proxies and shrinks %%
static int format_get_type(t_format *x, char **patternp){
    int type = 0;
    char errstring[MAXPDSTRING];
    char *ptr;
//    char modifier = 0;
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
                if(precision > FORMAT_MAXPRECISION){
                    if(x)
                        sprintf(errstring, "precision field too large");
                    break;
                }
            }
            else{
                if(width > FORMAT_MAXWIDTH){
                    if(x)
                        sprintf(errstring, "width field too large");
                    break;
                }
            }
            continue;
        }
        if(*numfield)
            numfield = 0;
        if(strchr("pdiouxX", *ptr)){ // INT
            type = FORMAT_INT;
            break;
        }
        else if(strchr("eEfFgG", *ptr)){ // FLOAT
            // needed to include if(modifier) to prevent %lf and stuff
            type = FORMAT_FLOAT;
            break;
        }
        else if(strchr("c", *ptr)){ // CHAR
            type = FORMAT_CHAR;
            break;
        }
        else if(strchr("s", *ptr)){
            type = FORMAT_STRING;
            break;
        }
        else if(*ptr == '%'){
            type = FORMAT_LITERAL;
            if(x){ // buffer-shrinking hack
                char *p1 = ptr, *p2 = ptr + 1;
                do
                    *p1++ = *p2;
                while (*p2++);
                ptr--;
            }
            break;
        }
/*        else if(strchr("l", *ptr)){ // remove longs for now
            if(modifier){
                if(x)
                    sprintf(errstring, "only single modifier is supported");
                break;
            }
            modifier = *ptr;
        }*/
        else if(*ptr == '.'){
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
            if (x)
                sprintf(errstring, "\'%c\' format character not supported", *ptr);
            break;
        }
    }
    if(*ptr)
        ptr++; // LATER rethink
    else
        if(x)
            sprintf(errstring, "type not specified");
    *patternp = ptr;
    return(type);
}

static void format_free(t_format *x){
    if (x->x_proxies){
        int i = x->x_nslots;
        while (i--){
            t_format_proxy *y = (t_format_proxy *)x->x_proxies[i];
            pd_free((t_pd *)y);
        }
        freebytes(x->x_proxies, x->x_nproxies * sizeof(*x->x_proxies));
    }
    if (x->x_fstring)
        freebytes(x->x_fstring, x->x_fsize);
}

static void *format_new(t_symbol *s, int ac, t_atom *av){
    t_format *x = (t_format *)pd_new(format_class);
    outlet_new((t_object *)x, &s_symbol);
    x->x_ignore = s;
    int fsize;
    char *fstring;
    char *p1, *p2;
    int i = 1, nslots, nproxies = 0;
    t_pd **proxies;
    fstring = makename_getstring(ac, av, &fsize);
    p1 = fstring;
    while((p2 = strchr(p1, '%'))){
        int type;
        p1 = p2 + 1;
        type = format_get_type(0, &p1);
        if(type >= FORMAT_MINSLOTTYPE)
            nproxies++;
    }
    if(!nproxies){ // no arguments creates with an inlet and prints errors
        x->x_nslots = 0;
        x->x_nproxies = 0;
        x->x_proxies = 0;
        x->x_fsize = fsize;
        x->x_fstring = fstring;
        p1 = fstring;
        while ((p2 = strchr(p1, '%'))){
            p1 = p2 + 1;
            format_get_type(x, &p1);
        };
        return (x);
    }
    if (!(proxies = (t_pd **)getbytes(nproxies * sizeof(*proxies)))){
        freebytes(fstring, fsize);
        return(0);
    }
    for (nslots = 0; nslots < nproxies; nslots++)
        if (!(proxies[nslots] = pd_new(format_proxy_class))) break;
    if (!nslots){
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
        type = format_get_type(x, &p1);
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
                if(i) // creates inlets for valid '%'
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
        (t_method)format_free, sizeof(t_format), 0, A_GIMME, 0);
    class_addlist(format_class, format_list);
    class_addanything(format_class, format_anything);
    format_proxy_class = class_new(gensym("_format_proxy"), 0, 0,
        sizeof(t_format_proxy), CLASS_PD | CLASS_NOINLET, 0);
    class_addfloat(format_proxy_class, format_proxy_float);
    class_addsymbol(format_proxy_class, format_proxy_symbol);
    class_addanything(format_proxy_class, format_proxy_anything);
}

