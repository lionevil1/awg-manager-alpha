#include "config.h"
#include "hash.h"
#include "session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return 1; \
        } \
    } while (0)

#define ASSERT_INT_EQ(expected, actual) \
    do { \
        int _e = (expected); \
        int _a = (actual); \
        if (_e != _a) { \
            fprintf(stderr, \
                    "ASSERT_INT_EQ failed at %s:%d: expected=%d actual=%d\n", \
                    __FILE__, \
                    __LINE__, \
                    _e, \
                    _a); \
            return 1; \
        } \
    } while (0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        const char *_e = (expected); \
        const char *_a = (actual); \
        if (strcmp(_e, _a) != 0) { \
            fprintf(stderr, \
                    "ASSERT_STR_EQ failed at %s:%d: expected='%s' actual='%s'\n", \
                    __FILE__, \
                    __LINE__, \
                    _e, \
                    _a); \
            return 1; \
        } \
    } while (0)

static int test_hash_vectors(void) {
    char md5_hex[33];
    char sha_hex[65];

    ASSERT_INT_EQ(0, awg_md5_hex((const unsigned char *)"", 0, md5_hex));
    ASSERT_STR_EQ("d41d8cd98f00b204e9800998ecf8427e", md5_hex);

    ASSERT_INT_EQ(0, awg_md5_hex((const unsigned char *)"abc", 3, md5_hex));
    ASSERT_STR_EQ("900150983cd24fb0d6963f7d28e17f72", md5_hex);

    ASSERT_INT_EQ(0, awg_sha256_hex((const unsigned char *)"", 0, sha_hex));
    ASSERT_STR_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", sha_hex);

    ASSERT_INT_EQ(0, awg_sha256_hex((const unsigned char *)"abc", 3, sha_hex));
    ASSERT_STR_EQ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", sha_hex);

    ASSERT_INT_EQ(-1, awg_md5_hex(NULL, 0, md5_hex));
    ASSERT_INT_EQ(-1, awg_sha256_hex(NULL, 0, sha_hex));

    return 0;
}

static int test_session_lifecycle(void) {
    awg_session_store store;
    char token[AWG_SESSION_TOKEN_LEN + 1];
    char small[8];

    awg_session_store_init(&store);

    ASSERT_INT_EQ(-1, awg_session_create(&store, 60, small, sizeof(small)));
    ASSERT_INT_EQ(0, awg_session_create(&store, 60, token, sizeof(token)));
    ASSERT_TRUE(token[0] != '\0');
    ASSERT_INT_EQ(1, awg_session_validate(&store, token));

    awg_session_revoke(&store, token);
    ASSERT_INT_EQ(0, awg_session_validate(&store, token));

    return 0;
}

static int test_session_expiry_and_capacity(void) {
    awg_session_store store;
    char token[AWG_SESSION_TOKEN_LEN + 1];
    int i = 0;

    awg_session_store_init(&store);
    ASSERT_INT_EQ(0, awg_session_create(&store, 1, token, sizeof(token)));
    sleep(2);
    ASSERT_INT_EQ(0, awg_session_validate(&store, token));

    awg_session_store_init(&store);
    for (i = 0; i < 128; i++) {
        ASSERT_INT_EQ(0, awg_session_create(&store, 60, token, sizeof(token)));
    }
    ASSERT_INT_EQ(-1, awg_session_create(&store, 60, token, sizeof(token)));

    return 0;
}

static int test_config_env_overrides(void) {
    awg_config cfg;

    setenv("AWG_LISTEN_ADDR", "127.0.0.1", 1);
    setenv("AWG_LISTEN_PORT", "18088", 1);
    setenv("AWG_ROUTER_ADDR", "192.168.7.1", 1);
    setenv("AWG_ROUTER_PORT", "8080", 1);
    setenv("AWG_WEB_ROOT", "/opt/share/awg-manager-alpha/www", 1);
    setenv("AWG_SESSION_TTL", "120", 1);

    awg_config_init_runtime(&cfg);
    ASSERT_STR_EQ("127.0.0.1", cfg.listen_addr);
    ASSERT_INT_EQ(18088, (int)cfg.listen_port);
    ASSERT_STR_EQ("192.168.7.1", cfg.router_addr);
    ASSERT_INT_EQ(8080, (int)cfg.router_port);
    ASSERT_STR_EQ("/opt/share/awg-manager-alpha/www", cfg.web_root);
    ASSERT_INT_EQ(120, (int)cfg.session_ttl_sec);

    setenv("AWG_SESSION_TTL", "20", 1);
    awg_config_init_runtime(&cfg);
    ASSERT_INT_EQ(1800, (int)cfg.session_ttl_sec);

    unsetenv("AWG_LISTEN_ADDR");
    unsetenv("AWG_LISTEN_PORT");
    unsetenv("AWG_ROUTER_ADDR");
    unsetenv("AWG_ROUTER_PORT");
    unsetenv("AWG_WEB_ROOT");
    unsetenv("AWG_SESSION_TTL");

    return 0;
}

static int test_constant_time_compare(void) {
    const char *a = "test_token_12345";
    const char *b = "test_token_12345";
    const char *c = "test_token_67890";
    const char *d = "short";

    /* Equal strings */
    ASSERT_INT_EQ(0, awg_constant_time_compare(a, b, strlen(a)));

    /* Different strings */
    ASSERT_INT_EQ(-1, awg_constant_time_compare(a, c, strlen(a)));

    /* Different lengths - compare only common part */
    ASSERT_INT_EQ(-1, awg_constant_time_compare(a, d, strlen(d)));

    /* NULL handling tested in hash vectors */

    return 0;
}

static int test_ipv4_validation(void) {
    awg_config cfg;

    /* Valid IPv4 should be accepted */
    setenv("AWG_LISTEN_ADDR", "192.168.1.1", 1);
    awg_config_init_runtime(&cfg);
    ASSERT_STR_EQ("192.168.1.1", cfg.listen_addr);

    /* Invalid IPv4 should be rejected (keep default) */
    setenv("AWG_LISTEN_ADDR", "invalid", 1);
    awg_config_init_runtime(&cfg);
    ASSERT_STR_EQ("0.0.0.0", cfg.listen_addr);

    /* Empty should be rejected (keep default) */
    setenv("AWG_LISTEN_ADDR", "", 1);
    awg_config_init_runtime(&cfg);
    ASSERT_STR_EQ("0.0.0.0", cfg.listen_addr);

    unsetenv("AWG_LISTEN_ADDR");

    return 0;
}

static int run_test(const char *name, int (*fn)(void)) {
    int rc = fn();
    if (rc == 0) {
        fprintf(stdout, "[ok] %s\n", name);
        return 0;
    }

    fprintf(stderr, "[fail] %s\n", name);
    return 1;
}

int main(void) {
    int fails = 0;

    fails += run_test("hash vectors", test_hash_vectors);
    fails += run_test("session lifecycle", test_session_lifecycle);
    fails += run_test("session expiry/capacity", test_session_expiry_and_capacity);
    fails += run_test("config env overrides", test_config_env_overrides);
    fails += run_test("constant-time compare", test_constant_time_compare);
    fails += run_test("IPv4 validation", test_ipv4_validation);

    if (fails != 0) {
        fprintf(stderr, "unit tests failed: %d\n", fails);
        return 1;
    }

    fprintf(stdout, "all unit tests passed\n");
    return 0;
}
