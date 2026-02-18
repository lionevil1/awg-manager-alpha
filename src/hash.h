#ifndef AWG_HASH_H
#define AWG_HASH_H

#include <stddef.h>

int awg_md5_hex(const unsigned char *data, size_t len, char out_hex[33]);
int awg_sha256_hex(const unsigned char *data, size_t len, char out_hex[65]);

/* Constant-time comparison for security-sensitive data */
int awg_constant_time_compare(const void *a, const void *b, size_t len);

#endif
