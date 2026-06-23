#ifndef SENTINELVAULT_PROTOCOL_H
#define SENTINELVAULT_PROTOCOL_H

#include "vault.h"

int handle_command(char *input, Session *session, char *response, size_t size, int *should_quit);

#endif

