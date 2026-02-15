#ifndef AWG_SESSION_H
#define AWG_SESSION_H

#include <stddef.h>
#include <time.h>

#define AWG_SESSION_TOKEN_LEN 48

typedef struct {
    int active;
    char token[AWG_SESSION_TOKEN_LEN + 1];
    time_t expires_at;
} awg_session_item;

typedef struct {
    awg_session_item slots[128];
} awg_session_store;

void awg_session_store_init(awg_session_store *store);
int awg_session_create(awg_session_store *store, int ttl_sec, char *out_token, size_t out_size);
int awg_session_validate(awg_session_store *store, const char *token);
void awg_session_revoke(awg_session_store *store, const char *token);
void awg_session_prune(awg_session_store *store);

#endif
