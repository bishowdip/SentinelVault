#ifndef SENTINELVAULT_VAULT_H
#define SENTINELVAULT_VAULT_H

#include "common.h"

typedef struct {
    int active;
    char username[MAX_USERNAME_LEN];
    char group[MAX_USERNAME_LEN];
    char role[32];
    char client_ip[INET_ADDRSTRLEN];
} Session;

int vault_init(void);
int authenticate_user(const char *username, const char *password, Session *session);
void logout_user(Session *session);
int validate_filename(const char *filename);
int vault_create(const Session *session, const char *filename, char *msg, size_t size);
int vault_write(const Session *session, const char *filename, const char *content, char *msg, size_t size);
int vault_read(const Session *session, const char *filename, char *out, size_t size);
int vault_delete(const Session *session, const char *filename, char *msg, size_t size);
int vault_list(const Session *session, char *out, size_t size);
int vault_chmod(const Session *session, const char *filename, const char *permissions, char *msg, size_t size);
int vault_encrypt(const Session *session, const char *filename, char *msg, size_t size);
int vault_decrypt(const Session *session, const char *filename, char *msg, size_t size);
int audit_read(const Session *session, char *out, size_t size);
int audit_verify(char *msg, size_t size);
void audit_event(const Session *session, const char *action, const char *filename,
                 int success, const char *reason);

#endif

