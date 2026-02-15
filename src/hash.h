#ifndef AWG_HASH_H
#define AWG_HASH_H

#include <stddef.h>

int awg_md5_hex(const unsigned char *data, size_t len, char out_hex[33]);
int awg_sha256_hex(const unsigned char *data, size_t len, char out_hex[65]);

#endif
