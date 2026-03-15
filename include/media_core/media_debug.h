/**
 * @file media_debug.h
 * @brief MEDIA_DEBUG=1 启用调试输出到 stderr
 */
#ifndef MEDIA_DEBUG_H
#define MEDIA_DEBUG_H

#include <stdlib.h>
#include <ctype.h>

static inline int media_debug_enabled(void) {
    const char *e = getenv("MEDIA_DEBUG");
    return e && (*e == '1' || *e == 'y' || *e == 'Y' ||
                 (isdigit((unsigned char)*e) && atoi(e) > 0));
}

#endif
