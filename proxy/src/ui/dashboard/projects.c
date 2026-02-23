#include "ui/dashboard/dashboard.h"
#include "core/database/project.h"
#include "core/database/database.h"
#include "auth/apikey/apikey.h"
#include "cloud/vps/vps.h"
#include "core/server/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

/* response.c */
void response_send_json(int fd, int status, const char *json);
void response_send_error(int fd, int status, const char *message);

static const char *json_str(struct json_object *obj, const char *key,
                             const char *def)
{
    struct json_object *v;
    if (json_object_object_get_ex(obj, key, &v))
        return json_object_get_string(v);
    return def;
}

static struct json_object *project_to_json(const db_project_t *p)
{
    struct json_object *obj = json_object_new_object();
    json_object_object_add(obj, "id", json_object_new_int64(p->id));
    json_object_object_add(obj, "name",
        json_object_new_string(p->name));
    json_object_object_add(obj, "repo_full_name",
        json_object_new_string(p->repo_full_name));
    json_object_object_add(obj, "model",
        json_object_new_string(p->model));
    json_object_object_add(obj, "instructions",
        json_object_new_string(p->instructions));
    json_object_object_add(obj, "container_id",
        json_object_new_string(p->container_id));
    json_object_object_add(obj, "branch_name",
        json_object_new_string(p->branch_name));
    json_object_object_add(obj, "status",
        json_object_new_string(p->status));
    json_object_object_add(obj, "api_key_id",
        json_object_new_int64(p->api_key_id));
    json_object_object_add(obj, "created_at",
        json_object_new_string(p->created_at));
    json_object_object_add(obj, "updated_at",
        json_object_new_string(p->updated_at));
    return obj;
}

void dashboard_projects_list(http_request_t *req, const db_user_t *user)
{
    (void)req;
    db_project_t *projects = NULL;
    int count = 0;

    if (db_project_list(user->id, &projects, &count) != 0) {
        response_send_error(req->client_fd, 500, "Failed to list projects");
        return;
    }

    struct json_object *arr = json_object_new_array();
    for (int i = 0; i < count; i++)
        json_object_array_add(arr, project_to_json(&projects[i]));
    free(projects);

    const char *str = json_object_to_json_string_ext(arr,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(arr);
}

void dashboard_projects_create(http_request_t *req, const db_user_t *user)
{
    if (!req->body || req->body_len == 0) {
        response_send_error(req->client_fd, 400, "Request body required");
        return;
    }

    struct json_object *body = json_tokener_parse(req->body);
    if (!body) {
        response_send_error(req->client_fd, 400, "Invalid JSON");
        return;
    }

    const char *repo = json_str(body, "repo_full_name", "");
    if (!repo[0]) {
        json_object_put(body);
        response_send_error(req->client_fd, 400,
            "repo_full_name is required");
        return;
    }

    if (!user->github_token[0]) {
        json_object_put(body);
        response_send_error(req->client_fd, 403,
            "GitHub not connected");
        return;
    }

    /* Generate a dedicated API key for this project */
    char plaintext[128], hash[65], prefix[32];
    if (apikey_generate(plaintext, sizeof(plaintext),
                        hash, sizeof(hash),
                        prefix, sizeof(prefix)) != 0) {
        json_object_put(body);
        response_send_error(req->client_fd, 500,
            "Failed to generate API key");
        return;
    }

    /* Store the API key */
    db_api_key_t key;
    memset(&key, 0, sizeof(key));
    key.user_id = user->id;
    snprintf(key.key_hash, sizeof(key.key_hash), "%s", hash);
    snprintf(key.key_prefix, sizeof(key.key_prefix), "%s", prefix);
    snprintf(key.name, sizeof(key.name), "cloud:%s", repo);
    snprintf(key.model, sizeof(key.model), "%s",
             json_str(body, "model", "gpt-4.1"));
    key.temperature = 0.5;
    key.top_p = 0.5;
    key.max_tokens = 4096;
    key.is_active = 1;

    int64_t key_id = 0;
    if (db_key_create(&key, &key_id) != 0) {
        json_object_put(body);
        response_send_error(req->client_fd, 500,
            "Failed to store API key");
        return;
    }

    /* Create the project record */
    db_project_t proj;
    memset(&proj, 0, sizeof(proj));
    proj.user_id = user->id;
    snprintf(proj.name, sizeof(proj.name), "%s",
             json_str(body, "name", repo));
    snprintf(proj.repo_full_name, sizeof(proj.repo_full_name), "%s", repo);
    snprintf(proj.model, sizeof(proj.model), "%s",
             json_str(body, "model", "gpt-4.1"));
    snprintf(proj.instructions, sizeof(proj.instructions), "%s",
             json_str(body, "instructions", ""));
    snprintf(proj.api_key_plain, sizeof(proj.api_key_plain), "%s", plaintext);
    proj.api_key_id = key_id;
    snprintf(proj.status, sizeof(proj.status), "pending");
    json_object_put(body);

    int64_t proj_id = 0;
    if (db_project_create(&proj, &proj_id) != 0) {
        response_send_error(req->client_fd, 500,
            "Failed to create project");
        return;
    }

    /* Launch container on VPS */
    char branch[256];
    snprintf(branch, sizeof(branch), "codex/task-%lld", (long long)proj_id);

    char cid[128] = {0};
    int rc = vps_create_container(proj.repo_full_name,
        user->github_token, branch, proj.model,
        proj.instructions, plaintext, cid, sizeof(cid));

    if (rc == 0 && cid[0]) {
        snprintf(proj.container_id, sizeof(proj.container_id), "%s", cid);
        snprintf(proj.branch_name, sizeof(proj.branch_name), "%s", branch);
        snprintf(proj.status, sizeof(proj.status), "running");
        db_project_set_container(proj_id, cid, branch, "running");
    } else {
        snprintf(proj.status, sizeof(proj.status), "failed");
        db_project_set_status(proj_id, "failed");
    }

    proj.id = proj_id;
    struct json_object *resp = project_to_json(&proj);
    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 201, str);
    json_object_put(resp);

    fprintf(stderr, "Project created: %s (id=%lld, status=%s)\n",
            proj.name, (long long)proj_id, proj.status);
}

