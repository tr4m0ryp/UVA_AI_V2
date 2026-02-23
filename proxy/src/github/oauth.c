#include "github/github.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

typedef struct {
    char  *data;
    size_t size;
} buf_t;

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *ud)
{
    size_t total = size * nmemb;
    buf_t *b = (buf_t *)ud;
    char *tmp = realloc(b->data, b->size + total + 1);
    if (!tmp) return 0;
    b->data = tmp;
    memcpy(b->data + b->size, ptr, total);
    b->size += total;
    b->data[b->size] = '\0';
    return total;
}

int github_exchange_code(const char *client_id, const char *client_secret,
                         const char *code,
                         char *token_out, int token_size,
                         char *login_out, int login_size)
{
    token_out[0] = '\0';
    login_out[0] = '\0';

    /* Step 1: POST to github.com/login/oauth/access_token */
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    char post_body[1024];
    snprintf(post_body, sizeof(post_body),
        "client_id=%s&client_secret=%s&code=%s",
        client_id, client_secret, code);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");

    buf_t resp = {NULL, 0};

    curl_easy_setopt(curl, CURLOPT_URL,
        "https://github.com/login/oauth/access_token");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !resp.data) {
        free(resp.data);
        return -1;
    }

    struct json_object *obj = json_tokener_parse(resp.data);
    free(resp.data);
    if (!obj) return -1;

    struct json_object *tok_obj;
    if (!json_object_object_get_ex(obj, "access_token", &tok_obj)) {
        json_object_put(obj);
        return -1;
    }
    snprintf(token_out, (size_t)token_size, "%s",
             json_object_get_string(tok_obj));
    json_object_put(obj);

    if (token_out[0] == '\0') return -1;

    /* Step 2: GET api.github.com/user to get login */
    curl = curl_easy_init();
    if (!curl) return -1;

    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header),
        "Authorization: Bearer %s", token_out);

    headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers,
        "User-Agent: UvA-Proxy/1.0");

    buf_t user_resp = {NULL, 0};

    curl_easy_setopt(curl, CURLOPT_URL,
        "https://api.github.com/user");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &user_resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !user_resp.data) {
        free(user_resp.data);
        return 0; /* token works, just no login name */
    }

    obj = json_tokener_parse(user_resp.data);
    free(user_resp.data);
    if (!obj) return 0;

    struct json_object *login_obj;
    if (json_object_object_get_ex(obj, "login", &login_obj))
        snprintf(login_out, (size_t)login_size, "%s",
                 json_object_get_string(login_obj));
    json_object_put(obj);

    return 0;
}
