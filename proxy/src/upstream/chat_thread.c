#include "upstream/chat_thread.h"
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>

/*
 * Extract text content from an OpenAI message's content field.
 * Handles both string and array-of-parts formats.
 */
static const char *msg_content(struct json_object *m)
{
    struct json_object *c;
    if (!json_object_object_get_ex(m, "content", &c))
        return NULL;

    if (json_object_is_type(c, json_type_string))
        return json_object_get_string(c);

    if (json_object_is_type(c, json_type_array)) {
        int len = (int)json_object_array_length(c);
        for (int i = 0; i < len; i++) {
            struct json_object *part =
                json_object_array_get_idx(c, (size_t)i);
            struct json_object *txt;
            if (json_object_object_get_ex(part, "text", &txt))
                return json_object_get_string(txt);
        }
    }
    return NULL;
}

static const char *msg_role(struct json_object *m)
{
    struct json_object *r;
    if (json_object_object_get_ex(m, "role", &r))
        return json_object_get_string(r);
    return "";
}

int chat_make_conv_key(int64_t api_key_id, const char *model,
                        const char *first_user_content,
                        char *out, size_t out_size)
{
    if (out_size < 65) return -1;
    if (!model) model = "";
    if (!first_user_content) first_user_content = "";

    /* Build input: "api_key_id:model:first_user_content" */
    char prefix[256];
    int n = snprintf(prefix, sizeof(prefix), "%lld:%s:",
                     (long long)api_key_id, model);

    unsigned char hash[32];
    unsigned int hash_len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, prefix, (size_t)n);
    EVP_DigestUpdate(ctx, first_user_content, strlen(first_user_content));
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    for (unsigned int i = 0; i < hash_len; i++)
        sprintf(out + i * 2, "%02x", hash[i]);
    out[hash_len * 2] = '\0';

    return 0;
}

const char *chat_first_user_content(struct json_object *msgs)
{
    if (!msgs || !json_object_is_type(msgs, json_type_array))
        return NULL;

    int len = (int)json_object_array_length(msgs);
    for (int i = 0; i < len; i++) {
        struct json_object *m = json_object_array_get_idx(msgs, (size_t)i);
        const char *role = msg_role(m);
        if (strcmp(role, "user") == 0) {
            const char *c = msg_content(m);
            if (c && c[0]) return c;
        }
    }
    return NULL;
}

const char *chat_last_user_content(struct json_object *msgs)
{
    if (!msgs || !json_object_is_type(msgs, json_type_array))
        return NULL;

    int len = (int)json_object_array_length(msgs);
    for (int i = len - 1; i >= 0; i--) {
        struct json_object *m = json_object_array_get_idx(msgs, (size_t)i);
        const char *role = msg_role(m);
        if (strcmp(role, "user") == 0) {
            const char *c = msg_content(m);
            if (c && c[0]) return c;
        }
    }
    return NULL;
}

int chat_count_messages(struct json_object *msgs)
{
    if (!msgs || !json_object_is_type(msgs, json_type_array))
        return 0;
    return (int)json_object_array_length(msgs);
}
