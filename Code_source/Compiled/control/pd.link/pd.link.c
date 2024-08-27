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
    int* x_last_connections;
} t_pdlink;

// Send any messages that arrive at inlet
void pdlink_anything(t_pdlink *x, t_symbol *s, int argc, t_atom *argv) {
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
void pdlink_receive(void *x, size_t len, const char* message) {
    // Convert text to atoms using binbuf
    t_binbuf *binbuf = binbuf_new();
    binbuf_text(binbuf, message, len);
    int argc = binbuf_getnatom(binbuf);
    t_atom* argv = binbuf_getvec(binbuf);

    // Call the outlet with the deserialized data
    outlet_anything(((t_pdlink*)x)->x_outlet, atom_getsymbol(&argv[0]), argc - 1, &argv[1]);
    binbuf_free(binbuf);
}

// Discovery and message retrieval loop
void pdlink_receive_loop(t_pdlink *x)
{
    // Occasionally check for new devices
    if((x->x_loopcount & 127) == 0) {
        link_discover(x->x_link);
        int num_peers = link_get_num_peers(x->x_link);

        link_ping(x->x_link);

        if(x->x_debug)
        {
            int new_last_connections[128] = {0};
            int new_size = 0;
            for(int i = 0; i < 128; i++) {
                int last_connection = x->x_last_connections[i];
                if(last_connection == 0) break;
                int found = 0;
                for(int j = 0; j < num_peers; j++)
                {
                    t_link_discovery_data data = link_get_discovered_peer_data(x->x_link, j);
                    if(data.port == last_connection)
                    {
                        found = 1;
                    }
                }
                if(!found)
                {
                    post("[pd.link]: connection lost: %i", last_connection);
                }
                else {
                    new_last_connections[new_size++] = last_connection;
                }
            }
            memcpy(x->x_last_connections, new_last_connections, 128 * sizeof(int));
        }

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
                   x->x_last_connections[i] = data.port;
                   post("[pd.link]: connected to:\n%s\n%s : %i\n%s\n%s", data.hostname, data.ip, data.port, data.platform, data.sndrcv);
                }
            }
            if(data.hostname) free(data.hostname);
            if(data.sndrcv) free(data.sndrcv);
            if(data.platform) free(data.platform);
            if(data.ip) free(data.ip);
        }
    }
    // Receive messages if we're connected
    if(link_isconnected(x->x_link)) link_receive(x->x_link, x, pdlink_receive);
    clock_delay(x->x_clock, 5);
    x->x_loopcount++;
}


void pdlink_free(t_pdlink *x)
{
    if(x->x_link) link_free(x->x_link);
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
                x->x_local = 1; // Localhost only connection
            } else if (strcmp(sym->s_name, "-debug") == 0) {
                x->x_debug = 1; // Enable debug logging
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

    // Get pd platform identifier (only what's known at compile time, so any external will report pure-data)
    char pd_platform[MAXPDSTRING];
    char os[16];

    #if _WIN32
        snprintf(os, MAXPDSTRING, "Windows");
    #elif __APPLE__
        #include "TargetConditionals.h"
        #if TARGET_OS_IPHONE && TARGET_IPHONE_SIMULATOR
            snprintf(os, MAXPDSTRING, "iOS Simulator");
        #elif TARGET_OS_IPHONE
            snprintf(os, MAXPDSTRING, "iOS");
        #else
            snprintf(os, MAXPDSTRING, "macOS");
        #endif
    #elif __linux__
        snprintf(os, MAXPDSTRING, "Linux");
    #else
        snprintf(os, MAXPDSTRING, "Unknown OS");
    #endif

#if PLUGDATA
    snprintf(pd_platform, MAXPDSTRING, "plugdata %s\n%s", PD_PLUGDATA_VERSION, os);
#else
    int major = 0, minor = 0, bugfix = 0;
    sys_getversion(&major, &minor, &bugfix);
    snprintf(pd_platform, MAXPDSTRING, "pure-data %i.%i-%i\n%s", major, minor, bugfix, os);
#endif
    // Initialise link and loop clock
    x->x_link = link_init(x->x_name->s_name, pd_platform, x->x_local);
    x->x_clock = clock_new(x, (t_method)pdlink_receive_loop);
    x->x_outlet = outlet_new((t_object*)x, 0);
    clock_delay(x->x_clock, 0);

    if(x->x_debug)
    {
        x->x_last_connections = calloc(128, sizeof(int));
        post("[pd.link]: current IP:\n%s : %i", link_get_own_ip(x->x_link), link_get_own_port(x->x_link));
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
}
