#ifndef SENTINELVAULT_FILE_PERMISSION_DEMO_H
#define SENTINELVAULT_FILE_PERMISSION_DEMO_H

#include "common.h"

typedef struct {
    mode_t mode;
    int owner_read;
    int owner_write;
    int group_read;
    int group_write;
    int other_read;
    int other_write;
} FilePermissionResult;

int run_file_permission_demo(const char *path, FilePermissionResult *result, FILE *log_file);
void run_permission_demo(void);

#endif
