#include "config/config.h"
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef PLATFORM_WINDOWS
  #include <getopt.h>
#else
  #include <getopt.h>
#endif

static void trim(char *s)
{
    char *end;
    while (isspace((unsigned char)*s)) s++;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
}

static void set_defaults(proxy_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->listen_port = DEFAULT_LISTEN_PORT;
    strncpy(cfg->base_url, DEFAULT_BASE_URL, MAX_URL_LEN - 1);
    strncpy(cfg->default_model, DEFAULT_MODEL, MAX_MODEL_LEN - 1);
    cfg->do_login = 0;
    cfg->verbose = 0;
    cfg->headless = 0;
    cfg->webui_enabled = 1;
    cfg->webui_port = 8080;
}

static int parse_env_file(proxy_config_t *cfg, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    char line[MAX_COOKIE_LEN + 256];
    while (fgets(line, sizeof(line), f)) {
        /* skip comments and blank lines */
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0' || *p == '\n')
            continue;

        char *eq = strchr(p, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        trim(key);
        trim(val);

        /* strip surrounding quotes */
        size_t vlen = strlen(val);
        if (vlen >= 2 && ((val[0] == '"' && val[vlen-1] == '"') ||
                          (val[0] == '\'' && val[vlen-1] == '\''))) {
            val[vlen-1] = '\0';
            val++;
        }

        if (strcmp(key, "LISTEN_PORT") == 0) {
            cfg->listen_port = (uint16_t)atoi(val);
        } else if (strcmp(key, "UVA_SESSION_COOKIE") == 0) {
            strncpy(cfg->session_cookie, val, MAX_COOKIE_LEN - 1);
        } else if (strcmp(key, "UVA_BASE_URL") == 0) {
            strncpy(cfg->base_url, val, MAX_URL_LEN - 1);
        } else if (strcmp(key, "DEFAULT_MODEL") == 0) {
            strncpy(cfg->default_model, val, MAX_MODEL_LEN - 1);
        } else if (strcmp(key, "GITHUB_CLIENT_ID") == 0) {
            strncpy(cfg->github_client_id, val,
                    sizeof(cfg->github_client_id) - 1);
        } else if (strcmp(key, "GITHUB_CLIENT_SECRET") == 0) {
            strncpy(cfg->github_client_secret, val,
                    sizeof(cfg->github_client_secret) - 1);
        } else if (strcmp(key, "VPS_HOST") == 0) {
            strncpy(cfg->vps_host, val, sizeof(cfg->vps_host) - 1);
        } else if (strcmp(key, "VPS_SSH_KEY") == 0) {
            strncpy(cfg->vps_ssh_key, val, sizeof(cfg->vps_ssh_key) - 1);
        } else if (strcmp(key, "VPS_DOCKER_IMAGE") == 0) {
            strncpy(cfg->vps_docker_image, val,
                    sizeof(cfg->vps_docker_image) - 1);
        } else if (strcmp(key, "WEBUI_ENABLED") == 0) {
            cfg->webui_enabled = (atoi(val) != 0);
        } else if (strcmp(key, "WEBUI_PORT") == 0) {
            cfg->webui_port = (uint16_t)atoi(val);
        }
    }

    fclose(f);
    return 0;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "Options:\n"
        "  --port PORT       Listen port (default: %d)\n"
        "  --cookie COOKIE   Session cookie value\n"
        "  --model MODEL     Default model (default: %s)\n"
        "  --url URL         UvA base URL (default: %s)\n"
        "  --login           Run browser login flow\n"
        "  --headless        Do not open dashboard window\n"
        "  --no-webui        Do not auto-launch Open WebUI\n"
        "  --verbose         Enable verbose logging\n"
        "  --help            Show this help\n",
        prog, DEFAULT_LISTEN_PORT, DEFAULT_MODEL, DEFAULT_BASE_URL);
}

