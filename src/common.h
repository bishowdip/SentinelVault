#ifndef SENTINELVAULT_COMMON_H
#define SENTINELVAULT_COMMON_H

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define SV_OK 0
#define SV_ERR -1

static inline void sv_copy(char *dst, size_t size, const char *src) {
    if (size > 0) {
        snprintf(dst, size, "%s", src ? src : "");
    }
}

#endif
