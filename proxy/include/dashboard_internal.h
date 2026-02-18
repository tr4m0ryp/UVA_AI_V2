#ifndef UVA_PROXY_DASHBOARD_INTERNAL_H
#define UVA_PROXY_DASHBOARD_INTERNAL_H

/* Shared helpers between dashboard auth files. */

/* response.c */
void response_send_json(int fd, int status, const char *json);
void response_send_error(int fd, int status, const char *message);

/* auth.c -- generate a random 64-char hex token. Returns 0 on success. */
int dashboard_generate_token(char *out, size_t size);

#endif /* UVA_PROXY_DASHBOARD_INTERNAL_H */
