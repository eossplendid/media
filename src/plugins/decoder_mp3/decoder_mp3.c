/**
 * @file decoder_mp3.c
 * @brief MP3 decoder node (1 in, 1 out); uses minimp3 for decoding.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define MINIMP3_NO_SIMD        /* 使用纯 C 实现，避免 SIMD 代码路径 */
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_core/media_debug.h"
#include "media_types.h"
#include <stdio.h>

typedef struct {
    mp3dec_t dec;
    uint8_t *pending;
    size_t pending_size;
    size_t pending_cap;
    uint32_t sample_rate;
    uint32_t channels;
} decoder_mp3_priv_t;

#define PENDING_INIT_CAP 4096

static int decoder_mp3_init(media_node_t *node, const node_config_t *config) {
    (void)config;
    decoder_mp3_priv_t *p = (decoder_mp3_priv_t *)node->private_data;
    if (!p) return -1;
    mp3dec_init(&p->dec);
    return 0;
}

static int decoder_mp3_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    decoder_mp3_priv_t *p = (decoder_mp3_priv_t *)node->private_data;
    if (!p || !out_caps) return -1;
    if (port_index == 0) {
        out_caps->sample_rate = p->sample_rate ? p->sample_rate : 44100;
        out_caps->channels = p->channels ? p->channels : 1;
        out_caps->format = MEDIA_FORMAT_S16;
        out_caps->bytes_per_sample = 2 * p->channels;
        out_caps->codec = MEDIA_CODEC_NONE;
    }
    return 0;
}

static int decoder_mp3_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    (void)node;(void)port_index;(void)caps;
    return 0;
}

static int append_pending(decoder_mp3_priv_t *p, const uint8_t *data, size_t size) {
    if (p->pending_size + size > p->pending_cap) {
        size_t new_cap = p->pending_cap ? p->pending_cap * 2 : PENDING_INIT_CAP;
        while (new_cap < p->pending_size + size) new_cap *= 2;
        uint8_t *n = (uint8_t *)realloc(p->pending, new_cap);
        if (!n) return -1;
        p->pending = n;
        p->pending_cap = new_cap;
    }
    memcpy(p->pending + p->pending_size, data, size);
    p->pending_size += size;
    return 0;
}

static int decoder_mp3_process(media_node_t *node) {
    decoder_mp3_priv_t *p = (decoder_mp3_priv_t *)node->private_data;
    media_buffer_t *in_buf = node->input_buffers[0];
    if (!p) return 0;

    /* Append new input to pending */
    if (in_buf && in_buf->data && in_buf->size > 0) {
        if (append_pending(p, in_buf->data, in_buf->size) != 0) return -1;
    }

    if (p->pending_size < 4) return 0;  /* Need at least header */

    mp3dec_frame_info_t info;
    int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int samples = mp3dec_decode_frame(&p->dec, p->pending, (int)p->pending_size, pcm, &info);
    if (samples <= 0 && info.frame_bytes <= 0) {
        /* Skip invalid byte and retry */
        if (p->pending_size > 0) {
            memmove(p->pending, p->pending + 1, p->pending_size - 1);
            p->pending_size--;
        }
        return 0;
    }

    if (info.frame_bytes > 0) {
        memmove(p->pending, p->pending + info.frame_bytes, p->pending_size - info.frame_bytes);
        p->pending_size -= info.frame_bytes;
    }

    if (samples <= 0) return 0;

    p->sample_rate = info.hz;
    p->channels = info.channels;

    size_t out_size = (size_t)samples * sizeof(int16_t);
    media_buffer_t *out_buf = media_buffer_alloc(out_size);
    if (!out_buf) return -1;
    memcpy(out_buf->data, pcm, out_size);
    out_buf->size = out_size;
    out_buf->caps.sample_rate = p->sample_rate;
    out_buf->caps.channels = p->channels;
    out_buf->caps.format = MEDIA_FORMAT_S16;
    out_buf->caps.bytes_per_sample = 2 * p->channels;
    out_buf->caps.codec = MEDIA_CODEC_NONE;
    node->output_buffers[0] = out_buf;
    if (media_debug_enabled()) fprintf(stderr, "[decoder_mp3] decoded %d samples %uHz %u ch\n", samples, p->sample_rate, p->channels);
    return 0;
}

static int decoder_mp3_flush(media_node_t *node) {
    decoder_mp3_priv_t *p = (decoder_mp3_priv_t *)node->private_data;
    if (p) {
        p->pending_size = 0;
    }
    if (node->output_buffers[0]) {
        media_buffer_free(node->output_buffers[0]);
        node->output_buffers[0] = NULL;
    }
    return 0;
}

static void decoder_mp3_destroy(media_node_t *node) {
    decoder_mp3_priv_t *p = (decoder_mp3_priv_t *)node->private_data;
    if (p) {
        free(p->pending);
        free(p);
    }
    if (node->instance_id) free((void*)node->instance_id);
    free(node);
}

static const node_ops_t decoder_mp3_ops = {
    .init = decoder_mp3_init,
    .get_caps = decoder_mp3_get_caps,
    .set_caps = decoder_mp3_set_caps,
    .process = decoder_mp3_process,
    .flush = decoder_mp3_flush,
    .destroy = decoder_mp3_destroy,
};

static const node_descriptor_t decoder_mp3_desc = {
    .name = "decoder_mp3",
    .ops = &decoder_mp3_ops,
    .num_input_ports = 1,
    .num_output_ports = 1,
};

media_node_t* decoder_mp3_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &decoder_mp3_desc;
    node->instance_id = strdup(instance_id ? instance_id : "dec_mp3");
    node->num_input_ports = 1;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(decoder_mp3_priv_t));
    if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
    (void)config;
    return node;
}

void decoder_mp3_destroy_fn(media_node_t *node) {
    decoder_mp3_destroy(node);
}
