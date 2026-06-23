#include "server.h"

typedef struct {
    int fd;
    struct sockaddr_in address;
} ClientContext;

static int send_all(int fd, const char *data) {
    size_t sent = 0, length = strlen(data);
    while (sent < length) {
        ssize_t n = send(fd, data + sent, length - sent, 0);
        if (n <= 0) return SV_ERR;
        sent += (size_t)n;
    }
    return SV_OK;
}

static void *handle_client(void *ptr) {
    ClientContext *context = ptr;
    Session session = {0};
    inet_ntop(AF_INET, &context->address.sin_addr, session.client_ip, sizeof(session.client_ip));
    printf("[SERVER] Client connected: %s:%d\n", session.client_ip, ntohs(context->address.sin_port));
    send_all(context->fd, "OK Connected to SentinelVault-C. Type HELP.\n");
    char command[MAX_COMMAND_LEN + 1], response[MAX_RESPONSE_LEN]; size_t used = 0;
    int quit = 0;
    while (!quit) {
        ssize_t n = recv(context->fd, command + used, MAX_COMMAND_LEN - used, 0);
        if (n <= 0) break;
        used += (size_t)n; command[used] = '\0';
        char *newline;
        while ((newline = strchr(command, '\n')) != NULL) {
            size_t line_len = (size_t)(newline - command) + 1;
            char line[MAX_COMMAND_LEN + 1];
            memcpy(line, command, line_len); line[line_len] = '\0';
            memmove(command, command + line_len, used - line_len); used -= line_len; command[used] = '\0';
            handle_command(line, &session, response, sizeof(response), &quit);
            if (send_all(context->fd, response) != SV_OK) { quit = 1; break; }
        }
        if (used == MAX_COMMAND_LEN) { send_all(context->fd, "ERROR Command too long\n"); break; }
    }
    if (session.active) audit_event(&session, "DISCONNECT", "-", 1, "Connection closed");
    close(context->fd); free(context);
    puts("[SERVER] Client disconnected safely");
    return NULL;
}

int start_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0), option = 1;
    if (server_fd < 0) { perror("socket"); return SV_ERR; }
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_ANY), .sin_port = htons((uint16_t)port)};
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0 || listen(server_fd, MAX_CLIENTS) < 0) {
        perror("bind/listen"); close(server_fd); return SV_ERR;
    }
    printf("[SERVER] SentinelVault-C listening on port %d\n", port);
    for (;;) {
        ClientContext *context = calloc(1, sizeof(*context)); socklen_t length = sizeof(context->address);
        if (!context) continue;
        context->fd = accept(server_fd, (struct sockaddr *)&context->address, &length);
        if (context->fd < 0) { free(context); if (errno == EINTR) continue; perror("accept"); break; }
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, context) != 0) { close(context->fd); free(context); continue; }
        pthread_detach(thread);
    }
    close(server_fd); return SV_ERR;
}

