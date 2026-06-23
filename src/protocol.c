#include "protocol.h"
#include "memory_simulator.h"
#include "scheduler.h"

static void respond(char *out, size_t size, int ok, const char *message) {
    snprintf(out, size, "%s %s\n", ok ? "OK" : "ERROR", message ? message : "");
}

int handle_command(char *input, Session *s, char *out, size_t size, int *should_quit) {
    char *save = NULL, *command;
    input[strcspn(input, "\r\n")] = '\0';
    command = strtok_r(input, " ", &save);
    if (!command) { respond(out, size, 0, "Empty command"); return SV_ERR; }
    for (char *p = command; *p; p++) *p = (char)toupper((unsigned char)*p);
    if (!strcmp(command, "HELP")) {
        snprintf(out, size, "DATA HELP AUTH WHOAMI CREATE WRITE READ DELETE LIST CHMOD ENCRYPT DECRYPT AUDIT VERIFY_AUDIT MEMSIM SCHEDSIM LOGOUT QUIT\n");
        return SV_OK;
    }
    if (!strcmp(command, "QUIT")) { *should_quit = 1; respond(out, size, 1, "Goodbye"); return SV_OK; }
    if (!strcmp(command, "AUTH")) {
        char *username = strtok_r(NULL, " ", &save), *password = strtok_r(NULL, " ", &save);
        if (!username || !password) { respond(out, size, 0, "Usage: AUTH username password"); return SV_ERR; }
        if (authenticate_user(username, password, s) == SV_OK) {
            char message[128]; snprintf(message, sizeof(message), "Authenticated as %s", s->username);
            respond(out, size, 1, message); audit_event(s, "AUTH", "-", 1, "OK"); return SV_OK;
        }
        audit_event(s, "AUTH", "-", 0, "Invalid credentials"); respond(out, size, 0, "Invalid credentials"); return SV_ERR;
    }
    if (!s->active) { respond(out, size, 0, "Authentication required"); return SV_ERR; }
    if (!strcmp(command, "WHOAMI")) {
        snprintf(out, size, "DATA username=%s group=%s role=%s\n", s->username, s->group, s->role); return SV_OK;
    }
    if (!strcmp(command, "LOGOUT")) { audit_event(s, "LOGOUT", "-", 1, "OK"); logout_user(s); respond(out, size, 1, "Logged out"); return SV_OK; }
    char message[MAX_RESPONSE_LEN] = {0}, *filename = strtok_r(NULL, " ", &save);
    int rc = SV_ERR;
    if (!strcmp(command, "CREATE") && filename) rc = vault_create(s, filename, message, sizeof(message));
    else if (!strcmp(command, "WRITE") && filename && save && *save) rc = vault_write(s, filename, save, message, sizeof(message));
    else if (!strcmp(command, "READ") && filename) {
        rc = vault_read(s, filename, message, sizeof(message));
        if (rc == SV_OK) { snprintf(out, size, "DATA %s\n", message); return SV_OK; }
        sv_copy(message, sizeof(message), "Permission denied or file unavailable");
    } else if (!strcmp(command, "DELETE") && filename) rc = vault_delete(s, filename, message, sizeof(message));
    else if (!strcmp(command, "LIST")) {
        rc = vault_list(s, message, sizeof(message));
        if (rc == SV_OK) { snprintf(out, size, "DATA %s%s", message[0] ? message : "(no accessible files)\n", message[0] && message[strlen(message)-1] != '\n' ? "\n" : ""); return SV_OK; }
    } else if (!strcmp(command, "CHMOD") && filename) {
        char *permissions = strtok_r(NULL, " ", &save);
        if (permissions) rc = vault_chmod(s, filename, permissions, message, sizeof(message));
    } else if (!strcmp(command, "ENCRYPT") && filename) rc = vault_encrypt(s, filename, message, sizeof(message));
    else if (!strcmp(command, "DECRYPT") && filename) rc = vault_decrypt(s, filename, message, sizeof(message));
    else if (!strcmp(command, "AUDIT")) {
        rc = audit_read(s, message, sizeof(message));
        if (rc == SV_OK) { snprintf(out, size, "DATA %s", message); return SV_OK; }
        sv_copy(message, sizeof(message), "Admin or auditor role required");
    } else if (!strcmp(command, "VERIFY_AUDIT")) {
        if (strcmp(s->role, "admin") && strcmp(s->role, "auditor")) sv_copy(message, sizeof(message), "Admin or auditor role required");
        else rc = audit_verify(message, sizeof(message));
    } else if (!strcmp(command, "MEMSIM")) {
        const int refs[] = {7,0,1,2,0,3,0,4,2,3,0,3,2};
        MemoryStats f = simulate_fifo(refs, 13, 3, NULL), l = simulate_lru(refs, 13, 3, NULL);
        snprintf(message, sizeof(message), "FIFO faults=%d hits=%d; LRU faults=%d hits=%d", f.faults, f.hits, l.faults, l.hits);
        rc = SV_OK;
    } else if (!strcmp(command, "SCHEDSIM")) {
        sv_copy(message, sizeof(message), "Run ./bin/scheduler_demo for the detailed scheduler trace"); rc = SV_OK;
    } else sv_copy(message, sizeof(message), "Invalid command or missing arguments");
    respond(out, size, rc == SV_OK, message); return rc;
}
