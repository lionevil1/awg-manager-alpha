#include "session.h"
#include "hash.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char k_b64url[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static int random_bytes(unsigned char *buf, size_t len) {
    int fd = -1;
    ssize_t got = 0;
    size_t total = 0;

    if (buf == NULL || len == 0) {
        return -1;
    }

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    while (total < len) {
        got = read(fd, buf + total, len - total);
        if (got <= 0) {
            close(fd);
            return -1;
        }
        total += (size_t)got;
    }

    close(fd);
    return 0;
}

static int make_token(char *out, size_t out_size) {
    unsigned char rnd[AWG_SESSION_TOKEN_LEN];
    size_t i = 0;

    if (out == NULL || out_size < AWG_SESSION_TOKEN_LEN + 1) {
        return -1;
    }
    if (random_bytes(rnd, sizeof(rnd)) != 0) {
        return -1;
    }

    for (i = 0; i < AWG_SESSION_TOKEN_LEN; i++) {
        out[i] = k_b64url[rnd[i] & 0x3fU];
    }
    out[AWG_SESSION_TOKEN_LEN] = '\0';
    return 0;
}

void awg_session_store_init(awg_session_store *store) {
    if (store == NULL) {
        return;
    }
    memset(store, 0, sizeof(*store));
}

void awg_session_prune(awg_session_store *store) {
    time_t now = time(NULL);
    size_t i = 0;

    if (store == NULL) {
        return;
    }

    for (i = 0; i < (sizeof(store->slots) / sizeof(store->slots[0])); i++) {
        if (store->slots[i].active && store->slots[i].expires_at <= now) {
            memset(&store->slots[i], 0, sizeof(store->slots[i]));
        }
    }
}

int awg_session_create(awg_session_store *store, int ttl_sec, char *out_token, size_t out_size) {
    size_t i = 0;
    time_t now = time(NULL);

    if (store == NULL || out_token == NULL || ttl_sec <= 0 || out_size < AWG_SESSION_TOKEN_LEN + 1) {
        return -1;
    }

    awg_session_prune(store);

    for (i = 0; i < (sizeof(store->slots) / sizeof(store->slots[0])); i++) {
        if (!store->slots[i].active) {
            if (make_token(store->slots[i].token, sizeof(store->slots[i].token)) != 0) {
                return -1;
            }
            store->slots[i].active = 1;
            store->slots[i].expires_at = now + ttl_sec;
            snprintf(out_token, out_size, "%s", store->slots[i].token);
            return 0;
        }
    }

    return -1;
}

int awg_session_validate(awg_session_store *store, const char *token) {
    size_t i = 0;
    time_t now = time(NULL);

    if (store == NULL || token == NULL || token[0] == '\0') {
        return 0;
    }

    for (i = 0; i < (sizeof(store->slots) / sizeof(store->slots[0])); i++) {
        if (store->slots[i].active && 
            awg_constant_time_compare(store->slots[i].token, token, AWG_SESSION_TOKEN_LEN) == 0) {
            if (store->slots[i].expires_at <= now) {
                memset(&store->slots[i], 0, sizeof(store->slots[i]));
                return 0;
            }
            return 1;
        }
    }

    return 0;
}

void awg_session_revoke(awg_session_store *store, const char *token) {
    size_t i = 0;

    if (store == NULL || token == NULL || token[0] == '\0') {
        return;
    }

    for (i = 0; i < (sizeof(store->slots) / sizeof(store->slots[0])); i++) {
        if (store->slots[i].active && strcmp(store->slots[i].token, token) == 0) {
            memset(&store->slots[i], 0, sizeof(store->slots[i]));
            return;
        }
    }
}
