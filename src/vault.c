#include "vault.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#define PBKDF2_ITERATIONS 100000
#define HASH_BYTES 32
#define SALT_BYTES 16
#define KEY_BYTES 32
#define IV_BYTES 16

typedef struct {
    char filename[MAX_FILENAME_LEN + 1];
    char owner[MAX_USERNAME_LEN];
    char group[MAX_USERNAME_LEN];
    char permissions[10];
    int encrypted;
} FileMetadata;

static pthread_mutex_t metadata_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t audit_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t auth_lock = PTHREAD_MUTEX_INITIALIZER;
static sem_t *file_access_sem;
static unsigned char vault_key[KEY_BYTES];
static int initialized;

typedef struct { char username[MAX_USERNAME_LEN]; int failures; int locked; } LoginState;
static LoginState login_states[32];

static void hex_encode(const unsigned char *input, size_t len, char *out, size_t out_size) {
    if (out_size < len * 2 + 1) return;
    for (size_t i = 0; i < len; i++) snprintf(out + i * 2, 3, "%02x", input[i]);
}

static int hex_decode(const char *input, unsigned char *out, size_t len) {
    if (strlen(input) != len * 2) return SV_ERR;
    for (size_t i = 0; i < len; i++) {
        unsigned int byte;
        if (sscanf(input + i * 2, "%2x", &byte) != 1) return SV_ERR;
        out[i] = (unsigned char)byte;
    }
    return SV_OK;
}

static int derive_password(const char *password, const unsigned char *salt, unsigned char *hash) {
    return PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, SALT_BYTES,
                             PBKDF2_ITERATIONS, EVP_sha256(), HASH_BYTES, hash) == 1 ? SV_OK : SV_ERR;
}

static int ensure_default_users(void) {
    FILE *fp = fopen(USERS_DB, "r");
    if (fp) { fclose(fp); return SV_OK; }
    fp = fopen(USERS_DB, "w");
    if (!fp) return SV_ERR;
    const char *users[][4] = {
        {"admin", "admin123", "staff", "admin"},
        {"author", "author123", "staff", "author"},
        {"auditor", "audit123", "audit", "auditor"},
        {"guest", "guest123", "guests", "viewer"}
    };
    for (size_t i = 0; i < sizeof(users) / sizeof(users[0]); i++) {
        unsigned char salt[SALT_BYTES], hash[HASH_BYTES];
        char salt_hex[SALT_BYTES * 2 + 1] = {0}, hash_hex[HASH_BYTES * 2 + 1] = {0};
        RAND_bytes(salt, sizeof(salt));
        derive_password(users[i][1], salt, hash);
        hex_encode(salt, sizeof(salt), salt_hex, sizeof(salt_hex));
        hex_encode(hash, sizeof(hash), hash_hex, sizeof(hash_hex));
        fprintf(fp, "%s|%s|%s|%s|%s\n", users[i][0], salt_hex, hash_hex, users[i][2], users[i][3]);
    }
    fclose(fp);
    return SV_OK;
}

static int load_or_create_key(void) {
    const char *path = "data/vault.key";
    FILE *fp = fopen(path, "rb");
    if (fp) {
        int ok = fread(vault_key, 1, sizeof(vault_key), fp) == sizeof(vault_key);
        fclose(fp);
        return ok ? SV_OK : SV_ERR;
    }
    if (RAND_bytes(vault_key, sizeof(vault_key)) != 1) return SV_ERR;
    fp = fopen(path, "wb");
    if (!fp) return SV_ERR;
    int ok = fwrite(vault_key, 1, sizeof(vault_key), fp) == sizeof(vault_key);
    fclose(fp);
    chmod(path, S_IRUSR | S_IWUSR);
    return ok ? SV_OK : SV_ERR;
}

