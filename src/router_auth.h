#ifndef AWG_ROUTER_AUTH_H
#define AWG_ROUTER_AUTH_H

#include <stdint.h>

int awg_router_verify_credentials(const char *router_host,
                                  uint16_t router_port,
                                  const char *login,
                                  const char *password);

#endif
