#ifndef UVA_PROXY_PROJECT_H
#define UVA_PROXY_PROJECT_H

#include <stdint.h>

#define PROJECT_MAX_NAME         256
#define PROJECT_MAX_PATH         512
#define PROJECT_MAX_REPO         512
#define PROJECT_MAX_MODEL        128
#define PROJECT_MAX_INSTRUCTIONS 4096
#define PROJECT_MAX_KEY          128
#define PROJECT_MAX_CID          128
#define PROJECT_MAX_BRANCH       256
#define PROJECT_MAX_STATUS       32
#define PROJECT_MAX_DATE         32

typedef struct {
    int64_t id;
    int64_t user_id;
    char    name[PROJECT_MAX_NAME];
    char    repo_full_name[PROJECT_MAX_REPO];
    char    model[PROJECT_MAX_MODEL];
    char    instructions[PROJECT_MAX_INSTRUCTIONS];
    char    api_key_plain[PROJECT_MAX_KEY];
    int64_t api_key_id;
    char    container_id[PROJECT_MAX_CID];
    char    branch_name[PROJECT_MAX_BRANCH];
    char    status[PROJECT_MAX_STATUS];      /* pending/running/done/failed */
    char    created_at[PROJECT_MAX_DATE];
    char    updated_at[PROJECT_MAX_DATE];
} db_project_t;

/* Initialize the projects table schema. Returns 0 on success. */
int db_project_init_schema(void);

/* Create a new project. Writes new ID to out_id. Returns 0 on success. */
int db_project_create(const db_project_t *proj, int64_t *out_id);

/* List all projects for a user. Caller must free *out. Returns 0 on success. */
int db_project_list(int64_t user_id, db_project_t **out, int *count);

/* Get a single project by id + user_id. Returns 0 on success. */
int db_project_get(int64_t id, int64_t user_id, db_project_t *out);

/* Update a project. Returns 0 on success, -1 if not found. */
int db_project_update(int64_t id, int64_t user_id, const db_project_t *proj);

/* Update container_id, branch_name, status. Returns 0 on success. */
int db_project_set_container(int64_t id, const char *container_id,
                             const char *branch, const char *status);

/* Update just the status field. Returns 0 on success. */
int db_project_set_status(int64_t id, const char *status);

/* Delete a project and its associated API key. Returns 0 on success. */
int db_project_delete(int64_t id, int64_t user_id);

#endif /* UVA_PROXY_PROJECT_H */