int vault_init(void) {
    if (initialized) return SV_OK;
    mkdir("data", 0700);
    mkdir(VAULT_DIR, 0700);
    FILE *fp = fopen(METADATA_DB, "a"); if (fp) fclose(fp);
    fp = fopen(AUDIT_LOG, "a"); if (fp) fclose(fp);
    if (ensure_default_users() != SV_OK || load_or_create_key() != SV_OK) return SV_ERR;
    sem_unlink("/sentinelvault_file_access");
    file_access_sem = sem_open("/sentinelvault_file_access", O_CREAT | O_EXCL, 0600, FILE_ACCESS_LIMIT);
    if (file_access_sem == SEM_FAILED) return SV_ERR;
    initialized = 1;
    return SV_OK;
}

int authenticate_user(const char *username, const char *password, Session *session) {
    FILE *fp = fopen(USERS_DB, "r");
    char line[512];
    if (!fp || !username || !password || !session) return SV_ERR;
    while (fgets(line, sizeof(line), fp)) {
        char user[64], salt_hex[33], hash_hex[65], group[64], role[32];
        if (sscanf(line, "%63[^|]|%32[^|]|%64[^|]|%63[^|]|%31[^\n]",
                   user, salt_hex, hash_hex, group, role) != 5 || strcmp(user, username) != 0) continue;
        unsigned char salt[SALT_BYTES], expected[HASH_BYTES], actual[HASH_BYTES];
        pthread_mutex_lock(&auth_lock);
        LoginState *state = NULL;
        for (size_t i = 0; i < sizeof(login_states) / sizeof(login_states[0]); i++) {
            if (!login_states[i].username[0]) {
                if (!state) state = &login_states[i];
            } else if (!strcmp(login_states[i].username, username)) {
                state = &login_states[i]; break;
            }
        }
        if (state && !state->username[0]) sv_copy(state->username, sizeof(state->username), username);
        if (state && state->locked) { pthread_mutex_unlock(&auth_lock); fclose(fp); return SV_ERR; }
        int ok = hex_decode(salt_hex, salt, sizeof(salt)) == SV_OK &&
                 hex_decode(hash_hex, expected, sizeof(expected)) == SV_OK &&
                 derive_password(password, salt, actual) == SV_OK &&
                 CRYPTO_memcmp(expected, actual, sizeof(actual)) == 0;
        if (state) {
            if (ok) state->failures = 0;
            else if (++state->failures >= 3) state->locked = 1;
        }
        pthread_mutex_unlock(&auth_lock);
        fclose(fp);
        if (!ok) return SV_ERR;
        session->active = 1;
        sv_copy(session->username, sizeof(session->username), user);
        sv_copy(session->group, sizeof(session->group), group);
        sv_copy(session->role, sizeof(session->role), role);
        return SV_OK;
    }
    fclose(fp);
    return SV_ERR;
}

void logout_user(Session *session) {
    if (session) {
        char ip[INET_ADDRSTRLEN]; sv_copy(ip, sizeof(ip), session->client_ip);
        memset(session, 0, sizeof(*session)); sv_copy(session->client_ip, sizeof(session->client_ip), ip);
    }
}

int validate_filename(const char *filename) {
    size_t len = filename ? strlen(filename) : 0;
    if (len == 0 || len > MAX_FILENAME_LEN || strstr(filename, "..")) return SV_ERR;
    for (size_t i = 0; i < len; i++)
        if (!(isalnum((unsigned char)filename[i]) || filename[i] == '_' || filename[i] == '-' || filename[i] == '.'))
            return SV_ERR;
    return SV_OK;
}

static int build_path(const char *filename, char *path, size_t size) {
    if (validate_filename(filename) != SV_OK) return SV_ERR;
    return snprintf(path, size, "%s/%s", VAULT_DIR, filename) < (int)size ? SV_OK : SV_ERR;
}

