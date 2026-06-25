#include "process_demo.h"

int run_process_ipc_demo(ProcessDemoResult *result, FILE *log_file) {
    int fds[2];
    char buffer[128];
    ssize_t nread;

    if (!result) return SV_ERR;
    memset(result, 0, sizeof(*result));
    result->parent_pid = getpid();

    if (pipe(fds) != 0) return SV_ERR;

    pid_t child = fork();
    if (child < 0) {
        close(fds[0]);
        close(fds[1]);
        return SV_ERR;
    }

    if (child == 0) {
        const char *message = "child process sent data through pipe";
        close(fds[0]);
        (void)write(fds[1], message, strlen(message));
        close(fds[1]);
        _exit(0);
    }

    result->child_pid = child;
    close(fds[1]);
    nread = read(fds[0], buffer, sizeof(buffer) - 1);
    close(fds[0]);
    if (nread < 0) return SV_ERR;
    buffer[nread] = '\0';
    sv_copy(result->message, sizeof(result->message), buffer);

    if (waitpid(child, &result->child_exit_status, 0) < 0) return SV_ERR;

    if (log_file) {
        fprintf(log_file, "Parent PID: %ld\n", (long)result->parent_pid);
        fprintf(log_file, "Child PID: %ld\n", (long)result->child_pid);
        fprintf(log_file, "Pipe message: %s\n", result->message);
        fprintf(log_file, "Child exited normally: %s\n",
                WIFEXITED(result->child_exit_status) ? "yes" : "no");
    }

    return WIFEXITED(result->child_exit_status) ? SV_OK : SV_ERR;
}

void run_process_demo(void) {
    ProcessDemoResult result;
    puts("=== PROCESS CREATION AND PIPE IPC DEMO ===");
    if (run_process_ipc_demo(&result, stdout) != SV_OK)
        puts("Process demo failed");
}
