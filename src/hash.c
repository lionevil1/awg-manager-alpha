#include "hash.h"

#include <stdint.h>
#include <string.h>

/*
 * Small self-contained MD5 and SHA-256 implementations for portability.
 * Intended only for protocol compatibility, not for new cryptographic design.
 */

static void to_hex(const uint8_t *in, size_t in_len, char *out, size_t out_len) {
    static const char hex[] = "0123456789abcdef";
    size_t i = 0;
    if (out_len < in_len * 2 + 1) {
        return;
    }
    for (i = 0; i < in_len; i++) {
        out[i * 2] = hex[(in[i] >> 4) & 0x0fU];
        out[i * 2 + 1] = hex[in[i] & 0x0fU];
    }
    out[in_len * 2] = '\0';
}

/* ---------------- MD5 ---------------- */

typedef struct {
    uint32_t h[4];
    uint64_t total_len;
    uint8_t buf[64];
    size_t buf_len;
} md5_ctx;

static uint32_t md5_rol(uint32_t x, uint32_t n) {
    return (x << n) | (x >> (32U - n));
}

static void md5_init(md5_ctx *c) {
    c->h[0] = 0x67452301U;
    c->h[1] = 0xefcdab89U;
    c->h[2] = 0x98badcfeU;
    c->h[3] = 0x10325476U;
    c->total_len = 0;
    c->buf_len = 0;
}

static void md5_process(md5_ctx *c, const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU, 0xf57c0fafU, 0x4787c62aU,
        0xa8304613U, 0xfd469501U, 0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU,
        0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U, 0xf61e2562U, 0xc040b340U,
        0x265e5a51U, 0xe9b6c7aaU, 0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
        0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU, 0xa9e3e905U, 0xfcefa3f8U,
        0x676f02d9U, 0x8d2a4c8aU, 0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU,
        0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U, 0x289b7ec6U, 0xeaa127faU,
        0xd4ef3085U, 0x04881d05U, 0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
        0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U, 0x655b59c3U, 0x8f0ccc92U,
        0xffeff47dU, 0x85845dd1U, 0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
        0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U,
    };
    static const uint32_t s[64] = {
        7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U, 7U, 12U, 17U, 22U,
        5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U, 5U, 9U, 14U, 20U,
        4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U, 4U, 11U, 16U, 23U,
        6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U, 6U, 10U, 15U, 21U,
    };
    uint32_t a = c->h[0], b = c->h[1], d = c->h[3], f = 0, g = 0;
    uint32_t cc = c->h[2];
    uint32_t m[16];
    size_t i;

    for (i = 0; i < 16; i++) {
        m[i] = (uint32_t)block[i * 4] | ((uint32_t)block[i * 4 + 1] << 8) |
               ((uint32_t)block[i * 4 + 2] << 16) | ((uint32_t)block[i * 4 + 3] << 24);
    }

    for (i = 0; i < 64; i++) {
        uint32_t tmp;
        if (i < 16) {
            f = (b & cc) | ((~b) & d);
            g = (uint32_t)i;
        } else if (i < 32) {
            f = (d & b) | ((~d) & cc);
            g = (uint32_t)((5 * i + 1) % 16);
        } else if (i < 48) {
            f = b ^ cc ^ d;
            g = (uint32_t)((3 * i + 5) % 16);
        } else {
            f = cc ^ (b | (~d));
            g = (uint32_t)((7 * i) % 16);
        }
        tmp = d;
        d = cc;
        cc = b;
        b = b + md5_rol(a + f + k[i] + m[g], s[i]);
        a = tmp;
    }

    c->h[0] += a;
    c->h[1] += b;
    c->h[2] += cc;
    c->h[3] += d;
}

static void md5_update(md5_ctx *c, const uint8_t *data, size_t len) {
    size_t off = 0;
    if (len == 0) {
        return;
    }
    c->total_len += len;
    if (c->buf_len > 0) {
        size_t need = 64 - c->buf_len;
        if (need > len) {
            need = len;
        }
        memcpy(c->buf + c->buf_len, data, need);
        c->buf_len += need;
        off += need;
        if (c->buf_len == 64) {
            md5_process(c, c->buf);
            c->buf_len = 0;
        }
    }
    while (off + 64 <= len) {
        md5_process(c, data + off);
        off += 64;
    }
    if (off < len) {
        c->buf_len = len - off;
        memcpy(c->buf, data + off, c->buf_len);
    }
}

static void md5_final(md5_ctx *c, uint8_t out[16]) {
    uint64_t bits = c->total_len * 8U;
    size_t i;
    c->buf[c->buf_len++] = 0x80U;
    if (c->buf_len > 56) {
        while (c->buf_len < 64) {
            c->buf[c->buf_len++] = 0;
        }
        md5_process(c, c->buf);
        c->buf_len = 0;
    }
    while (c->buf_len < 56) {
        c->buf[c->buf_len++] = 0;
    }
    for (i = 0; i < 8; i++) {
        c->buf[56 + i] = (uint8_t)((bits >> (8U * i)) & 0xffU);
    }
    md5_process(c, c->buf);
    for (i = 0; i < 4; i++) {
        out[i * 4] = (uint8_t)(c->h[i] & 0xffU);
        out[i * 4 + 1] = (uint8_t)((c->h[i] >> 8) & 0xffU);
        out[i * 4 + 2] = (uint8_t)((c->h[i] >> 16) & 0xffU);
        out[i * 4 + 3] = (uint8_t)((c->h[i] >> 24) & 0xffU);
    }
}

