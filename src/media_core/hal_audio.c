/**
 * @file hal_audio.c
 * @brief HAL audio registration and ops accessor.
 */
#include "../../include/media_core/hal_audio.h"
#include <stddef.h>

static const hal_audio_in_ops_t *g_audio_in_ops;
static const hal_audio_out_ops_t *g_audio_out_ops;

void hal_register_audio_in(const hal_audio_in_ops_t *ops) {
    g_audio_in_ops = ops;
}

void hal_register_audio_out(const hal_audio_out_ops_t *ops) {
    g_audio_out_ops = ops;
}

const hal_audio_in_ops_t* hal_get_audio_in_ops(void) {
    return g_audio_in_ops;
}

const hal_audio_out_ops_t* hal_get_audio_out_ops(void) {
    return g_audio_out_ops;
}
