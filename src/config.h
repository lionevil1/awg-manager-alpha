#ifndef AWG_CONFIG_H
#define AWG_CONFIG_H

#include <stdint.h>

typedef struct {
    char listen_addr[64];
    uint16_t listen_port;
    char router_addr[64];
    uint16_t router_port;
    char web_root[256];
    uint32_t session_ttl_sec;
} awg_config;

void awg_config_init_runtime(awg_config *cfg);

#endif
