#pragma once

#include <stdint.h>
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
    char* platform;
} t_link_discovery_data;

t_link_handle link_init(const char* user_data, const char* platform, int local, uint64_t application_id);

void link_free(t_link_handle link_handle);

void link_discover(t_link_handle link_handle);

int link_get_num_peers(t_link_handle link_handle);

t_link_discovery_data link_get_discovered_peer_data(t_link_handle link_handle, int idx);

int link_connect(t_link_handle link_handle, int port, const char* ip);

void link_send(t_link_handle link_handle, size_t size, const char* data);

void link_receive(t_link_handle link_handle, void* object, void(*callback)(void*, size_t, const char*));

int link_isconnected(t_link_handle link_handle);

const char* link_get_own_ip(t_link_handle link_handle);

int link_get_own_port(t_link_handle link_handle);

void link_ping(t_link_handle link_handle, void* object, void(*connection_lost_callback)(void*, int));

#ifdef __cplusplus
}
#endif
