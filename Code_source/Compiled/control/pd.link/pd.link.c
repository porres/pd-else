/**
 * include the interface to Pd
 */

#include "link.h"
#include "m_pd.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static t_class *pdlink_class;

typedef struct _pdlink {
    t_object  x_obj;
    t_int x_local;
    t_int x_debug;
    t_int x_loopcount;
    t_symbol* x_name;
    t_link_handle x_link;
    t_clock* x_clock;
    t_outlet* x_outlet;
} t_pdlink;

void pdlink_anything(t_pdlink *x, t_symbol *s, int argc, t_atom *argv) {
    t_atom symbol;
    SETSYMBOL(&symbol, s);

    t_binbuf *binbuf = binbuf_new();
    binbuf_add(binbuf, 1, &symbol);
    if(argc) {
        binbuf_add(binbuf, argc, argv);
    }

    char* buf;
    int len;
    binbuf_gettext(binbuf, &buf, &len);

    // Send the data
    link_send(x->x_link, (size_t)len, buf);
    binbuf_free(binbuf);
}

void pdlink_receive(void *x, size_t len, const char* message) {
    t_binbuf *binbuf = binbuf_new();
    binbuf_text(binbuf, message, len);
    int argc = binbuf_getnatom(binbuf);
    t_atom* argv = binbuf_getvec(binbuf);

    // Call the outlet with the deserialized data
    outlet_anything(((t_pdlink*)x)->x_outlet, atom_getsymbol(&argv[0]), argc - 1, &argv[1]);
    binbuf_free(binbuf);
}

void pdlink_receive_loop(t_pdlink *x)
{
    if((x->x_loopcount & 7) == 0) {
        link_run_discovery_loop(x->x_link);
        int num_peers = link_get_num_peers(x->x_link);
        for(int i = 0; i < num_peers; i++)
        {
            t_link_discovery_data data = link_get_discovered_peer_data(x->x_link, i);
            if(strcmp(data.sndrcv, x->x_name->s_name) == 0) {
                //if(x->x_local && strcmp(data.ip, "127.0.0.1") != 0) continue;
                //if(!x->x_local && strcmp(data.ip, "127.0.0.1") == 0) continue;
                int created = link_connect(x->x_link, data.port, data.ip);
                if(created && x->x_debug)
                {
                    post("Connected to:\n%s\n%s : %i\n%s", data.hostname, data.ip, data.port, data.sndrcv);
                }
            }
            free(data.hostname);
            free(data.sndrcv);
            free(data.ip);
        }
    }

    if(link_isconnected(x->x_link)) link_receive(x->x_link, x, pdlink_receive);
    clock_delay(x->x_clock, 5);
    x->x_loopcount++;
}


void pdlink_free(t_pdlink *x)
{
    if(x->x_link) link_cleanup(x->x_link);
    if(x->x_clock) clock_free(x->x_clock);
}

void *pdlink_new(t_symbol *s, int argc, t_atom *argv)
{
    t_pdlink *x = (t_pdlink *)pd_new(pdlink_class);
    x->x_name = NULL;
    x->x_link = NULL;
    x->x_clock = NULL;
    x->x_local = 0;
    x->x_debug = 0;
    x->x_loopcount = 0;

    for (int i = 0; i < argc; i++) {
        if (argv[i].a_type == A_SYMBOL) {
            t_symbol *sym = atom_getsymbol(&argv[i]);
            if (strcmp(sym->s_name, "-local") == 0) {
                x->x_local = 1;
            } else if (strcmp(sym->s_name, "-debug") == 0) {
                x->x_debug = 1;
            } else if (x->x_name == NULL) {
                // Assign the first non-flag symbol to x_name
                x->x_name = sym;
            }
        }
    }

    if (x->x_name == NULL) {
        pd_error(NULL, "[pdlink]: No name argument specified");
        pd_free((t_pd*)x);
        return NULL;
    }

    x->x_link = link_init(x->x_name->s_name, x->x_local);
    x->x_clock = clock_new(x, (t_method)pdlink_receive_loop);

    x->x_outlet = outlet_new((t_object*)x, 0);
    clock_delay(x->x_clock, 0);
    return (void *)x;
}

void setup_pd0x2elink(void) {
    pdlink_class = class_new(gensym("pd.link"),
                             (t_newmethod)pdlink_new,
                             (t_method)pdlink_free,
                             sizeof(t_pdlink),
                             CLASS_DEFAULT,
                             A_GIMME, 0);


    class_addanything(pdlink_class, pdlink_anything);
}
