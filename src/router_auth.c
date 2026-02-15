#include "router_auth.h"
#include "hash.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct {
    int status;
    char realm[128];
    char challenge[256];
    char cookie[256];
} auth_probe;

static int tcp_connect(const char *host, uint16_t port) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *it = NULL;
    char port_str[16];
    int fd = -1;
    int rc = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);
    rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0) {
        return -1;
    }

    for (it = res; it != NULL; it = it->ai_next) {
        int flags = 0;
        int rc_connect = 0;
        int so_err = 0;
        socklen_t so_len = sizeof(so_err);
        struct timeval tv;
        fd_set wfds;

        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) {
            continue;
        }

        flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            flags = 0;
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
            close(fd);
            fd = -1;
            continue;
        }

        rc_connect = connect(fd, it->ai_addr, it->ai_addrlen);
        if (rc_connect == 0) {
            if (flags >= 0) {
                (void)fcntl(fd, F_SETFL, flags);
            }
            break;
        }

        if (rc_connect < 0 && errno == EINPROGRESS) {
            FD_ZERO(&wfds);
            FD_SET(fd, &wfds);
            tv.tv_sec = 3;
            tv.tv_usec = 0;

            rc_connect = select(fd + 1, NULL, &wfds, NULL, &tv);
            if (rc_connect > 0 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_err, &so_len) == 0 &&
                so_err == 0) {
                if (flags >= 0) {
                    (void)fcntl(fd, F_SETFL, flags);
                }
                break;
            }
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);

    if (fd >= 0) {
        struct timeval io_tv;
        io_tv.tv_sec = 3;
        io_tv.tv_usec = 0;
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &io_tv, sizeof(io_tv));
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &io_tv, sizeof(io_tv));
    }

    return fd;
}

static int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n <= 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static int recv_to_end(int fd, char **out, size_t *out_len, size_t max_len) {
    char tmp[2048];
    char *buf = NULL;
    size_t len = 0;

    if (out == NULL || out_len == NULL) {
        return -1;
    }

    for (;;) {
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
        if (n == 0) {
            break;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            free(buf);
            return -1;
        }
        if (len + (size_t)n > max_len) {
            free(buf);
            return -1;
        }
        {
            char *new_buf = realloc(buf, len + (size_t)n + 1);
            if (new_buf == NULL) {
                free(buf);
                return -1;
            }
            buf = new_buf;
            memcpy(buf + len, tmp, (size_t)n);
            len += (size_t)n;
            buf[len] = '\0';
        }
    }

    *out = buf;
    *out_len = len;
    return 0;
}

static char *find_header(char *headers, const char *name) {
    size_t name_len = strlen(name);
    char *line = headers;

    while (line != NULL && *line != '\0') {
        char *next = strstr(line, "\r\n");
        if (next == NULL) {
            break;
        }
        if ((size_t)(next - line) > name_len + 1 && strncasecmp(line, name, name_len) == 0 &&
            line[name_len] == ':') {
            char *v = line + name_len + 1;
            while (*v == ' ' || *v == '\t') {
                v++;
            }
            return v;
        }
        line = next + 2;
    }
    return NULL;
}

static void copy_header_value(char *dst, size_t dst_sz, const char *src_line) {
    size_t i = 0;

    if (dst == NULL || dst_sz == 0) {
        return;
    }
    dst[0] = '\0';
    if (src_line == NULL) {
        return;
    }

    while (src_line[i] != '\0' && src_line[i] != '\r' && src_line[i] != '\n' && i + 1 < dst_sz) {
        dst[i] = src_line[i];
        i++;
    }
    dst[i] = '\0';
}

static void trim_cookie(char *cookie) {
    char *semi = NULL;

    if (cookie == NULL) {
        return;
    }
    semi = strchr(cookie, ';');
    if (semi != NULL) {
        *semi = '\0';
    }
}

static int parse_status(const char *resp) {
    int status = 0;
    if (resp == NULL) {
        return 0;
    }
    if (sscanf(resp, "HTTP/%*s %d", &status) != 1) {
        return 0;
    }
    return status;
}

