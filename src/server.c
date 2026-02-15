#include "server.h"

#include "router_auth.h"
#include "session.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define AWG_REQ_MAX (64 * 1024)
#define AWG_BODY_MAX (8 * 1024)
#define AWG_FILE_MAX (1024 * 1024)
#define AWG_KERNEL_MODULE_NAME "amneziawg"

static volatile sig_atomic_t g_stop = 0;

typedef struct {
    char method[8];
    char path[256];
    char headers[4096];
    const char *body;
    size_t body_len;
} http_req;

static void handle_sig(int sig) {
    (void)sig;
    g_stop = 1;
}

static const char *ci_strstr(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    const char *h = haystack;

    if (haystack == NULL || needle == NULL || nlen == 0) {
        return NULL;
    }

    while (*h != '\0') {
        if (strncasecmp(h, needle, nlen) == 0) {
            return h;
        }
        h++;
    }
    return NULL;
}

static void sanitize_request_path(const char *in, char *out, size_t out_size) {
    size_t i = 0;

    if (out == NULL || out_size == 0) {
        return;
    }

    out[0] = '/';
    out[1] = '\0';

    if (in == NULL || in[0] == '\0') {
        return;
    }

    while (in[i] != '\0' && in[i] != '?' && in[i] != '#' && i + 1 < out_size) {
        out[i] = in[i];
        i++;
    }
    out[i] = '\0';

    if (out[0] == '\0') {
        out[0] = '/';
        out[1] = '\0';
    }
}

static int path_has_suffix(const char *path, const char *suffix) {
    size_t plen = 0;
    size_t slen = 0;

    if (path == NULL || suffix == NULL) {
        return 0;
    }

    plen = strlen(path);
    slen = strlen(suffix);
    if (slen > plen) {
        return 0;
    }

    return strcmp(path + plen - slen, suffix) == 0;
}

static const char *guess_content_type(const char *path) {
    if (path_has_suffix(path, ".html")) {
        return "text/html; charset=utf-8";
    }
    if (path_has_suffix(path, ".css")) {
        return "text/css; charset=utf-8";
    }
    if (path_has_suffix(path, ".js")) {
        return "application/javascript; charset=utf-8";
    }
    if (path_has_suffix(path, ".json")) {
        return "application/json; charset=utf-8";
    }
    if (path_has_suffix(path, ".svg")) {
        return "image/svg+xml";
    }
    if (path_has_suffix(path, ".png")) {
        return "image/png";
    }
    if (path_has_suffix(path, ".jpg") || path_has_suffix(path, ".jpeg")) {
        return "image/jpeg";
    }
    if (path_has_suffix(path, ".ico")) {
        return "image/x-icon";
    }
    return "application/octet-stream";
}

static int is_safe_web_rel_path(const char *rel_path) {
    size_t i = 0;

    if (rel_path == NULL || rel_path[0] == '\0') {
        return 0;
    }
    if (strstr(rel_path, "..") != NULL || strchr(rel_path, '\\') != NULL) {
        return 0;
    }

    for (i = 0; rel_path[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)rel_path[i];
        if (isalnum(ch) || ch == '/' || ch == '-' || ch == '_' || ch == '.') {
            continue;
        }
        return 0;
    }

    return 1;
}

static int build_web_path(const awg_config *cfg, const char *rel_path, char *out, size_t out_size) {
    int n = 0;

    if (cfg == NULL || rel_path == NULL || out == NULL || out_size == 0) {
        return -1;
    }
    if (!is_safe_web_rel_path(rel_path)) {
        return -1;
    }

    n = snprintf(out, out_size, "%s/%s", cfg->web_root, rel_path);
    if (n < 0 || (size_t)n >= out_size) {
        return -1;
    }

    return 0;
}

