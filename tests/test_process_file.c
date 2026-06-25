#include "file_permission_demo.h"
#include "process_demo.h"
#include <assert.h>

int main(void) {
    ProcessDemoResult process_result;
    FilePermissionResult file_result;

    assert(run_process_ipc_demo(&process_result, NULL) == SV_OK);
    assert(process_result.parent_pid > 0);
    assert(process_result.child_pid > 0);
    assert(strstr(process_result.message, "pipe") != NULL);
    assert(WIFEXITED(process_result.child_exit_status));

    assert(run_file_permission_demo("permission_demo.tmp", &file_result, NULL) == SV_OK);
    assert(file_result.mode == 0640);
    assert(file_result.owner_read && file_result.owner_write);
    assert(file_result.group_read && !file_result.group_write);
    assert(!file_result.other_read && !file_result.other_write);
    assert(access("permission_demo.tmp", F_OK) != 0);

    puts("test_process_file: PASS");
    return 0;
}
