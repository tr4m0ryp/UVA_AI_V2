#ifndef UVA_PROXY_DASHBOARD_H
#define UVA_PROXY_DASHBOARD_H

#include "server.h"

/* Handle dashboard API requests. */
void dashboard_handle(http_request_t *req);

/* Serve static files from dashboard/ directory for /dashboard* paths. */
void dashboard_serve_static(http_request_t *req);

#endif /* UVA_PROXY_DASHBOARD_H */