static int read_file_limited(const char *path, char **out, size_t *out_len, size_t max_len) {
    FILE *fp = NULL;
    char chunk[4096];
    char *buf = NULL;
    size_t len = 0;

    if (path == NULL || out == NULL || out_len == NULL) {
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }

    for (;;) {
        size_t n = fread(chunk, 1, sizeof(chunk), fp);
        if (n == 0) {
            break;
        }
        if (len + n > max_len) {
            fclose(fp);
            free(buf);
            return -1;
        }

        {
            char *new_buf = realloc(buf, len + n + 1);
            if (new_buf == NULL) {
                fclose(fp);
                free(buf);
                return -1;
            }
            buf = new_buf;
        }

        memcpy(buf + len, chunk, n);
        len += n;
    }

    if (ferror(fp) != 0) {
        fclose(fp);
        free(buf);
        return -1;
    }

    fclose(fp);

    if (buf == NULL) {
        buf = malloc(1);
        if (buf == NULL) {
            return -1;
        }
        buf[0] = '\0';
        len = 0;
    } else {
        buf[len] = '\0';
    }

    *out = buf;
    *out_len = len;
    return 0;
}

static int send_all(int fd, const char *buf, size_t len) {
    size_t off = 0;

    while (off < len) {
        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        off += (size_t)n;
    }

    return 0;
}

static int send_response(int fd,
                         int status,
                         const char *status_text,
                         const char *content_type,
                         const char *extra_headers,
                         const void *body,
                         size_t body_len) {
    char head[4096];
    int n = 0;

    if (content_type == NULL) {
        content_type = "text/plain; charset=utf-8";
    }

    n = snprintf(head,
                 sizeof(head),
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "%s"
                 "\r\n",
                 status,
                 status_text,
                 content_type,
                 body_len,
                 extra_headers == NULL ? "" : extra_headers);

    if (n < 0 || (size_t)n >= sizeof(head)) {
        return -1;
    }

    if (send_all(fd, head, (size_t)n) != 0) {
        return -1;
    }

    if (body_len > 0 && body != NULL) {
        if (send_all(fd, (const char *)body, body_len) != 0) {
            return -1;
        }
    }

    return 0;
}

static int send_response_text(int fd,
                              int status,
                              const char *status_text,
                              const char *content_type,
                              const char *extra_headers,
                              const char *body_text) {
    size_t len = body_text == NULL ? 0 : strlen(body_text);
    return send_response(fd, status, status_text, content_type, extra_headers, body_text, len);
}

static int send_web_file(int fd, const awg_config *cfg, const char *rel_path) {
    char fs_path[512];
    char *data = NULL;
    size_t data_len = 0;
    const char *extra = NULL;
    int rc = 0;

    if (build_web_path(cfg, rel_path, fs_path, sizeof(fs_path)) != 0) {
        return send_response_text(fd, 403, "Forbidden", NULL, NULL, "forbidden");
    }

    if (read_file_limited(fs_path, &data, &data_len, AWG_FILE_MAX) != 0) {
        return send_response_text(fd, 404, "Not Found", NULL, NULL, "not found");
    }

    if (path_has_suffix(rel_path, ".html")) {
        extra = "Cache-Control: no-store\r\n";
    }

    rc = send_response(fd,
                       200,
                       "OK",
                       guess_content_type(rel_path),
                       extra,
                       data,
                       data_len);

    free(data);
    return rc;
}