static int find_metadata(const char *filename, FileMetadata *meta) {
    FILE *fp = fopen(METADATA_DB, "r");
    char line[512];
    if (!fp) return SV_ERR;
    while (fgets(line, sizeof(line), fp)) {
        FileMetadata m;
        if (sscanf(line, "%128[^|]|%63[^|]|%63[^|]|%9[^|]|%d",
                   m.filename, m.owner, m.group, m.permissions, &m.encrypted) == 5 &&
            strcmp(m.filename, filename) == 0) {
            fclose(fp); if (meta) *meta = m; return SV_OK;
        }
    }
    fclose(fp);
    return SV_ERR;
}

static int save_metadata(const FileMetadata *updated, int remove_entry) {
    FILE *in = fopen(METADATA_DB, "r");
    FILE *out = fopen("data/metadata.tmp", "w");
    char line[512]; int found = 0;
    if (!out) { if (in) fclose(in); return SV_ERR; }
    while (in && fgets(line, sizeof(line), in)) {
        char name[MAX_FILENAME_LEN + 1];
        if (sscanf(line, "%128[^|]", name) == 1 && strcmp(name, updated->filename) == 0) {
            found = 1;
            if (!remove_entry) fprintf(out, "%s|%s|%s|%s|%d\n", updated->filename,
                    updated->owner, updated->group, updated->permissions, updated->encrypted);
        } else fputs(line, out);
    }
    if (!found && !remove_entry) fprintf(out, "%s|%s|%s|%s|%d\n", updated->filename,
            updated->owner, updated->group, updated->permissions, updated->encrypted);
    if (in) fclose(in);
    if (fclose(out) != 0 || rename("data/metadata.tmp", METADATA_DB) != 0) return SV_ERR;
    return SV_OK;
}

static int permission(const Session *s, const FileMetadata *m, int offset) {
    if (!s || !s->active) return 0;
    if (strcmp(s->role, "admin") == 0) return 1;
    int base = strcmp(s->username, m->owner) == 0 ? 0 : strcmp(s->group, m->group) == 0 ? 3 : 6;
    return m->permissions[base + offset] == (offset == 0 ? 'r' : offset == 1 ? 'w' : 'x');
}

static void sha256_hex(const char *text, char out[65]) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)text, strlen(text), digest);
    hex_encode(digest, sizeof(digest), out, 65);
}

void audit_event(const Session *s, const char *action, const char *filename, int success, const char *reason) {
    pthread_mutex_lock(&audit_lock);
    char previous[65] = "GENESIS", line[1024], hash[65], timestamp[32];
    FILE *fp = fopen(AUDIT_LOG, "r");
    char existing[1200];
    while (fp && fgets(existing, sizeof(existing), fp)) {
        char *last = strrchr(existing, '|');
        if (last) { last[strcspn(last, "\r\n")] = '\0'; sv_copy(previous, sizeof(previous), last + 1); }
    }
    if (fp) fclose(fp);
    time_t now = time(NULL); struct tm tm_value; localtime_r(&now, &tm_value);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z", &tm_value);
    snprintf(line, sizeof(line), "%s|%s|%s|%s|%s|%s|%s|%s",
             timestamp, s && s->active ? s->username : "anonymous",
             s && s->client_ip[0] ? s->client_ip : "local", action,
             filename && filename[0] ? filename : "-", success ? "SUCCESS" : "FAIL",
             reason ? reason : "-", previous);
    sha256_hex(line, hash);
    fp = fopen(AUDIT_LOG, "a");
    if (fp) { fprintf(fp, "%s|%s\n", line, hash); fclose(fp); }
    pthread_mutex_unlock(&audit_lock);
}

