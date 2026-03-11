#include "api/translator/translator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/*
 * Model ID mapping between OpenAI-style names and UvA internal names.
 * Based on extracted theme-config.ts from aichat.uva.nl source maps.
 *
 * UvA uses the model IDs directly (e.g. "claude-sonnet-4.5").
 * We expose the same IDs plus common aliases.
 */
/*
 * Models confirmed available on aichat.uva.nl as of 2026-03-11.
 * Verified via direct API calls to /api/v1/chat.
 * Only models that return valid responses are listed here.
 */
static const model_mapping_t g_models[] = {
    /* OpenAI GPT-5 family */
    {"gpt-5",               "gpt-5",               "GPT-5"},
    {"gpt-5.1",             "gpt-5.1",             "GPT-5.1"},
    {"gpt-5.2",             "gpt-5.2",             "GPT-5.2"},
    {"gpt-5-mini",          "gpt-5-mini",          "GPT-5 Mini"},
    {"gpt-5-nano",          "gpt-5-nano",          "GPT-5 Nano"},
    /* OpenAI GPT-4 family */
    {"gpt-4.1",             "gpt-4.1",             "GPT-4.1"},
    {"gpt-4o",              "gpt-4o",              "GPT-4o"},
    /* Anthropic Claude */
    {"claude-sonnet-4.6",   "claude-sonnet-4.6",   "Claude Sonnet 4.6"},
    {"claude-sonnet-4.5",   "claude-sonnet-4.5",   "Claude Sonnet 4.5"},
    {"claude-haiku-4.5",    "claude-haiku-4.5",    "Claude Haiku 4.5"},
    /* Open source */
    {"gpt-oss-120b",        "gpt-oss-120b",        "GPT-OSS 120B"},
};

static const int g_model_count = sizeof(g_models) / sizeof(g_models[0]);

int models_get_all(const model_mapping_t **out)
{
    *out = g_models;
    return g_model_count;
}

const char *models_to_uva(const char *openai_id)
{
    for (int i = 0; i < g_model_count; i++) {
        if (strcasecmp(g_models[i].openai_id, openai_id) == 0)
            return g_models[i].uva_id;
    }
    /* If not found, pass through as-is (UvA might accept it) */
    return openai_id;
}

const char *models_to_openai(const char *uva_id)
{
    for (int i = 0; i < g_model_count; i++) {
        if (strcasecmp(g_models[i].uva_id, uva_id) == 0)
            return g_models[i].openai_id;
    }
    return uva_id;
}

char *models_list_json(void)
{
    struct json_object *root = json_object_new_object();
    struct json_object *data = json_object_new_array();

    json_object_object_add(root, "object", json_object_new_string("list"));

    for (int i = 0; i < g_model_count; i++) {
        struct json_object *model = json_object_new_object();
        json_object_object_add(model, "id",
            json_object_new_string(g_models[i].openai_id));
        json_object_object_add(model, "object",
            json_object_new_string("model"));
        json_object_object_add(model, "created",
            json_object_new_int(1700000000));
        json_object_object_add(model, "owned_by",
            json_object_new_string("uva-proxy"));
        json_object_array_add(data, model);
    }

    json_object_object_add(root, "data", data);

    const char *str = json_object_to_json_string_ext(root,
        JSON_C_TO_STRING_PLAIN);
    char *result = strdup(str);
    json_object_put(root);
    return result;
}
