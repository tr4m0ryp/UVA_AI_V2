#include "api/upstream/upstream.h"
#include "core/config/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

static proxy_config_t g_cfg;
static int g_initialized = 0;

void buffer_init(buffer_t *buf)
{
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

void buffer_free(buffer_t *buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

int buffer_append(buffer_t *buf, const char *data, size_t len)
{
    if (buf->size + len + 1 > buf->capacity) {
        size_t new_cap = (buf->capacity == 0) ? 4096 : buf->capacity * 2;
        while (new_cap < buf->size + len + 1)
            new_cap *= 2;
        char *tmp = realloc(buf->data, new_cap);
        if (!tmp) return -1;
        buf->data = tmp;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
    return 0;
}

int upstream_init(const proxy_config_t *cfg)
{
    if (g_initialized) return 0;
    memcpy(&g_cfg, cfg, sizeof(g_cfg));
    curl_global_init(CURL_GLOBAL_ALL);
    g_initialized = 1;
    return 0;
}

void upstream_cleanup(void)
{
    if (!g_initialized) return;
    curl_global_cleanup();
    g_initialized = 0;
}

static size_t write_buffer_cb(void *ptr, size_t size, size_t nmemb,
                              void *userdata)
{
    buffer_t *buf = (buffer_t *)userdata;
    size_t total = size * nmemb;
    if (buffer_append(buf, (const char *)ptr, total) < 0)
        return 0;
    return total;
}

typedef struct {
    stream_callback_t cb;
    void             *userdata;
} stream_ctx_t;

static size_t write_stream_cb(void *ptr, size_t size, size_t nmemb,
                               void *userdata)
{
    stream_ctx_t *ctx = (stream_ctx_t *)userdata;
    size_t total = size * nmemb;
    return ctx->cb((const char *)ptr, total, ctx->userdata);
}

static CURL *make_curl_ex(const char *url, const char *cookie,
                          struct curl_slist **hdrs_out)
{
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    struct curl_slist *hdrs = NULL;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    hdrs = curl_slist_append(hdrs,
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:138.0) "
        "Gecko/20100101 Firefox/138.0");

    /* Set session cookie */
    if (cookie && cookie[0]) {
        curl_easy_setopt(curl, CURLOPT_COOKIE, cookie);
    }

    *hdrs_out = hdrs;
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    return curl;
}

static CURL *make_curl(const char *url, struct curl_slist **hdrs_out)
{
    return make_curl_ex(url, g_cfg.session_cookie, hdrs_out);
}

static int do_chat(const char *body, size_t body_len, const char *cookie,
                   stream_callback_t stream_cb, void *cb_data,
                   buffer_t *response)
{
    char url[MAX_URL_LEN + 64];
    snprintf(url, sizeof(url), "%s/api/v1/chat", g_cfg.base_url);

    struct curl_slist *hdrs = NULL;
    CURL *curl = make_curl_ex(url, cookie, &hdrs);
    if (!curl) return -1;

    if (stream_cb) {
        hdrs = curl_slist_append(hdrs, "Accept: text/event-stream");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
    }

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body_len);

    stream_ctx_t sctx;
    if (stream_cb) {
        sctx.cb = stream_cb;
        sctx.userdata = cb_data;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_stream_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sctx);
    } else {
        buffer_init(response);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_buffer_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "upstream chat: curl error: %s\n",
                curl_easy_strerror(res));
        return -1;
    }
    return (int)http_code;
}

int upstream_chat(const char *body, size_t body_len,
                  stream_callback_t stream_cb, void *cb_data,
                  buffer_t *response)
{
    return do_chat(body, body_len, g_cfg.session_cookie,
                   stream_cb, cb_data, response);
}

int upstream_server_action(const char *action_id,
                           const char *body, size_t body_len,
                           buffer_t *response)
{
    struct curl_slist *hdrs = NULL;
    CURL *curl = make_curl(g_cfg.base_url, &hdrs);
    if (!curl) return -1;

    /* Server action headers */
    char action_hdr[128];
    snprintf(action_hdr, sizeof(action_hdr), "Next-Action: %s", action_id);
    hdrs = curl_slist_append(hdrs, action_hdr);

    /* Override content type for server actions */
    hdrs = curl_slist_append(hdrs,
        "Content-Type: text/plain;charset=UTF-8");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body_len);

    buffer_init(response);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_buffer_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "upstream_server_action: curl error: %s\n",
                curl_easy_strerror(res));
        return -1;
    }

    return (int)http_code;
}

int upstream_session_valid(void)
{
    char email[256], name[256];
    return upstream_validate_cookie(g_cfg.session_cookie,
                                    email, sizeof(email),
                                    name, sizeof(name));
}

int upstream_chat_with_cookie(const char *body, size_t body_len,
                              const char *cookie,
                              stream_callback_t stream_cb, void *cb_data,
                              buffer_t *response)
{
    return do_chat(body, body_len, cookie, stream_cb, cb_data, response);
}

int upstream_validate_cookie(const char *cookie,
                             char *email_out, size_t email_size,
                             char *name_out, size_t name_size)
{
    if (!cookie || !cookie[0]) return 0;

    char url[MAX_URL_LEN + 64];
    snprintf(url, sizeof(url), "%s/api/auth/session", g_cfg.base_url);

    struct curl_slist *hdrs = NULL;
    CURL *curl = make_curl_ex(url, cookie, &hdrs);
    if (!curl) return 0;

    buffer_t resp;
    buffer_init(&resp);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_buffer_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);

    int valid = 0;
    if (res == CURLE_OK && http_code == 200 && resp.data) {
        struct json_object *root = json_tokener_parse(resp.data);
        if (root) {
            struct json_object *user_obj;
            if (json_object_object_get_ex(root, "user", &user_obj)) {
                struct json_object *e, *n;
                if (json_object_object_get_ex(user_obj, "email", &e)) {
                    const char *ev = json_object_get_string(e);
                    if (ev && email_out)
                        snprintf(email_out, email_size, "%s", ev);
                    valid = 1;
                }
                if (json_object_object_get_ex(user_obj, "name", &n)) {
                    const char *nv = json_object_get_string(n);
                    if (nv && name_out)
                        snprintf(name_out, name_size, "%s", nv);
                }
            }
            json_object_put(root);
        }
    }

    buffer_free(&resp);
    return valid;
}
