#include "vault.h"
#include <assert.h>
int main(void) {
    assert(vault_init() == SV_OK);
    assert(validate_filename("incident_01.txt") == SV_OK);
    assert(validate_filename("../../etc/passwd") == SV_ERR);
    Session admin = {0};
    assert(authenticate_user("admin", "admin123", &admin) == SV_OK);
    assert(strcmp(admin.role, "admin") == 0);
    Session bad = {0};
    assert(authenticate_user("admin", "wrong", &bad) == SV_ERR);
    assert(authenticate_user("guest", "wrong", &bad) == SV_ERR);
    assert(authenticate_user("guest", "wrong", &bad) == SV_ERR);
    assert(authenticate_user("guest", "wrong", &bad) == SV_ERR);
    assert(authenticate_user("guest", "guest123", &bad) == SV_ERR);
    char message[256];
    assert(audit_verify(message, sizeof(message)) == SV_OK);
    puts("test_vault: PASS");
    return 0;
}