int config_load(proxy_config_t *cfg, int argc, char **argv)
{
    set_defaults(cfg);

    /* Try to load from proxy.env in current directory */
    parse_env_file(cfg, CONFIG_FILE);

    /* Also try config dir */
    char cfg_path[PLATFORM_PATH_MAX];
    platform_path_join(cfg_path, sizeof(cfg_path),
                       platform_config_dir(), CONFIG_FILE);
    parse_env_file(cfg, cfg_path);

    /* Parse CLI arguments */
    static struct option long_opts[] = {
        {"port",     required_argument, NULL, 'p'},
        {"cookie",   required_argument, NULL, 'c'},
        {"model",    required_argument, NULL, 'm'},
        {"url",      required_argument, NULL, 'u'},
        {"login",    no_argument,       NULL, 'l'},
        {"headless", no_argument,       NULL, 'H'},
        {"no-webui", no_argument,       NULL, 'W'},
        {"verbose",  no_argument,       NULL, 'v'},
        {"help",     no_argument,       NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    optind = 1; /* reset getopt */
    while ((opt = getopt_long(argc, argv, "p:c:m:u:lHWvh",
                              long_opts, NULL)) != -1) {
        switch (opt) {
        case 'p':
            cfg->listen_port = (uint16_t)atoi(optarg);
            break;
        case 'c':
            strncpy(cfg->session_cookie, optarg, MAX_COOKIE_LEN - 1);
            break;
        case 'm':
            strncpy(cfg->default_model, optarg, MAX_MODEL_LEN - 1);
            break;
        case 'u':
            strncpy(cfg->base_url, optarg, MAX_URL_LEN - 1);
            break;
        case 'l':
            cfg->do_login = 1;
            break;
        case 'H':
            cfg->headless = 1;
            break;
        case 'W':
            cfg->webui_enabled = 0;
            break;
        case 'v':
            cfg->verbose = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            exit(0);
        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    return 0;
}

int config_save_cookie(const proxy_config_t *cfg)
{
    /* Read existing file, update UVA_SESSION_COOKIE line, write back */
    FILE *f = fopen(CONFIG_FILE, "r");
    char lines[128][MAX_COOKIE_LEN + 256];
    int nlines = 0;
    int found = 0;

    if (f) {
        while (nlines < 128 && fgets(lines[nlines], sizeof(lines[0]), f)) {
            char *p = lines[nlines];
            while (isspace((unsigned char)*p)) p++;
            if (strncmp(p, "UVA_SESSION_COOKIE", 18) == 0) {
                snprintf(lines[nlines], sizeof(lines[0]),
                         "UVA_SESSION_COOKIE=\"%s\"\n", cfg->session_cookie);
                found = 1;
            }
            nlines++;
        }
        fclose(f);
    }

    if (!found) {
        snprintf(lines[nlines], sizeof(lines[0]),
                 "UVA_SESSION_COOKIE=\"%s\"\n", cfg->session_cookie);
        nlines++;
    }

    f = fopen(CONFIG_FILE, "w");
    if (!f) {
        perror("config_save_cookie: fopen");
        return -1;
    }
    for (int i = 0; i < nlines; i++)
        fputs(lines[i], f);
    fclose(f);
    return 0;
}

void config_print(const proxy_config_t *cfg)
{
    fprintf(stderr, "Configuration:\n");
    fprintf(stderr, "  listen_port:  %d\n", cfg->listen_port);
    fprintf(stderr, "  base_url:     %s\n", cfg->base_url);
    fprintf(stderr, "  default_model: %s\n", cfg->default_model);
    fprintf(stderr, "  cookie:       %s\n",
            cfg->session_cookie[0] ? "(set)" : "(not set)");
    fprintf(stderr, "  headless:     %s\n", cfg->headless ? "yes" : "no");
    fprintf(stderr, "  verbose:      %s\n", cfg->verbose ? "yes" : "no");
    fprintf(stderr, "  github:       %s\n",
            cfg->github_client_id[0] ? "(configured)" : "(not set)");
    fprintf(stderr, "  vps_host:     %s\n",
            cfg->vps_host[0] ? cfg->vps_host : "(not set)");
    fprintf(stderr, "  vps_image:    %s\n",
            cfg->vps_docker_image[0] ? cfg->vps_docker_image : "(default)");
    fprintf(stderr, "  webui:        %s (port %d)\n",
            cfg->webui_enabled ? "enabled" : "disabled", cfg->webui_port);
}