int audit_verify(char *msg, size_t size) {
    pthread_mutex_lock(&audit_lock);
    FILE *fp = fopen(AUDIT_LOG, "r");
    char line[1200], previous[65] = "GENESIS"; int number = 0;
    if (!fp) { pthread_mutex_unlock(&audit_lock); snprintf(msg, size, "Audit log unavailable"); return SV_ERR; }
    while (fgets(line, sizeof(line), fp)) {
        number++; line[strcspn(line, "\r\n")] = '\0';
        char *hash_sep = strrchr(line, '|');
        if (!hash_sep) goto invalid;
        char recorded[65]; sv_copy(recorded, sizeof(recorded), hash_sep + 1); *hash_sep = '\0';
        char *prev_sep = strrchr(line, '|');
        if (!prev_sep || strcmp(prev_sep + 1, previous) != 0) goto invalid;
        char computed[65]; sha256_hex(line, computed);
        if (strcmp(computed, recorded) != 0) goto invalid;
        sv_copy(previous, sizeof(previous), recorded);
    }
    fclose(fp); pthread_mutex_unlock(&audit_lock);
    snprintf(msg, size, "Audit chain verified (%d entries)", number); return SV_OK;
invalid:
    fclose(fp); pthread_mutex_unlock(&audit_lock);
    snprintf(msg, size, "Audit tampering detected at line %d", number); return SV_ERR;
}

int audit_read(const Session *s, char *out, size_t size) {
    if (!s || !s->active || (strcmp(s->role, "admin") && strcmp(s->role, "auditor"))) return SV_ERR;
    FILE *fp = fopen(AUDIT_LOG, "r"); size_t used = 0;
    if (!fp) return SV_ERR;
    while (used + 1 < size && fgets(out + used, (int)(size - used), fp)) used = strlen(out);
    fclose(fp); return SV_OK;
}

int vault_create(const Session *s, const char *filename, char *msg, size_t size) {
    char path[MAX_PATH_LEN]; FileMetadata meta = {0};
    if (!s || !s->active || validate_filename(filename) || build_path(filename, path, sizeof(path))) goto denied;
    pthread_mutex_lock(&metadata_lock);
    if (find_metadata(filename, NULL) == SV_OK) { pthread_mutex_unlock(&metadata_lock); snprintf(msg, size, "File already exists"); return SV_ERR; }
    FILE *fp = fopen(path, "wx");
    if (!fp) { pthread_mutex_unlock(&metadata_lock); snprintf(msg, size, "Cannot create file"); return SV_ERR; }
    fclose(fp); sv_copy(meta.filename, sizeof(meta.filename), filename);
    sv_copy(meta.owner, sizeof(meta.owner), s->username); sv_copy(meta.group, sizeof(meta.group), s->group);
    sv_copy(meta.permissions, sizeof(meta.permissions), "rw-r-----");
    int rc = save_metadata(&meta, 0); pthread_mutex_unlock(&metadata_lock);
    snprintf(msg, size, rc == SV_OK ? "File created" : "Metadata update failed"); audit_event(s, "CREATE", filename, rc == SV_OK, msg); return rc;
denied:
    snprintf(msg, size, "Authentication or filename validation failed"); audit_event(s, "CREATE", filename, 0, msg); return SV_ERR;
}

