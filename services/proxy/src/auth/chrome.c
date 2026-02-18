#include "auth_internal.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <openssl/evp.h>

#define PATH_MAX_LEN  512
#define CMD_MAX_LEN   1200
#define CHROME_SALT   "saltysalt"
#define CHROME_SALT_LEN 9
#define CHROME_KEY_LEN  16
#define CHROME_IV_LEN   16
#define CHROME_ITER     1

/* Chrome cookie DB paths to try, in order of preference. */
static const char *chrome_db_patterns[] = {
    "%s/.config/google-chrome/Default/Cookies",
    "%s/.config/chromium/Default/Cookies",
    "%s/snap/chromium/common/chromium/Default/Cookies",
    NULL
};

/*
 * Find the first existing Chrome/Chromium Cookies database.
 */
static int find_chrome_cookies_db(char *out, size_t out_size)
{
    const char *home = getenv("HOME");
    if (!home) return -1;

    for (int i = 0; chrome_db_patterns[i]; i++) {
        snprintf(out, out_size, chrome_db_patterns[i], home);
        struct stat st;
        if (stat(out, &st) == 0)
            return 0;
    }
    return -1;
}

/*
 * Derive the Chrome decryption key using PBKDF2.
 * On Linux, Chrome uses the password from gnome-keyring (via secret-tool),
 * falling back to the hardcoded "peanuts" default.
 */
static int derive_chrome_key(unsigned char *key_out)
{
    const char *password = NULL;
    static char keyring_pass[256];

    /* Try gnome-keyring first */
    FILE *pipe = popen(
        "secret-tool lookup application chrome 2>/dev/null", "r");
    if (pipe) {
        if (fgets(keyring_pass, sizeof(keyring_pass), pipe)) {
            size_t len = strlen(keyring_pass);
            while (len > 0 && (keyring_pass[len-1] == '\n' ||
                               keyring_pass[len-1] == '\r'))
                keyring_pass[--len] = '\0';
            if (len > 0)
                password = keyring_pass;
        }
        pclose(pipe);
    }

    if (!password)
        password = "peanuts";

    if (PKCS5_PBKDF2_HMAC_SHA1(password, (int)strlen(password),
                                (const unsigned char *)CHROME_SALT,
                                CHROME_SALT_LEN, CHROME_ITER,
                                CHROME_KEY_LEN, key_out) != 1)
        return -1;
    return 0;
}

/*
 * Decrypt a Chrome "v10" encrypted cookie value.
 * Format: "v10" (3 bytes) + IV (16 bytes) + ciphertext
 * Returns 0 on success, -1 on failure.
 */
static int decrypt_v10_cookie(const unsigned char *encrypted, int enc_len,
                              const unsigned char *key,
                              char *out, size_t out_size)
{
    /* Must start with "v10" and have at least IV + 1 block */
    if (enc_len < 3 + CHROME_IV_LEN + 1)
        return -1;
    if (memcmp(encrypted, "v10", 3) != 0)
        return -1;

    const unsigned char *iv = encrypted + 3;
    const unsigned char *ciphertext = encrypted + 3 + CHROME_IV_LEN;
    int ct_len = enc_len - 3 - CHROME_IV_LEN;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    int ok = -1;
    unsigned char *plaintext = malloc((size_t)ct_len + 16);
    if (!plaintext) { EVP_CIPHER_CTX_free(ctx); return -1; }

    int outlen = 0, finallen = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv) != 1)
        goto done;
    if (EVP_DecryptUpdate(ctx, plaintext, &outlen, ciphertext, ct_len) != 1)
        goto done;
    if (EVP_DecryptFinal_ex(ctx, plaintext + outlen, &finallen) != 1)
        goto done;

    outlen += finallen;
    if (outlen <= 0 || (size_t)outlen >= out_size)
        goto done;

    memcpy(out, plaintext, (size_t)outlen);
    out[outlen] = '\0';
    ok = 0;

done:
    free(plaintext);
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

/*
 * Extract a specific cookie from Chrome/Chromium for a given domain.
 * Copies the DB to /tmp first to avoid lock conflicts.
 */
int chrome_extract_cookie(const char *domain, const char *name,
                          char *out, size_t out_size)
{
    char db_path[PATH_MAX_LEN];
    if (find_chrome_cookies_db(db_path, sizeof(db_path)) != 0)
        return -1;

    /* Copy DB to temp to avoid lock conflicts */
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "/tmp/uva_chrome_cookies_%d.sqlite", getpid());

    char cmd[CMD_MAX_LEN];
    snprintf(cmd, sizeof(cmd), "cp '%.400s' '%s'", db_path, tmp);
    if (system(cmd) != 0) return -1;

    /* Also copy WAL if present */
    char wal_src[PATH_MAX_LEN], wal_dst[80];
    snprintf(wal_src, sizeof(wal_src), "%.500s-wal", db_path);
    snprintf(wal_dst, sizeof(wal_dst), "%s-wal", tmp);
    snprintf(cmd, sizeof(cmd), "cp '%s' '%s' 2>/dev/null", wal_src, wal_dst);
    system(cmd);

    /* Derive decryption key */
    unsigned char key[CHROME_KEY_LEN];
    if (derive_chrome_key(key) != 0) {
        unlink(tmp);
        return -1;
    }

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(tmp, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        unlink(tmp);
        unlink(wal_dst);
        return -1;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT encrypted_value, value FROM cookies "
        "WHERE host_key LIKE ? AND name = ? "
        "ORDER BY last_access_utc DESC LIMIT 1";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        unlink(tmp);
        unlink(wal_dst);
        return -1;
    }

    char host_pattern[256];
    snprintf(host_pattern, sizeof(host_pattern), "%%%s%%", domain);
    sqlite3_bind_text(stmt, 1, host_pattern, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void *enc_blob = sqlite3_column_blob(stmt, 0);
        int enc_len = sqlite3_column_bytes(stmt, 0);
        const char *plain_value = (const char *)sqlite3_column_text(stmt, 1);

        /* Try encrypted value first */
        if (enc_blob && enc_len > 3) {
            result = decrypt_v10_cookie(
                (const unsigned char *)enc_blob, enc_len, key,
                out, out_size);
        }

        /* Fall back to plaintext value if decryption failed */
        if (result != 0 && plain_value && plain_value[0]) {
            snprintf(out, out_size, "%s", plain_value);
            result = 0;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    unlink(tmp);
    unlink(wal_dst);
    return result;
}
