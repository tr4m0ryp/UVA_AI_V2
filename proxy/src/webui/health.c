#include "webui/webui.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

/* Discard response body */
static size_t discard_cb(char *data, size_t size, size_t nmemb, void *ud)
{
    (void)data; (void)ud;
    return size * nmemb;
}

int webui_check_health(int port)
{
    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    char url[128];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/health", port);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_cb);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    if (res == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);
    return (res == CURLE_OK && http_code == 200) ? 1 : 0;
}

int webui_port_in_use(int port)
{
    /* Try a TCP connect to 127.0.0.1:port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    platform_close_socket(sock);
    return (ret == 0) ? 1 : 0;
}