static int crypt_file(const char *path, int encrypt) {
    FILE *fp = fopen(path, "rb"); if (!fp) return SV_ERR;
    fseek(fp, 0, SEEK_END); long length = ftell(fp); rewind(fp);
    if (length < 0 || length > MAX_CONTENT_LEN * 16) { fclose(fp); return SV_ERR; }
    unsigned char *input = malloc((size_t)length + 1), *output = malloc((size_t)length + EVP_MAX_BLOCK_LENGTH + IV_BYTES);
    if (!input || !output || fread(input, 1, (size_t)length, fp) != (size_t)length) { fclose(fp); free(input); free(output); return SV_ERR; }
    fclose(fp);
    unsigned char iv[IV_BYTES]; const unsigned char *payload = input; int payload_len = (int)length;
    if (encrypt) { if (RAND_bytes(iv, sizeof(iv)) != 1) goto fail; }
    else {
        if (length < IV_BYTES) goto fail;
        memcpy(iv, input, IV_BYTES); payload += IV_BYTES; payload_len -= IV_BYTES;
    }
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); int out1 = 0, out2 = 0, ok;
    if (!ctx) goto fail;
    if (encrypt) {
        ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, vault_key, iv) &&
             EVP_EncryptUpdate(ctx, output + IV_BYTES, &out1, payload, payload_len) &&
             EVP_EncryptFinal_ex(ctx, output + IV_BYTES + out1, &out2);
        memcpy(output, iv, IV_BYTES); out1 += IV_BYTES;
    } else {
        ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, vault_key, iv) &&
             EVP_DecryptUpdate(ctx, output, &out1, payload, payload_len) &&
             EVP_DecryptFinal_ex(ctx, output + out1, &out2);
    }
    EVP_CIPHER_CTX_free(ctx); if (!ok) goto fail;
    char temp[MAX_PATH_LEN]; snprintf(temp, sizeof(temp), "%s.tmp", path);
    fp = fopen(temp, "wb"); if (!fp) goto fail;
    size_t total = (size_t)(out1 + out2);
    ok = fwrite(output, 1, total, fp) == total && fclose(fp) == 0 && rename(temp, path) == 0;
    free(input); free(output); return ok ? SV_OK : SV_ERR;
fail:
    free(input); free(output); return SV_ERR;
}

int vault_write(const Session *s, const char *filename, const char *content, char *msg, size_t size) {
    FileMetadata m; char path[MAX_PATH_LEN];
    if (find_metadata(filename, &m) || !permission(s, &m, 1) || m.encrypted || build_path(filename, path, sizeof(path))) {
        snprintf(msg, size, "Permission denied, missing file, or encrypted file"); audit_event(s, "WRITE", filename, 0, msg); return SV_ERR;
    }
    sem_wait(file_access_sem); FILE *fp = fopen(path, "w");
    int ok = 0;
    if (fp) { ok = fputs(content, fp) >= 0; if (fclose(fp) != 0) ok = 0; }
    sem_post(file_access_sem);
    snprintf(msg, size, ok ? "File written" : "Write failed"); audit_event(s, "WRITE", filename, ok, msg); return ok ? SV_OK : SV_ERR;
}

int vault_read(const Session *s, const char *filename, char *out, size_t size) {
    FileMetadata m; char path[MAX_PATH_LEN], temp[MAX_PATH_LEN]; const char *read_path;
    if (find_metadata(filename, &m) || !permission(s, &m, 0) || build_path(filename, path, sizeof(path))) { audit_event(s, "READ", filename, 0, "Permission denied"); return SV_ERR; }
    sem_wait(file_access_sem); read_path = path;
    if (m.encrypted) {
        snprintf(temp, sizeof(temp), "%s.readtmp", path);
        FILE *src = fopen(path, "rb"), *dst = fopen(temp, "wb");
        if (!src || !dst) { if (src) fclose(src); if (dst) fclose(dst); sem_post(file_access_sem); return SV_ERR; }
        char b[4096]; size_t n; while ((n = fread(b, 1, sizeof(b), src)) > 0) fwrite(b, 1, n, dst);
        fclose(src); fclose(dst);
        if (crypt_file(temp, 0) != SV_OK) { unlink(temp); sem_post(file_access_sem); return SV_ERR; }
        read_path = temp;
    }
    FILE *fp = fopen(read_path, "r"); size_t n = fp ? fread(out, 1, size - 1, fp) : 0;
    if (fp) fclose(fp); out[n] = '\0'; if (m.encrypted) unlink(temp); sem_post(file_access_sem);
    audit_event(s, "READ", filename, fp != NULL, fp ? "OK" : "Read failed"); return fp ? SV_OK : SV_ERR;
}

