#include "common.h"

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "Usage: %s host port\n", argv[0]); return EXIT_FAILURE; }
    int fd = socket(AF_INET, SOCK_STREAM, 0), port = atoi(argv[2]);
    struct sockaddr_in server = {.sin_family = AF_INET, .sin_port = htons((uint16_t)port)};
    if (fd < 0 || inet_pton(AF_INET, argv[1], &server.sin_addr) != 1 ||
        connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0) { perror("connect"); if (fd >= 0) close(fd); return EXIT_FAILURE; }
    char input[MAX_COMMAND_LEN + 2], response[MAX_RESPONSE_LEN];
    ssize_t n = recv(fd, response, sizeof(response) - 1, 0);
    if (n > 0) { response[n] = '\0'; fputs(response, stdout); }
    while (printf("> "), fflush(stdout), fgets(input, sizeof(input), stdin)) {
        if (!strchr(input, '\n')) { fprintf(stderr, "Command too long\n"); int c; while ((c = getchar()) != '\n' && c != EOF) {} continue; }
        if (send(fd, input, strlen(input), 0) <= 0) break;
        n = recv(fd, response, sizeof(response) - 1, 0);
        if (n <= 0) break;
        response[n] = '\0'; fputs(response, stdout);
        if (!strncmp(input, "QUIT", 4)) break;
    }
    close(fd); return EXIT_SUCCESS;
}

