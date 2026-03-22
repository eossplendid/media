/**
 * @file encoder_opus.c
 * @brief Opus encoder node (1 in, 1 out); PCM S16 -> Opus packets.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_core/config_schema.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_OPUS)
#include <opus/opus.h>
#endif

typedef struct {
#if defined(HAVE_OPUS)
    OpusEncoder *enc;
    int frame_size;
    uint32_t sample_rate;
    uint32_t channels;
#endif
} encoder_opus_priv_t;

static int encoder_opus_init(media_node_t *node, const node_config_t *config) {
    encoder_opus_priv_t *p = (encoder_opus_priv_t *)node->private_data;
    if (!p) return -1;
#if defined(HAVE_OPUS)
    p->sample_rate = config_get_uint32(config, "sample_rate", 16000);
    p->channels = config_get_uint32(config, "channels", 1);
    p->frame_size = (int)(p->sample_rate * 20 / 1000);
    if (p->frame_size <= 0) p->frame_size = 320;
    int err;
    p->enc = opus_encoder_create((opus_int32)p->sample_rate, (int)p->channels,
                                 OPUS_APPLICATION_VOIP, &err);
    if (!p->enc || err != OPUS_OK) return -1;
    opus_encoder_ctl(p->enc, OPUS_SET_BITRATE(64000));
#endif
    return 0;
}

static int encoder_opus_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    encoder_opus_priv_t *p = (encoder_opus_priv_t *)node->private_data;
    if (!p || !out_caps) return -1;
    if (port_index == 0) {
        memset(out_caps, 0, sizeof(*out_caps));
        out_caps->codec = MEDIA_CODEC_OPUS;
        out_caps->sample_rate = p->sample_rate;
        out_caps->channels = p->channels;
    }
    return 0;
}

static int encoder_opus_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    encoder_opus_priv_t *p = (encoder_opus_priv_t *)node->private_data;
    if (!p || !caps) return -1;
    if (port_index == 0) {
        if (caps->sample_rate) p->sample_rate = caps->sample_rate;
        if (caps->channels) p->channels = caps->channels;
        p->frame_size = (int)(p->sample_rate * 20 / 1000);
    }
    return 0;
}

static int encoder_opus_process(media_node_t *node) {
#if defined(HAVE_OPUS)
    encoder_opus_priv_t *p = (encoder_opus_priv_t *)node->private_data;
    media_buffer_t *in_buf = node->input_buffers[0];
    if (!p || !p->enc || !in_buf || !in_buf->data || in_buf->size == 0) return 0;

    int n_samples = (int)(in_buf->size / (2 * p->channels));
    if (n_samples <= 0) return 0;

    int max_out = 1275;
    media_buffer_t *out_buf = media_buffer_alloc((size_t)max_out);
    if (!out_buf) return -1;
    int len = opus_encode(p->enc, (const opus_int16 *)in_buf->data, n_samples,
                          out_buf->data, max_out);
    if (len < 0) {
        media_buffer_free(out_buf);
        return 0;
    }
    out_buf->size = (size_t)len;
    out_buf->pts = in_buf->pts;
    out_buf->caps.codec = MEDIA_CODEC_OPUS;
    out_buf->caps.sample_rate = p->sample_rate;
    out_buf->caps.channels = p->channels;
    node->output_buffers[0] = out_buf;
    return 0;
#else
    (void)node;
    return 0;
#endif
}

static int encoder_opus_flush(media_node_t *node) {
    if (node->output_buffers[0]) {
        media_buffer_free(node->output_buffers[0]);
        node->output_buffers[0] = NULL;
    }
    return 0;
}

static void encoder_opus_destroy(media_node_t *node) {
    encoder_opus_priv_t *p = (encoder_opus_priv_t *)node->private_data;
    if (p) {
#if defined(HAVE_OPUS)
        if (p->enc) opus_encoder_destroy(p->enc);
#endif
        free(p);
    }
    if (node->instance_id) free((void *)node->instance_id);
    free(node);
}

static const node_ops_t encoder_opus_ops = {
    .init = encoder_opus_init,
    .get_caps = encoder_opus_get_caps,
    .set_caps = encoder_opus_set_caps,
    .process = encoder_opus_process,
    .flush = encoder_opus_flush,
    .destroy = encoder_opus_destroy,
};

static const node_descriptor_t encoder_opus_desc = {
    .name = "encoder_opus",
    .ops = &encoder_opus_ops,
    .num_input_ports = 1,
    .num_output_ports = 1,
};

media_node_t *encoder_opus_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &encoder_opus_desc;
    node->instance_id = strdup(instance_id ? instance_id : "enc_opus");
    node->num_input_ports = 1;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(encoder_opus_priv_t));
    if (!node->private_data) {
        free((void *)node->instance_id);
        free(node);
        return NULL;
    }
    return node;
}

void encoder_opus_destroy_fn(media_node_t *node) {
    encoder_opus_destroy(node);
}