static int parse_request(char *raw, size_t raw_len, http_req *out) {
    char *hdr_end = NULL;
    char *line_end = NULL;
    size_t header_len = 0;
    size_t headers_only_len = 0;
    int body_len = 0;

    if (raw == NULL || out == NULL || raw_len == 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    hdr_end = strstr(raw, "\r\n\r\n");
    if (hdr_end == NULL) {
        return -1;
    }

    header_len = (size_t)(hdr_end - raw) + 4;
    if (header_len > sizeof(out->headers) - 1) {
        return -1;
    }

    line_end = strstr(raw, "\r\n");
    if (line_end == NULL) {
        return -1;
    }

    {
        size_t req_line_len = (size_t)(line_end - raw);
        char req_line[320];

        if (req_line_len >= sizeof(req_line)) {
            return -1;
        }

        memcpy(req_line, raw, req_line_len);
        req_line[req_line_len] = '\0';

        if (sscanf(req_line, "%7s %255s", out->method, out->path) != 2) {
            return -1;
        }
    }

    headers_only_len = (size_t)(hdr_end - (line_end + 2));
    if (headers_only_len >= sizeof(out->headers)) {
        return -1;
    }

    memcpy(out->headers, line_end + 2, headers_only_len);
    out->headers[headers_only_len] = '\0';

    {
        const char *cl = ci_strstr(out->headers, "Content-Length:");
        if (cl != NULL) {
            cl += strlen("Content-Length:");
            while (*cl == ' ' || *cl == '\t') {
                cl++;
            }
            body_len = atoi(cl);
            if (body_len < 0 || body_len > AWG_BODY_MAX) {
                return -1;
            }
        }
    }

    if (header_len + (size_t)body_len > raw_len) {
        return -1;
    }

    out->body = raw + header_len;
    out->body_len = (size_t)body_len;

    return 0;
}

static int bind_listen(const awg_config *cfg) {
    int fd = -1;
    int yes = 1;
    struct timeval tv;
    struct sockaddr_in sa;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(cfg->listen_port);
    if (inet_pton(AF_INET, cfg->listen_addr, &sa.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 16) != 0) {
        close(fd);
        return -1;
    }

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    return fd;
}

static int url_decode(const char *src, size_t src_len, char *dst, size_t dst_sz) {
    size_t i = 0;
    size_t o = 0;

    if (dst == NULL || dst_sz == 0) {
        return -1;
    }

    while (i < src_len && o + 1 < dst_sz) {
        if (src[i] == '+') {
            dst[o++] = ' ';
            i++;
        } else if (src[i] == '%' && i + 2 < src_len && isxdigit((unsigned char)src[i + 1]) &&
                   isxdigit((unsigned char)src[i + 2])) {
            char h[3] = {src[i + 1], src[i + 2], '\0'};
            dst[o++] = (char)strtol(h, NULL, 16);
            i += 3;
        } else {
            dst[o++] = src[i++];
        }
    }

    dst[o] = '\0';
    return 0;
}

static int form_get_field(const char *body,
                          size_t body_len,
                          const char *key,
                          char *out,
                          size_t out_sz) {
    size_t key_len = strlen(key);
    size_t i = 0;

    if (body == NULL || key == NULL || out == NULL || out_sz == 0) {
        return -1;
    }

    while (i < body_len) {
        size_t start = i;
        size_t end = i;

        while (end < body_len && body[end] != '&') {
            end++;
        }

        {
            const char *eq = memchr(body + start, '=', end - start);
            if (eq != NULL) {
                size_t klen = (size_t)(eq - (body + start));
                if (klen == key_len && strncmp(body + start, key, key_len) == 0) {
                    const char *val = eq + 1;
                    size_t vlen = end - (size_t)(val - body);
                    return url_decode(val, vlen, out, out_sz);
                }
            }
        }

        i = end + 1;
    }

    out[0] = '\0';
    return -1;
}

static int cookie_get_token(const char *headers, char *out, size_t out_sz) {
    const char *h = NULL;
    const char *v = NULL;
    const char *end = NULL;

    if (headers == NULL || out == NULL || out_sz == 0) {
        return -1;
    }

    h = ci_strstr(headers, "Cookie:");
    if (h == NULL) {
        return -1;
    }

    v = strstr(h, "awg_session=");
    if (v == NULL) {
        return -1;
    }

    v += strlen("awg_session=");
    end = v;
    while (*end != '\0' && *end != ';' && *end != '\r' && *end != '\n') {
        end++;
    }

    if ((size_t)(end - v) >= out_sz) {
        return -1;
    }

    memcpy(out, v, (size_t)(end - v));
    out[end - v] = '\0';

    return 0;
}

static int request_has_valid_session(const http_req *req, awg_session_store *sessions) {
    char token[AWG_SESSION_TOKEN_LEN + 1];

    if (req == NULL || sessions == NULL) {
        return 0;
    }

    token[0] = '\0';
    if (cookie_get_token(req->headers, token, sizeof(token)) != 0) {
        return 0;
    }

    return awg_session_validate(sessions, token);
}

static int kernel_module_is_loaded(const char *module_name) {
    FILE *fp = NULL;
    char line[256];
    size_t name_len = 0;

    if (module_name == NULL || module_name[0] == '\0') {
        return -1;
    }

    fp = fopen("/proc/modules", "r");
    if (fp == NULL) {
        return -1;
    }

    name_len = strlen(module_name);
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, module_name, name_len) == 0 &&
            isspace((unsigned char)line[name_len])) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

