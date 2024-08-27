#pragma once

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef void* t_link_handle;

typedef struct
{
    char* sndrcv;
    char* ip;
    int port;
    char* hostname;
} t_link_discovery_data;

t_link_handle link_init(const char* user_data, int local);

void link_cleanup(t_link_handle link_handle);

void link_run_discovery_loop(t_link_handle link_handle);

int link_get_num_peers(t_link_handle link_handle);

t_link_discovery_data link_get_discovered_peer_data(t_link_handle link_handle, int idx);

int link_connect(t_link_handle link_handle, int port, const char* ip);

void link_send(t_link_handle link_handle, size_t size, const char* data);

void link_receive(t_link_handle link_handle, void* object, void(*callback)(void*, size_t, const char*));

int link_isconnected(t_link_handle link_handle);

#ifdef __cplusplus
}
#endif
