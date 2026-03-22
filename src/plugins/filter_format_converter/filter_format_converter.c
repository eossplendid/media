/**
 * @file filter_format_converter.c
 * @brief Format converter (1 in, 1 out): S16/S32/F32/U8 conversion.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    media_format_t format_in;
    media_format_t format_out;
    uint32_t sample_rate;
    uint32_t channels;
} format_converter_priv_t;

static int converter_init(media_node_t *node, const node_config_t *config) {
    (void)node;
    (void)config;
    return 0;
}

static int converter_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    format_converter_priv_t *p = (format_converter_priv_t *)node->private_data;
    if (!p || !out_caps) return -1;
    if (port_index == 0) {
        out_caps->sample_rate = p->sample_rate;
        out_caps->channels = p->channels;
        out_caps->format = p->format_out;
        out_caps->bytes_per_sample = media_format_bytes(p->format_out) * p->channels;
    }
    return 0;
}

static int converter_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    format_converter_priv_t *p = (format_converter_priv_t *)node->private_data;
    if (!p || !caps || port_index != 0) return -1;
    p->format_in = caps->format;
    p->sample_rate = caps->sample_rate ? caps->sample_rate : 48000;
    p->channels = caps->channels ? caps->channels : 1;
    return 0;
}

static void s16_to_s32(const int16_t *in, int32_t *out, size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = (int32_t)in[i] << 16;
}

static void s32_to_s16(const int32_t *in, int16_t *out, size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = (int16_t)(in[i] >> 16);
}

static void s16_to_f32(const int16_t *in, float *out, size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = (float)in[i] / 32768.f;
}

static void f32_to_s16(const float *in, int16_t *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        float v = in[i] * 32768.f;
        if (v > 32767.f) v = 32767.f;
        if (v < -32768.f) v = -32768.f;
        out[i] = (int16_t)v;
    }
}

static void s32_to_f32(const int32_t *in, float *out, size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = (float)(in[i] >> 16) / 32768.f;
}

static void f32_to_s32(const float *in, int32_t *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        float v = in[i] * 32768.f;
        int32_t vi = (int32_t)v;
        if (vi > 2147483647) vi = 2147483647;
        if (vi < -2147483648) vi = -2147483648;
        out[i] = vi << 16;
    }
}

static void u8_to_s16(const uint8_t *in, int16_t *out, size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = (int16_t)(((int32_t)in[i] - 128) * 256);
}

static void s16_to_u8(const int16_t *in, uint8_t *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int v = (in[i] >> 8) + 128;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        out[i] = (uint8_t)v;
    }
}

static int converter_process(media_node_t *node) {
    format_converter_priv_t *p = (format_converter_priv_t *)node->private_data;
    media_buffer_t *in_buf = node->input_buffers[0];
    if (!p) return 0;
    if (!in_buf || !in_buf->data || in_buf->size == 0) return 0;

    uint32_t in_bps = media_format_bytes(p->format_in);
    uint32_t out_bps = media_format_bytes(p->format_out);
    if (in_bps == 0 || out_bps == 0) return 0;

    uint32_t ch = p->channels ? p->channels : 1;
    size_t frames = in_buf->size / (ch * in_bps);
    size_t out_size = frames * ch * out_bps;

    media_buffer_t *out_buf = media_buffer_alloc(out_size);
    if (!out_buf) return 0;
    out_buf->pts = in_buf->pts;
    out_buf->caps = in_buf->caps;
    out_buf->caps.format = p->format_out;
    out_buf->caps.bytes_per_sample = out_bps * ch;
    out_buf->size = out_size;

    size_t n = frames * ch;
    if (p->format_in == MEDIA_FORMAT_S16 && p->format_out == MEDIA_FORMAT_S32) {
        s16_to_s32((const int16_t *)in_buf->data, (int32_t *)out_buf->data, n);
    } else if (p->format_in == MEDIA_FORMAT_S32 && p->format_out == MEDIA_FORMAT_S16) {
        s32_to_s16((const int32_t *)in_buf->data, (int16_t *)out_buf->data, n);
    } else if (p->format_in == MEDIA_FORMAT_S16 && p->format_out == MEDIA_FORMAT_F32) {
        s16_to_f32((const int16_t *)in_buf->data, (float *)out_buf->data, n);
    } else if (p->format_in == MEDIA_FORMAT_F32 && p->format_out == MEDIA_FORMAT_S16) {
        f32_to_s16((const float *)in_buf->data, (int16_t *)out_buf->data, n);
    } else if (p->format_in == MEDIA_FORMAT_S32 && p->format_out == MEDIA_FORMAT_F32) {
        s32_to_f32((const int32_t *)in_buf->data, (float *)out_buf->data, n);
    } else if (p->format_in == MEDIA_FORMAT_F32 && p->format_out == MEDIA_FORMAT_S32) {
        f32_to_s32((const float *)in_buf->data, (int32_t *)out_buf->data, n);
    } else if (p->format_in == MEDIA_FORMAT_U8 && p->format_out == MEDIA_FORMAT_S16) {
        u8_to_s16((const uint8_t *)in_buf->data, (int16_t *)out_buf->data, n);
    } else if (p->format_in == MEDIA_FORMAT_S16 && p->format_out == MEDIA_FORMAT_U8) {
        s16_to_u8((const int16_t *)in_buf->data, (uint8_t *)out_buf->data, n);
    } else {
        media_buffer_free(out_buf);
        return 0;
    }

    node->output_buffers[0] = out_buf;
    return 0;
}

static int converter_flush(media_node_t *node) {
    (void)node;
    return 0;
}

static void converter_destroy(media_node_t *node) {
    if (node->instance_id) free((void *)node->instance_id);
    free(node);
}

static const node_ops_t converter_ops = {
    .init = converter_init,
    .get_caps = converter_get_caps,
    .set_caps = converter_set_caps,
    .process = converter_process,
    .flush = converter_flush,
    .destroy = converter_destroy,
    .set_volume = NULL,
};

static const node_descriptor_t converter_desc = {
    .name = "filter_format_converter",
    .ops = &converter_ops,
    .num_input_ports = 1,
    .num_output_ports = 1,
};

media_node_t* filter_format_converter_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &converter_desc;
    node->instance_id = strdup(instance_id ? instance_id : "format_conv");
    node->num_input_ports = 1;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(format_converter_priv_t));
    if (!node->private_data) {
        free((void *)node->instance_id);
        free(node);
        return NULL;
    }
    format_converter_priv_t *p = (format_converter_priv_t *)node->private_data;
    p->format_in = MEDIA_FORMAT_S16;
    p->format_out = config && config->format ? (media_format_t)config->format : MEDIA_FORMAT_S16;
    return node;
}

void filter_format_converter_destroy_fn(media_node_t *node) {
    converter_destroy(node);
}
