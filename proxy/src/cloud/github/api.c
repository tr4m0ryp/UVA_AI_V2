#include "cloud/github/github.h"
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

static CURL *github_curl(const char *url, const char *token,
                          struct curl_slist **headers_out, buf_t *resp)
{
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);

    struct curl_slist *h = NULL;
    h = curl_slist_append(h, auth);
    h = curl_slist_append(h, "Accept: application/vnd.github+json");
    h = curl_slist_append(h, "User-Agent: UvA-Proxy/1.0");
    h = curl_slist_append(h, "X-GitHub-Api-Version: 2022-11-28");
    *headers_out = h;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

    return curl;
}

static const char *jstr(struct json_object *obj, const char *key)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_string(v);
    return "";
}

static int jbool(struct json_object *obj, const char *key)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_boolean(v);
    return 0;
}

static int64_t jint(struct json_object *obj, const char *key)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_int64(v);
    return 0;
}

int github_list_repos(const char *token,
                      github_repo_t **repos_out, int *count)
{
    *repos_out = NULL;
    *count = 0;

    const char *url =
        "https://api.github.com/user/repos"
        "?per_page=100&sort=updated&type=all";

    struct curl_slist *headers = NULL;
    buf_t resp = {NULL, 0};
    CURL *curl = github_curl(url, token, &headers, &resp);
    if (!curl) return -1;

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !resp.data) {
        free(resp.data);
        return -1;
    }

    struct json_object *arr = json_tokener_parse(resp.data);
    free(resp.data);
    if (!arr || !json_object_is_type(arr, json_type_array)) {
        if (arr) json_object_put(arr);
        return -1;
    }

    int n = (int)json_object_array_length(arr);
    if (n == 0) {
        json_object_put(arr);
        return 0;
    }

    github_repo_t *out = calloc((size_t)n, sizeof(github_repo_t));
    if (!out) {
        json_object_put(arr);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        struct json_object *r = json_object_array_get_idx(arr, (size_t)i);
        out[i].github_id = jint(r, "id");
        snprintf(out[i].full_name, sizeof(out[i].full_name),
                 "%s", jstr(r, "full_name"));
        snprintf(out[i].description, sizeof(out[i].description),
                 "%s", jstr(r, "description"));
        snprintf(out[i].clone_url, sizeof(out[i].clone_url),
                 "%s", jstr(r, "clone_url"));
        out[i].is_private = jbool(r, "private");
    }

    json_object_put(arr);
    *repos_out = out;
    *count = n;
    return 0;
}
