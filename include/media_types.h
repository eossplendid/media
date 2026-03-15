/**
 * @file media_types.h
 * @brief Common media types: sample rate, channels, format, timestamp.
 */
#ifndef MEDIA_TYPES_H
#define MEDIA_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Audio sample format */
typedef enum {
    MEDIA_FORMAT_S16 = 0,
    MEDIA_FORMAT_S32,
    MEDIA_FORMAT_F32,
    MEDIA_FORMAT_U8,
    MEDIA_FORMAT_UNKNOWN
} media_format_t;

/** Capabilities / format descriptor for a port */
typedef struct media_caps {
    uint32_t sample_rate;
    uint32_t channels;
    media_format_t format;
    uint32_t bytes_per_sample;  /* derived: e.g. 2 for S16 */
    uint32_t frame_ms;          /* 帧长(ms)，0=未指定/灵活，用于 prepare 协商 */
} media_caps_t;

/** Timestamp in microseconds (optional for sync) */
typedef int64_t media_ts_t;

/** Get bytes per sample for format */
static inline uint32_t media_format_bytes(media_format_t f) {
    switch (f) {
        case MEDIA_FORMAT_S16: return 2;
        case MEDIA_FORMAT_S32:
        case MEDIA_FORMAT_F32: return 4;
        case MEDIA_FORMAT_U8:  return 1;
        default: return 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_TYPES_H */
