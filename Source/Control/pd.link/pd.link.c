/**
 * include the interface to Pd
 */

#include "link.h"
#include <m_pd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static t_class *pdlink_class;

typedef struct _pdlink {
    t_object x_obj;
    t_int x_local;
    t_int x_debug;
    t_int x_loopcount;
    t_symbol *x_name;
    t_link_handle x_link;
    t_clock *x_clock;
    t_inlet *x_set_inlet;
    t_outlet *x_outlet;
} t_pdlink;

// Send any messages that arrive at inlet
static void pdlink_anything(t_pdlink *x, t_symbol *s, int argc, t_atom *argv) {
    if(x->x_name == gensym("")) return;

    // Format symbol and atoms into binbuf
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

    // Send the binbuf data as text
    link_send(x->x_link, (size_t)len, buf);
    binbuf_free(binbuf);
}

// Receive callback for messages
static void pdlink_receive(void *ptr, const size_t len, const char* message) {
     t_pdlink *x = (t_pdlink *)ptr;

    // Convert text to atoms using binbuf
    t_binbuf *binbuf = binbuf_new();
    binbuf_text(binbuf, message, len);
    int argc = binbuf_getnatom(binbuf);
    t_atom* argv = binbuf_getvec(binbuf);

    // Call the outlet with the deserialized data
    outlet_anything(x->x_outlet, atom_getsymbol(&argv[0]), argc - 1, &argv[1]);
    binbuf_free(binbuf);
}

static void pdlink_connection_lost(void *x, const int port)
{
    if(((t_pdlink*)x)->x_debug)
    {
         post("[pd.link]: connection lost: %i", port);
    }
}

// Discovery and message retrieval loop
static void pdlink_receive_loop(t_pdlink *x)
{
    clock_delay(x->x_clock, 5);

    // Occasionally check for new devices
    if((x->x_loopcount & 127) == 0) {
        link_discover(x->x_link);

        int num_peers = link_get_num_peers(x->x_link);
        for(int i = 0; i < num_peers; i++)
        {
            t_link_discovery_data data = link_get_discovered_peer_data(x->x_link, i);
            if(strcmp(data.sndrcv, x->x_name->s_name) == 0) {
                // Only connect to localhost in -local mode
                if(x->x_local && strcmp(data.ip, "127.0.0.1") != 0) continue;
                if(!x->x_local && strcmp(data.ip, "127.0.0.1") == 0) continue;

                int created = link_connect(x->x_link, data.port, data.ip);
                if(created && x->x_debug)
                {
                   post("[pd.link]: connected to:\n%s\n%s:%i\n%s", data.hostname, data.ip, data.port, data.platform);
                }
            }
            if(data.hostname) free(data.hostname);
            if(data.sndrcv) free(data.sndrcv);
            if(data.platform) free(data.platform);
            if(data.ip) free(data.ip);
        }

        // Send ping to connected servers, and remove connections that we have not received a ping from in the last 1.5 seconds
        link_ping(x->x_link, x, pdlink_connection_lost);
    }
    // Receive messages if we're connected
    link_receive(x->x_link, x, pdlink_receive);
    x->x_loopcount++;
}


static void pdlink_free(t_pdlink *x)
{
    if(x->x_link) link_free(x->x_link);
    if(x->x_clock) clock_free(x->x_clock);
}

static void pdlink_set(t_pdlink *x, t_symbol *s)
{
    x->x_name = s;

    // Get pd platform identifier (only what's known at compile time, so any external will report pure-data)
    char pd_platform[MAXPDSTRING];
    char os[16];

    #if _WIN32
        snprintf(os, 16, "Windows");
    #elif __APPLE__
        #include "TargetConditionals.h"
        #if TARGET_OS_IPHONE && TARGET_IPHONE_SIMULATOR
            snprintf(os, 16, "iOS Simulator");
        #elif TARGET_OS_IPHONE
            snprintf(os, 16, "iOS");
        #else
            snprintf(os, 16, "macOS");
        #endif
    #elif __linux__
        snprintf(os, 16, "Linux");
    #else
        snprintf(os, 16, "Unknown OS");
    #endif

#if PLUGDATA
    snprintf(pd_platform, MAXPDSTRING, "plugdata %s - %s", PD_PLUGDATA_VERSION, os);
#else
    int major = 0, minor = 0, bugfix = 0;
    sys_getversion(&major, &minor, &bugfix);
    snprintf(pd_platform, MAXPDSTRING, "pure-data %i.%i-%i - %s", major, minor, bugfix, os);
#endif

    if(x->x_link) link_free(x->x_link);
    x->x_link = link_init(x->x_name->s_name, pd_platform, x->x_local, 7680412);
    if(!x->x_link)
    {
        pd_error(x, "[pd.link]: failed to bind server socket");
        x->x_link = NULL; // TODO: handle this state!
    }
}

static void *pdlink_new(t_symbol *s, int argc, t_atom *argv)
{
    t_pdlink *x = (t_pdlink *)pd_new(pdlink_class);
    x->x_name = gensym("");
    x->x_link = NULL;
    x->x_clock = NULL;
    x->x_local = 0;
    x->x_debug = 0;
    x->x_loopcount = 0;

    for (int i = 0; i < argc; i++) {
        if (argv[i].a_type == A_SYMBOL) {
            t_symbol *sym = atom_getsymbol(&argv[i]);
            if (strcmp(sym->s_name, "-local") == 0) {
                x->x_local = 1; // Localhost only connection
            } else if (strcmp(sym->s_name, "-debug") == 0) {
                x->x_debug = 1; // Enable debug logging
            } else if (x->x_name == gensym("")) {
                // Assign the first non-flag symbol to x_name
                x->x_name = sym;
            }
        }
    }
    int is_valid = x->x_name != gensym("");

    pdlink_set(x, x->x_name);
    if(!x->x_link)
    {
        pd_error(x, "[pd.link]: failed to bind server socket");
        pd_free((t_pd*)x);
    }
    if(is_valid) {
        x->x_clock = clock_new(x, (t_method)pdlink_receive_loop);
        clock_delay(x->x_clock, 0);
    }

    x->x_set_inlet = inlet_new((t_object*)x, (t_pd*)x, &s_symbol, gensym("__set"));
    x->x_outlet = outlet_new((t_object*)x, 0);

    if(x->x_debug)
    {
        post("[pd.link]: own IP:\n%s:%i", link_get_own_ip(x->x_link), link_get_own_port(x->x_link));
    }
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
    class_addmethod(pdlink_class, (t_method)pdlink_set, gensym("__set"), A_SYMBOL, 0);
}