static int probe_auth(const char *host, uint16_t port, auth_probe *out_probe) {
    int fd = -1;
    char req[512];
    char *resp = NULL;
    size_t resp_len = 0;
    char *headers = NULL;
    char *p = NULL;

    if (host == NULL || out_probe == NULL) {
        return -1;
    }

    memset(out_probe, 0, sizeof(*out_probe));
    fd = tcp_connect(host, port);
    if (fd < 0) {
        return -1;
    }

    snprintf(req,
             sizeof(req),
             "GET /auth HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "Accept: */*\r\n\r\n",
             host);

    if (send_all(fd, req, strlen(req)) != 0 || recv_to_end(fd, &resp, &resp_len, 128 * 1024) != 0) {
        close(fd);
        free(resp);
        return -1;
    }
    close(fd);

    out_probe->status = parse_status(resp);

    p = strstr(resp, "\r\n\r\n");
    if (p == NULL) {
        free(resp);
        return -1;
    }
    *p = '\0';
    headers = resp;

    copy_header_value(out_probe->realm, sizeof(out_probe->realm), find_header(headers, "X-NDM-Realm"));
    copy_header_value(out_probe->challenge,
                      sizeof(out_probe->challenge),
                      find_header(headers, "X-NDM-Challenge"));
    copy_header_value(out_probe->cookie, sizeof(out_probe->cookie), find_header(headers, "Set-Cookie"));
    trim_cookie(out_probe->cookie);

    free(resp);
    return 0;
}

static int post_auth(const char *host,
                     uint16_t port,
                     const char *cookie,
                     const char *login,
                     const char *password_hash_hex) {
    int fd = -1;
    char json[512];
    char req[2048];
    char *resp = NULL;
    size_t resp_len = 0;
    int status = 0;

    fd = tcp_connect(host, port);
    if (fd < 0) {
        return -1;
    }

    snprintf(json, sizeof(json), "{\"login\":\"%s\",\"password\":\"%s\"}", login, password_hash_hex);
    snprintf(req,
             sizeof(req),
             "POST /auth HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "Accept: */*\r\n"
             "Content-Type: application/json\r\n"
             "Cookie: %s\r\n"
             "Content-Length: %zu\r\n\r\n"
             "%s",
             host,
             cookie == NULL ? "" : cookie,
             strlen(json),
             json);

    if (send_all(fd, req, strlen(req)) != 0 || recv_to_end(fd, &resp, &resp_len, 128 * 1024) != 0) {
        close(fd);
        free(resp);
        return -1;
    }
    close(fd);

    status = parse_status(resp);
    free(resp);
    return status;
}

int awg_router_verify_credentials(const char *router_host,
                                  uint16_t router_port,
                                  const char *login,
                                  const char *password) {
    auth_probe probe;
    char md5_input[512];
    char md5_hex_val[33];
    char sha_input[512];
    char sha_hex_val[65];
    int status = 0;

    if (router_host == NULL || login == NULL || password == NULL || login[0] == '\0' ||
        password[0] == '\0') {
        return 0;
    }

    if (probe_auth(router_host, router_port, &probe) != 0) {
        return 0;
    }

    if (probe.status == 200) {
        return 1;
    }
    if (probe.status != 401 || probe.realm[0] == '\0' || probe.challenge[0] == '\0') {
        return 0;
    }

    snprintf(md5_input, sizeof(md5_input), "%s:%s:%s", login, probe.realm, password);
    if (awg_md5_hex((const unsigned char *)md5_input, strlen(md5_input), md5_hex_val) != 0) {
        memset(md5_input, 0, sizeof(md5_input));
        return 0;
    }

    snprintf(sha_input, sizeof(sha_input), "%s%s", probe.challenge, md5_hex_val);
    if (awg_sha256_hex((const unsigned char *)sha_input, strlen(sha_input), sha_hex_val) != 0) {
        memset(md5_input, 0, sizeof(md5_input));
        memset(md5_hex_val, 0, sizeof(md5_hex_val));
        memset(sha_input, 0, sizeof(sha_input));
        return 0;
    }

    status = post_auth(router_host, router_port, probe.cookie, login, sha_hex_val);

    memset(md5_input, 0, sizeof(md5_input));
    memset(md5_hex_val, 0, sizeof(md5_hex_val));
    memset(sha_input, 0, sizeof(sha_input));
    memset(sha_hex_val, 0, sizeof(sha_hex_val));

    return status == 200;
}