static void process_client(int cfd, const awg_config *cfg, awg_session_store *sessions) {
    char req_buf[AWG_REQ_MAX + 1];
    size_t total = 0;
    size_t need_total = 0;
    http_req req;
    int header_seen = 0;
    char route_path[256];
    struct timeval io_tv;

    io_tv.tv_sec = 5;
    io_tv.tv_usec = 0;
    (void)setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &io_tv, sizeof(io_tv));
    (void)setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, &io_tv, sizeof(io_tv));

    for (;;) {
        ssize_t n = recv(cfd, req_buf + total, sizeof(req_buf) - 1 - total, 0);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }

        if (n == 0) {
            break;
        }

        total += (size_t)n;
        if (total >= sizeof(req_buf) - 1) {
            send_response_text(cfd, 413, "Payload Too Large", NULL, NULL, "too large");
            return;
        }

        req_buf[total] = '\0';

        if (!header_seen) {
            char *hdr_end = strstr(req_buf, "\r\n\r\n");
            if (hdr_end != NULL) {
                int cl = 0;
                size_t header_len = (size_t)(hdr_end - req_buf) + 4;
                const char *cl_hdr = ci_strstr(req_buf, "Content-Length:");

                header_seen = 1;

                if (cl_hdr != NULL && cl_hdr < hdr_end) {
                    cl_hdr += strlen("Content-Length:");
                    while (*cl_hdr == ' ' || *cl_hdr == '\t') {
                        cl_hdr++;
                    }
                    cl = atoi(cl_hdr);
                    if (cl < 0 || cl > AWG_BODY_MAX) {
                        send_response_text(cfd,
                                           400,
                                           "Bad Request",
                                           NULL,
                                           NULL,
                                           "bad content length");
                        return;
                    }
                }

                need_total = header_len + (size_t)cl;
            }
        }

        if (header_seen && total >= need_total) {
            break;
        }
    }

    if (parse_request(req_buf, total, &req) != 0) {
        send_response_text(cfd, 400, "Bad Request", NULL, NULL, "bad request");
        return;
    }

    sanitize_request_path(req.path, route_path, sizeof(route_path));

    if (strcmp(req.method, "GET") == 0 && strcmp(route_path, "/") == 0) {
        (void)send_web_file(cfd, cfg, "login.html");
        return;
    }

    if (strcmp(req.method, "GET") == 0 && strncmp(route_path, "/assets/", 8) == 0) {
        (void)send_web_file(cfd, cfg, route_path + 1);
        return;
    }

    if (strcmp(req.method, "POST") == 0 && strcmp(route_path, "/api/login") == 0) {
        char user[128];
        char pass[256];
        char token[AWG_SESSION_TOKEN_LEN + 1];
        char headers[512];
        int ok = 0;

        user[0] = '\0';
        pass[0] = '\0';
        token[0] = '\0';

        form_get_field(req.body, req.body_len, "username", user, sizeof(user));
        form_get_field(req.body, req.body_len, "password", pass, sizeof(pass));

        if (user[0] != '\0' && pass[0] != '\0') {
            ok = awg_router_verify_credentials(cfg->router_addr, cfg->router_port, user, pass);
        }

        memset(user, 0, sizeof(user));
        memset(pass, 0, sizeof(pass));

        if (!ok) {
            send_response_text(cfd, 401, "Unauthorized", NULL, NULL, "auth failed");
            return;
        }

        if (awg_session_create(sessions, (int)cfg->session_ttl_sec, token, sizeof(token)) != 0) {
            send_response_text(cfd,
                               503,
                               "Service Unavailable",
                               NULL,
                               NULL,
                               "session store full");
            return;
        }

        snprintf(headers,
                 sizeof(headers),
                 "Set-Cookie: awg_session=%s; HttpOnly; SameSite=Strict; Path=/; Max-Age=%u\r\n",
                 token,
                 (unsigned)cfg->session_ttl_sec);

        (void)send_response_text(cfd,
                                 200,
                                 "OK",
                                 "text/plain; charset=utf-8",
                                 headers,
                                 "ok");
        return;
    }

    if (strcmp(req.method, "GET") == 0 && strcmp(route_path, "/app") == 0) {
        if (request_has_valid_session(&req, sessions)) {
            (void)send_web_file(cfd, cfg, "app.html");
            return;
        }

        send_response_text(cfd, 302, "Found", NULL, "Location: /\r\n", "");
        return;
    }

    if (strcmp(req.method, "GET") == 0 && strcmp(route_path, "/api/kernel-status") == 0) {
        int loaded = 0;

        if (!request_has_valid_session(&req, sessions)) {
            send_response_text(cfd,
                               401,
                               "Unauthorized",
                               "application/json; charset=utf-8",
                               NULL,
                               "{\"error\":\"unauthorized\"}");
            return;
        }

        loaded = kernel_module_is_loaded(AWG_KERNEL_MODULE_NAME);
        if (loaded < 0) {
            send_response_text(cfd,
                               503,
                               "Service Unavailable",
                               "application/json; charset=utf-8",
                               "Cache-Control: no-store\r\n",
                               "{\"module\":\"amneziawg\",\"loaded\":false,\"error\":\"read_failed\"}");
            return;
        }

        if (loaded) {
            send_response_text(cfd,
                               200,
                               "OK",
                               "application/json; charset=utf-8",
                               "Cache-Control: no-store\r\n",
                               "{\"module\":\"amneziawg\",\"loaded\":true}");
        } else {
            send_response_text(cfd,
                               200,
                               "OK",
                               "application/json; charset=utf-8",
                               "Cache-Control: no-store\r\n",
                               "{\"module\":\"amneziawg\",\"loaded\":false}");
        }
        return;
    }

    if (strcmp(req.method, "POST") == 0 && strcmp(route_path, "/api/logout") == 0) {
        char token[AWG_SESSION_TOKEN_LEN + 1];
        token[0] = '\0';

        if (cookie_get_token(req.headers, token, sizeof(token)) == 0) {
            awg_session_revoke(sessions, token);
        }

        (void)send_response_text(cfd,
                                 200,
                                 "OK",
                                 "text/plain; charset=utf-8",
                                 "Set-Cookie: awg_session=deleted; Path=/; Max-Age=0; HttpOnly; SameSite=Strict\r\n",
                                 "ok");
        return;
    }

    if (strcmp(req.method, "GET") == 0 && strcmp(route_path, "/health") == 0) {
        send_response_text(cfd, 200, "OK", "text/plain; charset=utf-8", NULL, "ok");
        return;
    }

    send_response_text(cfd, 404, "Not Found", NULL, NULL, "not found");
}

int awg_server_run(const awg_config *cfg) {
    int lfd = -1;
    awg_session_store sessions;

    if (cfg == NULL) {
        return 1;
    }

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    lfd = bind_listen(cfg);
    if (lfd < 0) {
        fprintf(stderr, "failed to bind %s:%u\n", cfg->listen_addr, (unsigned)cfg->listen_port);
        return 1;
    }

    fprintf(stdout,
            "awg-manager-alpha listening on %s:%u (router %s:%u, web %s)\n",
            cfg->listen_addr,
            (unsigned)cfg->listen_port,
            cfg->router_addr,
            (unsigned)cfg->router_port,
            cfg->web_root);

    awg_session_store_init(&sessions);

    while (!g_stop) {
        struct sockaddr_storage peer;
        socklen_t peer_len = sizeof(peer);
        int cfd = accept(lfd, (struct sockaddr *)&peer, &peer_len);

        if (cfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            break;
        }

        process_client(cfd, cfg, &sessions);
        close(cfd);
        awg_session_prune(&sessions);
    }

    close(lfd);
    return 0;
}
