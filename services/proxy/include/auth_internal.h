#ifndef UVA_PROXY_AUTH_INTERNAL_H
#define UVA_PROXY_AUTH_INTERNAL_H

#include "config.h"
#include <stddef.h>

/* browser.c -- Firefox cookie extraction */
int find_firefox_cookies(char *out, size_t out_size);
int extract_firefox_cookies(const char *db_path, char *out, size_t out_size);

/* chrome.c -- Chrome/Chromium cookie extraction */
int chrome_extract_cookie(const char *domain, const char *name,
                          char *out, size_t out_size);

#endif /* UVA_PROXY_AUTH_INTERNAL_H */
