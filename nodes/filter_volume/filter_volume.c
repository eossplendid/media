/**
 * @file filter_volume.c
 * @brief Volume filter (1 in, 1 out): gain 0.0-1.0, supports S16/F32.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    float gain;
} filter_volume_priv_t;

static int filter_volume_init(media_node_t *node, const node_config_t *config) {
    filter_volume_priv_t *p = (filter_volume_priv_t *)node->private_data;
    if (!p) return -1;
    p->gain = 1.0f;
    if (config && config->opaque) {
        float *fg = (float *)config->opaque;
        if (*fg >= 0.f && *fg <= 1.f) p->gain = *fg;
    }
    return 0;
}

static int filter_volume_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    (void)node;
    (void)port_index;
    (void)out_caps;
    return 0;
}

static int filter_volume_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    (void)node;
    (void)port_index;
    (void)caps;
    return 0;
}

static int filter_volume_set_volume(media_node_t *node, float gain) {
    filter_volume_priv_t *p = (filter_volume_priv_t *)node->private_data;
    if (!p) return -1;
    if (gain < 0.f) gain = 0.f;
    if (gain > 1.f) gain = 1.f;
    p->gain = gain;
    return 0;
}

static int filter_volume_process(media_node_t *node) {
    filter_volume_priv_t *p = (filter_volume_priv_t *)node->private_data;
    media_buffer_t *in_buf = node->input_buffers[0];
    if (!p || !in_buf || !in_buf->data || in_buf->size == 0) return 0;

    media_caps_t caps = in_buf->caps;
    media_buffer_t *out_buf = media_buffer_alloc(in_buf->size);
    if (!out_buf) return 0;
    out_buf->pts = in_buf->pts;
    out_buf->caps = caps;
    out_buf->size = in_buf->size;

    if (p->gain >= 0.9999f && p->gain <= 1.0001f) {
        memcpy(out_buf->data, in_buf->data, in_buf->size);
    } else if (caps.format == MEDIA_FORMAT_S16) {
        int16_t *in = (int16_t *)in_buf->data;
        int16_t *out = (int16_t *)out_buf->data;
        size_t n = in_buf->size / 2;
        for (size_t i = 0; i < n; i++) {
            float v = (float)in[i] * p->gain;
            if (v > 32767.f) v = 32767.f;
            if (v < -32768.f) v = -32768.f;
            out[i] = (int16_t)v;
        }
    } else if (caps.format == MEDIA_FORMAT_F32) {
        float *in = (float *)in_buf->data;
        float *out = (float *)out_buf->data;
        size_t n = in_buf->size / 4;
        for (size_t i = 0; i < n; i++)
            out[i] = in[i] * p->gain;
    } else {
        memcpy(out_buf->data, in_buf->data, in_buf->size);
    }

    node->output_buffers[0] = out_buf;
    return 0;
}

static int filter_volume_flush(media_node_t *node) {
    (void)node;
    return 0;
}

static void filter_volume_destroy(media_node_t *node) {
    if (node->instance_id) free((void *)node->instance_id);
    free(node);
}

static const node_ops_t filter_volume_ops = {
    .init = filter_volume_init,
    .get_caps = filter_volume_get_caps,
    .set_caps = filter_volume_set_caps,
    .process = filter_volume_process,
    .flush = filter_volume_flush,
    .destroy = filter_volume_destroy,
    .set_volume = filter_volume_set_volume,
};

static const node_descriptor_t filter_volume_desc = {
    .name = "filter_volume",
    .ops = &filter_volume_ops,
    .num_input_ports = 1,
    .num_output_ports = 1,
};

media_node_t* filter_volume_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &filter_volume_desc;
    node->instance_id = strdup(instance_id ? instance_id : "vol");
    node->num_input_ports = 1;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(filter_volume_priv_t));
    if (!node->private_data) {
        free((void *)node->instance_id);
        free(node);
        return NULL;
    }
    filter_volume_priv_t *p = (filter_volume_priv_t *)node->private_data;
    p->gain = 1.0f;
    return node;
}

void filter_volume_destroy_fn(media_node_t *node) {
    filter_volume_destroy(node);
}
