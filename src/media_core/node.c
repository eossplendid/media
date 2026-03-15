/**
 * @file node.c
 * @brief Helpers for buffer allocation and node port handling.
 */
#include "../../include/media_core/node.h"
#include "../../include/media_types.h"
#include <stdlib.h>
#include <string.h>

media_buffer_t* media_buffer_alloc(size_t size) {
    media_buffer_t *buf = (media_buffer_t *)calloc(1, sizeof(media_buffer_t));
    if (!buf) return NULL;
    buf->data = (uint8_t *)malloc(size);
    if (!buf->data) { free(buf); return NULL; }
    buf->size = size;
    buf->release = NULL;
    return buf;
}

void media_buffer_free(media_buffer_t *buf) {
    if (!buf) return;
    if (buf->release) buf->release(buf);
    else {
        free(buf->data);
        buf->data = NULL;
        buf->size = 0;
    }
    free(buf);
}

void media_caps_from_format(media_caps_t *caps, uint32_t sample_rate, uint32_t channels,
                            media_format_t format) {
    if (!caps) return;
    caps->sample_rate = sample_rate;
    caps->channels = channels;
    caps->format = format;
    caps->bytes_per_sample = media_format_bytes(format) * channels;
}
