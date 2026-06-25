#include "file_permission_demo.h"

static void fill_permission_result(mode_t mode, FilePermissionResult *result) {
    result->mode = mode & 0777;
    result->owner_read = (mode & S_IRUSR) != 0;
    result->owner_write = (mode & S_IWUSR) != 0;
    result->group_read = (mode & S_IRGRP) != 0;
    result->group_write = (mode & S_IWGRP) != 0;
    result->other_read = (mode & S_IROTH) != 0;
    result->other_write = (mode & S_IWOTH) != 0;
}

int run_file_permission_demo(const char *path, FilePermissionResult *result, FILE *log_file) {
    struct stat st;
    int fd;
    const char *content = "permission demo\n";

    if (!path || !result) return SV_ERR;
    memset(result, 0, sizeof(*result));

    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd < 0) return SV_ERR;
    if (write(fd, content, strlen(content)) < 0) {
        close(fd);
        unlink(path);
        return SV_ERR;
    }
    close(fd);

    if (chmod(path, 0640) != 0 || stat(path, &st) != 0) {
        unlink(path);
        return SV_ERR;
    }

    fill_permission_result(st.st_mode, result);

    if (log_file) {
        fprintf(log_file, "Created file: %s\n", path);
        fprintf(log_file, "Mode after chmod: %03o\n", (unsigned int)result->mode);
        fprintf(log_file, "Owner read/write: %s/%s\n",
                result->owner_read ? "yes" : "no",
                result->owner_write ? "yes" : "no");
        fprintf(log_file, "Group read/write: %s/%s\n",
                result->group_read ? "yes" : "no",
                result->group_write ? "yes" : "no");
        fprintf(log_file, "Others read/write: %s/%s\n",
                result->other_read ? "yes" : "no",
                result->other_write ? "yes" : "no");
    }

    unlink(path);
    return SV_OK;
}

void run_permission_demo(void) {
    FilePermissionResult result;
    puts("=== POSIX FILE PERMISSION DEMO ===");
    if (run_file_permission_demo("permission_demo.tmp", &result, stdout) != SV_OK)
        puts("Permission demo failed");
}