void dashboard_projects_get(http_request_t *req, const db_user_t *user,
                            int64_t id)
{
    db_project_t proj;
    if (db_project_get(id, user->id, &proj) != 0) {
        response_send_error(req->client_fd, 404, "Project not found");
        return;
    }

    struct json_object *resp = project_to_json(&proj);
    const char *str = json_object_to_json_string_ext(resp,
        JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(resp);
}

void dashboard_projects_update(http_request_t *req, const db_user_t *user,
                               int64_t id)
{
    if (!req->body || req->body_len == 0) {
        response_send_error(req->client_fd, 400, "Request body required");
        return;
    }

    struct json_object *body = json_tokener_parse(req->body);
    if (!body) {
        response_send_error(req->client_fd, 400, "Invalid JSON");
        return;
    }

    db_project_t proj;
    if (db_project_get(id, user->id, &proj) != 0) {
        json_object_put(body);
        response_send_error(req->client_fd, 404, "Project not found");
        return;
    }

    struct json_object *v;
    if (json_object_object_get_ex(body, "name", &v))
        snprintf(proj.name, sizeof(proj.name), "%s",
                 json_object_get_string(v));
    if (json_object_object_get_ex(body, "model", &v))
        snprintf(proj.model, sizeof(proj.model), "%s",
                 json_object_get_string(v));
    if (json_object_object_get_ex(body, "instructions", &v))
        snprintf(proj.instructions, sizeof(proj.instructions), "%s",
                 json_object_get_string(v));
    json_object_put(body);

    if (db_project_update(id, user->id, &proj) != 0) {
        response_send_error(req->client_fd, 500, "Failed to update project");
        return;
    }

    response_send_json(req->client_fd, 200, "{\"ok\":true}");
}

void dashboard_projects_delete(http_request_t *req, const db_user_t *user,
                               int64_t id)
{
    /* Remove VPS container if any */
    db_project_t proj;
    if (db_project_get(id, user->id, &proj) == 0 && proj.container_id[0])
        vps_remove_container(proj.container_id);

    if (db_project_delete(id, user->id) != 0) {
        response_send_error(req->client_fd, 404,
            "Project not found or access denied");
        return;
    }
    response_send_json(req->client_fd, 200, "{\"ok\":true}");
}

void dashboard_projects_status(http_request_t *req, const db_user_t *user,
                               int64_t id)
{
    db_project_t proj;
    if (db_project_get(id, user->id, &proj) != 0)
        { response_send_error(req->client_fd, 404, "Project not found"); return; }
    if (proj.container_id[0] && strcmp(proj.status, "running") == 0) {
        char vs[64] = {0};
        if (vps_container_status(proj.container_id, vs, sizeof(vs)) == 0 &&
            strcmp(vs, "exited") == 0) {
            db_project_set_status(proj.id, "done");
            snprintf(proj.status, sizeof(proj.status), "done");
        }
    }
    struct json_object *resp = project_to_json(&proj);
    const char *str = json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(resp);
}

void dashboard_projects_logs(http_request_t *req, const db_user_t *user,
                             int64_t id)
{
    db_project_t proj;
    if (db_project_get(id, user->id, &proj) != 0)
        { response_send_error(req->client_fd, 404, "Project not found"); return; }
    if (!proj.container_id[0])
        { response_send_json(req->client_fd, 200, "{\"logs\":\"\"}"); return; }
    char *logs = NULL;
    vps_container_logs(proj.container_id, &logs);
    struct json_object *resp = json_object_new_object();
    json_object_object_add(resp, "logs",
        json_object_new_string(logs ? logs : ""));
    free(logs);
    const char *str = json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
    response_send_json(req->client_fd, 200, str);
    json_object_put(resp);
}
