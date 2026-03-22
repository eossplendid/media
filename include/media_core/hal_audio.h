/**
 * @file hal_audio.h
 * @brief HAL: hal_audio_in / hal_audio_out for platform audio driver adapters.
 */
#ifndef MEDIA_CORE_HAL_AUDIO_H
#define MEDIA_CORE_HAL_AUDIO_H

#include "../media_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_audio_config {
    const char *device;
    uint32_t sample_rate;
    uint32_t channels;
    media_format_t format;
    uint32_t period_frames;
    uint32_t frame_ms;
} hal_audio_config_t;

typedef struct hal_audio_in_ops {
    int (*open)(void **handle, const hal_audio_config_t *config);
    int (*read)(void *handle, uint8_t *buf, size_t frames, int64_t *pts);
    void (*close)(void *handle);
} hal_audio_in_ops_t;

typedef struct hal_audio_out_ops {
    int (*open)(void **handle, const hal_audio_config_t *config);
    int (*write)(void *handle, const uint8_t *buf, size_t frames);
    void (*close)(void *handle);
} hal_audio_out_ops_t;

void hal_register_audio_in(const hal_audio_in_ops_t *ops);
void hal_register_audio_out(const hal_audio_out_ops_t *ops);

const hal_audio_in_ops_t* hal_get_audio_in_ops(void);
const hal_audio_out_ops_t* hal_get_audio_out_ops(void);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CORE_HAL_AUDIO_H */
