#ifndef UVA_PROXY_GRADING_H
#define UVA_PROXY_GRADING_H

#include <stdint.h>

#define GRADING_MAX_NAME        256
#define GRADING_MAX_STATUS      32
#define GRADING_MAX_FEEDBACK    8192
#define GRADING_MAX_CRITERIA    16384
#define GRADING_MAX_STUDENT_ID  64
#define GRADING_MAX_FILENAME    256

typedef struct {
    int64_t id;
    int64_t user_id;
    char    name[GRADING_MAX_NAME];
    int     submission_count;
    double  avg_percentage;
    char    status[GRADING_MAX_STATUS];
    char    created_at[32];
} db_grading_session_t;

typedef struct {
    int64_t id;
    int64_t session_id;
    char    student_name[GRADING_MAX_NAME];
    char    student_id_str[GRADING_MAX_STUDENT_ID];
    char    file_name[GRADING_MAX_FILENAME];
    double  total_score;
    double  total_max;
    double  percentage;
    char    overall_feedback[GRADING_MAX_FEEDBACK];
    char    criteria_json[GRADING_MAX_CRITERIA];
    char    status[GRADING_MAX_STATUS];
    char    error_text[1024];
    char    created_at[32];
} db_grading_result_t;

/* Session CRUD */
int db_grading_session_create(int64_t user_id, const char *name,
                              const char *assignment, const char *rubric,
                              const char *example, int64_t *out_id);
int db_grading_session_list(int64_t user_id, db_grading_session_t **out,
                            int *count);
int db_grading_session_get(int64_t session_id, int64_t user_id,
                           db_grading_session_t *out);
int db_grading_session_delete(int64_t session_id, int64_t user_id);
int db_grading_session_update_stats(int64_t session_id, int count,
                                    double avg_pct, const char *status);

/* Result CRUD */
int db_grading_result_save(int64_t session_id, const db_grading_result_t *r);
int db_grading_result_list(int64_t session_id, db_grading_result_t **out,
                           int *count);

#endif /* UVA_PROXY_GRADING_H */
