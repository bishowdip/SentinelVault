#ifndef SENTINELVAULT_CONFIG_H
#define SENTINELVAULT_CONFIG_H

#define DEFAULT_SERVER_PORT 9090
#define MAX_CLIENTS 32
#define MAX_COMMAND_LEN 2048
#define MAX_RESPONSE_LEN 16384
#define MAX_USERNAME_LEN 64
#define MAX_FILENAME_LEN 128
#define MAX_PATH_LEN 512
#define MAX_CONTENT_LEN 8192
#define MAX_JOBS 128
#define WORKER_COUNT 4
#define FILE_ACCESS_LIMIT 2

#define VAULT_DIR "data/vault"
#define USERS_DB "data/users.db"
#define METADATA_DB "data/metadata.db"
#define AUDIT_LOG "data/audit.log"

#endif

