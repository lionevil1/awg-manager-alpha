#include "config.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static void apply_default_values(awg_config *cfg) {
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->listen_addr, sizeof(cfg->listen_addr), "%s", "0.0.0.0");
    cfg->listen_port = 8088;
    snprintf(cfg->router_addr, sizeof(cfg->router_addr), "%s", "192.168.1.1");
    cfg->router_port = 80;
    snprintf(cfg->web_root, sizeof(cfg->web_root), "%s", "/opt/share/awg-manager-alpha/www");
    cfg->session_ttl_sec = 1800;
}

static int parse_gateway_hex_to_ip(const char *gateway_hex, char *out_ip, size_t out_ip_sz) {
    unsigned int b1 = 0;
    unsigned int b2 = 0;
    unsigned int b3 = 0;
    unsigned int b4 = 0;

    if (gateway_hex == NULL || out_ip == NULL || out_ip_sz == 0) {
        return -1;
    }

    if (strlen(gateway_hex) != 8) {
        return -1;
    }

    if (sscanf(gateway_hex, "%2x%2x%2x%2x", &b4, &b3, &b2, &b1) != 4) {
        return -1;
    }

    snprintf(out_ip, out_ip_sz, "%u.%u.%u.%u", b1, b2, b3, b4);
    return 0;
}

static int detect_iface_ipv4(const char *ifname, char *out_ip, size_t out_ip_sz) {
    int fd = -1;
    struct ifreq ifr;
    struct sockaddr_in *sin = NULL;

    if (ifname == NULL || out_ip == NULL || out_ip_sz == 0) {
        return -1;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

    if (ioctl(fd, SIOCGIFADDR, &ifr) != 0) {
        close(fd);
        return -1;
    }

    sin = (struct sockaddr_in *)&ifr.ifr_addr;
    if (inet_ntop(AF_INET, &sin->sin_addr, out_ip, out_ip_sz) == NULL) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int detect_router_lan_ip(char *out_ip, size_t out_ip_sz) {
    static const char *ifaces[] = {"br0", "br-lan", "lan", "bridge0"};
    size_t i = 0;

    if (out_ip == NULL || out_ip_sz == 0) {
        return -1;
    }

    for (i = 0; i < (sizeof(ifaces) / sizeof(ifaces[0])); i++) {
        if (detect_iface_ipv4(ifaces[i], out_ip, out_ip_sz) == 0) {
            return 0;
        }
    }
    return -1;
}

static int detect_router_gateway_ip(char *out_ip, size_t out_ip_sz) {
    FILE *fp = NULL;
    char line[512];

    if (out_ip == NULL || out_ip_sz == 0) {
        return -1;
    }

    fp = fopen("/proc/net/route", "r");
    if (fp == NULL) {
        return -1;
    }

    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char iface[64];
        char destination_hex[16];
        char gateway_hex[16];
        unsigned int flags = 0;

        if (sscanf(line, "%63s %15s %15s %x", iface, destination_hex, gateway_hex, &flags) == 4) {
            if (strcmp(destination_hex, "00000000") == 0 && (flags & 0x2U) != 0U) {
                fclose(fp);
                return parse_gateway_hex_to_ip(gateway_hex, out_ip, out_ip_sz);
            }
        }
    }

    fclose(fp);
    return -1;
}

static void apply_env_overrides(awg_config *cfg) {
    const char *v = NULL;

    if (cfg == NULL) {
        return;
    }

    v = getenv("AWG_LISTEN_ADDR");
    if (v != NULL && v[0] != '\0') {
        snprintf(cfg->listen_addr, sizeof(cfg->listen_addr), "%s", v);
    }

    v = getenv("AWG_LISTEN_PORT");
    if (v != NULL && v[0] != '\0') {
        long p = strtol(v, NULL, 10);
        if (p > 0 && p <= 65535) {
            cfg->listen_port = (uint16_t)p;
        }
    }

    v = getenv("AWG_ROUTER_ADDR");
    if (v != NULL && v[0] != '\0') {
        snprintf(cfg->router_addr, sizeof(cfg->router_addr), "%s", v);
    }

    v = getenv("AWG_ROUTER_PORT");
    if (v != NULL && v[0] != '\0') {
        long p = strtol(v, NULL, 10);
        if (p > 0 && p <= 65535) {
            cfg->router_port = (uint16_t)p;
        }
    }

    v = getenv("AWG_WEB_ROOT");
    if (v != NULL && v[0] != '\0') {
        snprintf(cfg->web_root, sizeof(cfg->web_root), "%s", v);
    }

    v = getenv("AWG_SESSION_TTL");
    if (v != NULL && v[0] != '\0') {
        long t = strtol(v, NULL, 10);
        if (t >= 60 && t <= 86400) {
            cfg->session_ttl_sec = (uint32_t)t;
        }
    }
}

void awg_config_init_runtime(awg_config *cfg) {
    if (cfg == NULL) {
        return;
    }

    apply_default_values(cfg);
    if (detect_router_lan_ip(cfg->router_addr, sizeof(cfg->router_addr)) != 0) {
        detect_router_gateway_ip(cfg->router_addr, sizeof(cfg->router_addr));
    }
    apply_env_overrides(cfg);
}