int vault_delete(const Session *s, const char *filename, char *msg, size_t size) {
    FileMetadata m; char path[MAX_PATH_LEN];
    if (find_metadata(filename, &m) || (!s->active || (strcmp(s->role, "admin") && strcmp(s->username, m.owner))) || build_path(filename, path, sizeof(path))) { snprintf(msg, size, "Permission denied"); return SV_ERR; }
    pthread_mutex_lock(&metadata_lock); int ok = unlink(path) == 0 && save_metadata(&m, 1) == SV_OK; pthread_mutex_unlock(&metadata_lock);
    snprintf(msg, size, ok ? "File deleted" : "Delete failed"); audit_event(s, "DELETE", filename, ok, msg); return ok ? SV_OK : SV_ERR;
}

int vault_list(const Session *s, char *out, size_t size) {
    FILE *fp = fopen(METADATA_DB, "r"); char line[512]; size_t used = 0; out[0] = '\0';
    if (!s || !s->active || !fp) return SV_ERR;
    while (fgets(line, sizeof(line), fp)) {
        FileMetadata m;
        if (sscanf(line, "%128[^|]|%63[^|]|%63[^|]|%9[^|]|%d", m.filename, m.owner, m.group, m.permissions, &m.encrypted) == 5 && permission(s, &m, 0)) {
            int n = snprintf(out + used, size - used, "%s owner=%s group=%s perms=%s encrypted=%s\n",
                             m.filename, m.owner, m.group, m.permissions, m.encrypted ? "yes" : "no");
            if (n < 0 || (size_t)n >= size - used) break; used += (size_t)n;
        }
    }
    fclose(fp); return SV_OK;
}

int vault_chmod(const Session *s, const char *filename, const char *permissions, char *msg, size_t size) {
    FileMetadata m;
    if (!permissions || strlen(permissions) != 9 || find_metadata(filename, &m) ||
        (!s->active || (strcmp(s->role, "admin") && strcmp(s->username, m.owner)))) goto bad;
    for (int i = 0; i < 9; i++) if (permissions[i] != '-' && permissions[i] != "rwx"[i % 3]) goto bad;
    sv_copy(m.permissions, sizeof(m.permissions), permissions);
    pthread_mutex_lock(&metadata_lock); int rc = save_metadata(&m, 0); pthread_mutex_unlock(&metadata_lock);
    snprintf(msg, size, rc == SV_OK ? "Permissions updated" : "Metadata update failed"); audit_event(s, "CHMOD", filename, rc == SV_OK, msg); return rc;
bad:
    snprintf(msg, size, "Invalid permissions or permission denied"); audit_event(s, "CHMOD", filename, 0, msg); return SV_ERR;
}

static int change_encryption(const Session *s, const char *filename, int encrypt, char *msg, size_t size) {
    FileMetadata m; char path[MAX_PATH_LEN];
    if (find_metadata(filename, &m) || !permission(s, &m, 1) || m.encrypted == encrypt || build_path(filename, path, sizeof(path))) goto bad;
    sem_wait(file_access_sem); int rc = crypt_file(path, encrypt); sem_post(file_access_sem);
    if (rc == SV_OK) { m.encrypted = encrypt; pthread_mutex_lock(&metadata_lock); rc = save_metadata(&m, 0); pthread_mutex_unlock(&metadata_lock); }
    snprintf(msg, size, rc == SV_OK ? (encrypt ? "File encrypted" : "File decrypted") : "Crypto operation failed");
    audit_event(s, encrypt ? "ENCRYPT" : "DECRYPT", filename, rc == SV_OK, msg); return rc;
bad:
    snprintf(msg, size, "Permission denied or invalid encryption state"); audit_event(s, encrypt ? "ENCRYPT" : "DECRYPT", filename, 0, msg); return SV_ERR;
}

int vault_encrypt(const Session *s, const char *f, char *m, size_t z) { return change_encryption(s, f, 1, m, z); }
int vault_decrypt(const Session *s, const char *f, char *m, size_t z) { return change_encryption(s, f, 0, m, z); }
