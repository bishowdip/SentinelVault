#include "protocol.h"
#include <assert.h>

static int command(Session *session, const char *text, char *response, size_t size) {
    char input[MAX_COMMAND_LEN + 1];
    int quit = 0;
    snprintf(input, sizeof(input), "%s", text);
    return handle_command(input, session, response, size, &quit);
}

int main(void) {
    char response[MAX_RESPONSE_LEN];
    Session admin = {0}, guest = {0};

    assert(vault_init() == SV_OK);
    assert(command(&admin, "CREATE forbidden.txt", response, sizeof(response)) == SV_ERR);
    assert(strstr(response, "Authentication required"));
    assert(command(&admin, "AUTH admin admin123", response, sizeof(response)) == SV_OK);
    assert(command(&admin, "CREATE protocol_test.txt", response, sizeof(response)) == SV_OK ||
           strstr(response, "already exists"));
    assert(command(&admin, "WRITE protocol_test.txt Secret protocol evidence", response, sizeof(response)) == SV_OK);
    assert(command(&admin, "CHMOD protocol_test.txt rw-------", response, sizeof(response)) == SV_OK);
    assert(command(&admin, "ENCRYPT protocol_test.txt", response, sizeof(response)) == SV_OK);
    assert(command(&admin, "READ protocol_test.txt", response, sizeof(response)) == SV_OK);
    assert(strstr(response, "Secret protocol evidence"));

    assert(command(&guest, "AUTH guest guest123", response, sizeof(response)) == SV_OK);
    assert(command(&guest, "READ protocol_test.txt", response, sizeof(response)) == SV_ERR);
    assert(strstr(response, "Permission denied"));

    assert(command(&admin, "DECRYPT protocol_test.txt", response, sizeof(response)) == SV_OK);
    assert(command(&admin, "DELETE protocol_test.txt", response, sizeof(response)) == SV_OK);
    assert(command(&admin, "VERIFY_AUDIT", response, sizeof(response)) == SV_OK);
    puts("test_protocol: PASS");
    return 0;
}
