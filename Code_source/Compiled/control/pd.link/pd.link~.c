/**
 * include the interface to Pd
 */

#include "link.h"
#include "m_pd.h"
#include "magic.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static t_class *pdlink_tilde_class;

#define MAX_SEND 16

typedef struct _pdlink_tilde {
    t_object  x_obj;
    t_glist* x_glist;
    t_int x_local;
    t_int x_debug;
    t_int x_delay;
    t_symbol* x_name;
    t_link_handle x_link;
    t_clock* x_clock;
    t_outlet* x_outlet;
    t_float x_signal_buffer[MAX_SEND][4096];
    t_int x_buf_write_pos[MAX_SEND];
    t_int x_buf_read_pos[MAX_SEND];
    t_int x_receiver_ports[MAX_SEND];
    t_int x_used_receiver_ports[MAX_SEND];
} t_pdlink_tilde;

int pdlink_tilde_get_signal_idx(t_pdlink_tilde *x, int port) {
    for(int i = 0; i < MAX_SEND; i++)
    {
         if(x->x_receiver_ports[i] == port)
         {
             return i;
         }
    }

    for(int i = 0; i < MAX_SEND; i++)
    {
        if(x->x_receiver_ports[i] == 0)
        {
            x->x_receiver_ports[i] = port;
            return i;
        }
    }

    return 0;
}

void pdlink_tilde_receive(void *ptr, size_t len, const char* message) {
    t_pdlink_tilde *x = (t_pdlink_tilde *)ptr;
    int num_float = (len - 32) / 4;
    const t_float* samples = (const t_float*)(message + 32);
    int port = pdlink_tilde_get_signal_idx(x, atoi(message));
    if(port < 0 || port > MAX_SEND) return;
    x->x_used_receiver_ports[port] = 1;

    for(int i = 0; i < num_float; i++)
    {
        x->x_signal_buffer[port][x->x_buf_write_pos[port]] = samples[i];
        x->x_buf_write_pos[port] = (x->x_buf_write_pos[port] + 1) % 4096;
    }
}

// Receive callback for messages
static t_int *pdlink_tilde_perform(t_int *w){
    t_pdlink_tilde *x = (t_pdlink_tilde *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);

    // Format signal message (port number + audio data)
    char message_buf[288];
    snprintf(message_buf, 32, "%i", link_get_own_port(x->x_link));
    memcpy(message_buf + 32, in1, nblock * sizeof(t_float));

    // Don't send if there's no inlet connection
    int connected = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    if(connected) link_send(x->x_link, 288, message_buf);

    link_receive(x->x_link, x, pdlink_tilde_receive);

    memset(out1, 0, nblock*sizeof(t_float));
    for(int i = 0; i < MAX_SEND; i++) {
        if(!x->x_used_receiver_ports[i]) continue;
        for(int n = 0; n < nblock; n++)
        {
            if(x->x_buf_write_pos[i] == x->x_buf_read_pos[i])
            {
                x->x_used_receiver_ports[i] = 0;
                break;
            }
            out1[n] += x->x_signal_buffer[i][x->x_buf_read_pos[i]];
            x->x_buf_read_pos[i] = (x->x_buf_read_pos[i] + 1) % 4096;
        }
    }

    return w+5;
}

void pdlink_tilde_dsp(t_pdlink_tilde *x, t_signal **sp) {
    dsp_add(pdlink_tilde_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

void pdlink_tilde_connection_lost(void*x, int port)
{
    if(((t_pdlink_tilde*)x)->x_debug)
    {
         post("[pd.link~]: connection lost: %i", port);
    }
}

// Discovery and message retrieval loop
void pdlink_tilde_discover_loop(t_pdlink_tilde *x)
{
    clock_delay(x->x_clock, 400);

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
               post("[pd.link~]: connected to:\n%s\n%s:%i\n%s", data.hostname, data.ip, data.port, data.platform);
            }
        }
        if(data.hostname) free(data.hostname);
        if(data.sndrcv) free(data.sndrcv);
        if(data.platform) free(data.platform);
        if(data.ip) free(data.ip);
    }

    // Send ping to connected servers, and remove connections that we have not received a ping from in the last 1.5 seconds
    link_ping(x->x_link, x, pdlink_tilde_connection_lost);
}

void pdlink_tilde_free(t_pdlink_tilde *x)
{
    if(x->x_link) link_free(x->x_link);
    if(x->x_clock) clock_free(x->x_clock);
}

void *pdlink_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_pdlink_tilde *x = (t_pdlink_tilde *)pd_new(pdlink_tilde_class);
    x->x_name = NULL;
    x->x_link = NULL;
    x->x_clock = NULL;
    x->x_local = 0;
    x->x_debug = 0;
    x->x_delay = 1024;
    x->x_glist = canvas_getcurrent();

    for (int i = 0; i < argc; i++) {
        if (argv[i].a_type == A_SYMBOL) {
            t_symbol *sym = atom_getsymbol(&argv[i]);
            if (strcmp(sym->s_name, "-local") == 0) {
                x->x_local = 1; // Localhost only connection
            } else if (strcmp(sym->s_name, "-debug") == 0) {
                x->x_debug = 1; // Enable debug logging
            } else if(strcmp(sym->s_name, "-bufsize") == 0 && i < argc-1 && argv[i+1].a_type == A_FLOAT) {
                i++;
                x->x_delay = atom_getfloat(argv + i);
            }
            else if (x->x_name == NULL) {
                // Assign the first non-flag symbol to x_name
                x->x_name = sym;
            }
        }
    }

    if (x->x_name == NULL) {
        pd_error(NULL, "[pd.link~]: No name argument specified");
        pd_free((t_pd*)x);
        return NULL;
    }


    for(int i = 0; i < MAX_SEND; i++)
    {
        x->x_buf_write_pos[i] = x->x_delay;
        x->x_buf_read_pos[i] = 0;
        x->x_receiver_ports[i] = 0;
        x->x_used_receiver_ports[i] = 0;
        memset(x->x_signal_buffer[i], 0, 4096 * sizeof(t_float));
    }

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
    // Initialise link and loop clock
    x->x_link = link_init(x->x_name->s_name, pd_platform, x->x_local, 7680413);
    if(!x->x_link)
    {
        pd_error(x, "[pd.link~]: failed to bind server socket");
        pd_free((t_pd*)x);
        return NULL;
    }
    x->x_clock = clock_new(x, (t_method)pdlink_tilde_discover_loop);
    x->x_outlet = outlet_new((t_object*)x, &s_signal);
    clock_delay(x->x_clock, 0);

    if(x->x_debug)
    {
        post("[pd.link~]: own IP:\n%s:%i", link_get_own_ip(x->x_link), link_get_own_port(x->x_link));
    }
    return (void *)x;
}

void setup_pd0x2elink_tilde(void) {
    pdlink_tilde_class = class_new(gensym("pd.link~"),
                             (t_newmethod)pdlink_tilde_new,
                             (t_method)pdlink_tilde_free,
                             sizeof(t_pdlink_tilde),
                             CLASS_DEFAULT,
                             A_GIMME, 0);

    class_addmethod(pdlink_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(pdlink_tilde_class, (t_method)pdlink_tilde_dsp, gensym("dsp"), 0);
}
