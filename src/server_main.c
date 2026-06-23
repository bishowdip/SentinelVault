#include "server.h"
int main(int argc, char **argv) {
    int port = argc > 1 ? atoi(argv[1]) : DEFAULT_SERVER_PORT;
    if (port < 1 || port > 65535 || vault_init() != SV_OK) {
        fprintf(stderr, "Failed to initialize SentinelVault-C\n"); return EXIT_FAILURE;
    }
    signal(SIGPIPE, SIG_IGN);
    return start_server(port) == SV_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

