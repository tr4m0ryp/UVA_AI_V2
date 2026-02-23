#ifndef UVA_PROXY_CHAT_THREAD_H
#define UVA_PROXY_CHAT_THREAD_H

#include <json-c/json.h>

/*
 * Build a stable conversation key from SHA-256 hash of
 * (api_key_id, model, first_user_content).
 * Writes hex string to out (must be >= 65 bytes). Returns 0 on success.
 */
int chat_make_conv_key(int64_t api_key_id, const char *model,
                        const char *first_user_content,
                        char *out, size_t out_size);

/* Extract the content of the first user message from an OpenAI messages
 * array. Returns a borrowed pointer (valid as long as msgs lives). */
const char *chat_first_user_content(struct json_object *msgs);

/* Extract the content of the last user message. Borrowed pointer. */
const char *chat_last_user_content(struct json_object *msgs);

/* Count the number of messages in an OpenAI messages array. */
int chat_count_messages(struct json_object *msgs);

#endif /* UVA_PROXY_CHAT_THREAD_H */
