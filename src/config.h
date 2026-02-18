#ifndef AWG_CONFIG_H
#define AWG_CONFIG_H

#include <stdint.h>
#include <stddef.h>

/* Security limits */
#define AWG_MAX_LOGIN_ATTEMPTS 5
#define AWG_BLOCK_DURATION_SEC 900
#define AWG_SESSION_TOKEN_LEN 48
#define AWG_REQ_MAX (64 * 1024)
#define AWG_BODY_MAX (8 * 1024)
#define AWG_FILE_MAX (1024 * 1024)
#define AWG_RATE_LIMIT_WINDOW_SEC 300

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