/* ---------------- SHA-256 ---------------- */

typedef struct {
    uint32_t h[8];
    uint64_t total_len;
    uint8_t buf[64];
    size_t buf_len;
} sha256_ctx;

static uint32_t rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32U - n));
}

static void sha256_init(sha256_ctx *c) {
    c->h[0] = 0x6a09e667U;
    c->h[1] = 0xbb67ae85U;
    c->h[2] = 0x3c6ef372U;
    c->h[3] = 0xa54ff53aU;
    c->h[4] = 0x510e527fU;
    c->h[5] = 0x9b05688cU;
    c->h[6] = 0x1f83d9abU;
    c->h[7] = 0x5be0cd19U;
    c->total_len = 0;
    c->buf_len = 0;
}

static void sha256_process(sha256_ctx *c, const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
        0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
        0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
        0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
        0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
        0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
    };
    uint32_t w[64];
    uint32_t a, b, cc, d, e, f, g, h;
    size_t i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7U) ^ rotr32(w[i - 15], 18U) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17U) ^ rotr32(w[i - 2], 19U) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = c->h[0];
    b = c->h[1];
    cc = c->h[2];
    d = c->h[3];
    e = c->h[4];
    f = c->h[5];
    g = c->h[6];
    h = c->h[7];

    for (i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(e, 6U) ^ rotr32(e, 11U) ^ rotr32(e, 25U);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + s1 + ch + k[i] + w[i];
        uint32_t s0 = rotr32(a, 2U) ^ rotr32(a, 13U) ^ rotr32(a, 22U);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = cc;
        cc = b;
        b = a;
        a = t1 + t2;
    }

    c->h[0] += a;
    c->h[1] += b;
    c->h[2] += cc;
    c->h[3] += d;
    c->h[4] += e;
    c->h[5] += f;
    c->h[6] += g;
    c->h[7] += h;
}

static void sha256_update(sha256_ctx *c, const uint8_t *data, size_t len) {
    size_t off = 0;
    if (len == 0) {
        return;
    }
    c->total_len += len;
    if (c->buf_len > 0) {
        size_t need = 64 - c->buf_len;
        if (need > len) {
            need = len;
        }
        memcpy(c->buf + c->buf_len, data, need);
        c->buf_len += need;
        off += need;
        if (c->buf_len == 64) {
            sha256_process(c, c->buf);
            c->buf_len = 0;
        }
    }
    while (off + 64 <= len) {
        sha256_process(c, data + off);
        off += 64;
    }
    if (off < len) {
        c->buf_len = len - off;
        memcpy(c->buf, data + off, c->buf_len);
    }
}

static void sha256_final(sha256_ctx *c, uint8_t out[32]) {
    uint64_t bits = c->total_len * 8U;
    size_t i;

    c->buf[c->buf_len++] = 0x80U;
    if (c->buf_len > 56) {
        while (c->buf_len < 64) {
            c->buf[c->buf_len++] = 0;
        }
        sha256_process(c, c->buf);
        c->buf_len = 0;
    }
    while (c->buf_len < 56) {
        c->buf[c->buf_len++] = 0;
    }
    for (i = 0; i < 8; i++) {
        c->buf[63 - i] = (uint8_t)((bits >> (8U * i)) & 0xffU);
    }
    sha256_process(c, c->buf);

    for (i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)((c->h[i] >> 24) & 0xffU);
        out[i * 4 + 1] = (uint8_t)((c->h[i] >> 16) & 0xffU);
        out[i * 4 + 2] = (uint8_t)((c->h[i] >> 8) & 0xffU);
        out[i * 4 + 3] = (uint8_t)(c->h[i] & 0xffU);
    }
}

int awg_md5_hex(const unsigned char *data, size_t len, char out_hex[33]) {
    md5_ctx c;
    uint8_t d[16];

    if (data == NULL || out_hex == NULL) {
        return -1;
    }
    md5_init(&c);
    md5_update(&c, data, len);
    md5_final(&c, d);
    to_hex(d, sizeof(d), out_hex, 33);
    memset(&c, 0, sizeof(c));
    memset(d, 0, sizeof(d));
    return 0;
}

int awg_sha256_hex(const unsigned char *data, size_t len, char out_hex[65]) {
    sha256_ctx c;
    uint8_t d[32];

    if (data == NULL || out_hex == NULL) {
        return -1;
    }
    sha256_init(&c);
    sha256_update(&c, data, len);
    sha256_final(&c, d);
    to_hex(d, sizeof(d), out_hex, 65);
    memset(&c, 0, sizeof(c));
    memset(d, 0, sizeof(d));
    return 0;
}
