/**
 * @file encoder_mp3.c
 * @brief MP3 encoder node (1 in, 1 out); PCM -> MP3.
 *        Stub implementation; full encode requires LAME/Shine integration.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t sample_rate;
    uint32_t channels;
    int bitrate_kbps;
} encoder_mp3_priv_t;

static int encoder_mp3_init(media_node_t *node, const node_config_t *config) {
    encoder_mp3_priv_t *p = (encoder_mp3_priv_t *)node->private_data;
    if (!p) return -1;
    p->sample_rate = config && config->sample_rate ? config->sample_rate : 44100;
    p->channels = config && config->channels ? config->channels : 1;
    p->bitrate_kbps = 128;
    return 0;
}

static int encoder_mp3_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    encoder_mp3_priv_t *p = (encoder_mp3_priv_t *)node->private_data;
    if (!p || !out_caps) return -1;
    if (port_index == 0) {
        memset(out_caps, 0, sizeof(*out_caps));
        out_caps->codec = MEDIA_CODEC_MP3;
    }
    return 0;
}

static int encoder_mp3_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    encoder_mp3_priv_t *p = (encoder_mp3_priv_t *)node->private_data;
    if (!p || !caps) return -1;
    if (port_index == 0) {
        p->sample_rate = caps->sample_rate ? caps->sample_rate : 44100;
        p->channels = caps->channels ? caps->channels : 1;
    }
    return 0;
}

static int encoder_mp3_process(media_node_t *node) {
    (void)node;
    /* Stub: pass-through would require LAME/Shine; for now no output */
    return 0;
}

static int encoder_mp3_flush(media_node_t *node) {
    if (node->output_buffers[0]) {
        media_buffer_free(node->output_buffers[0]);
        node->output_buffers[0] = NULL;
    }
    return 0;
}

static void encoder_mp3_destroy(media_node_t *node) {
    if (node->private_data) free(node->private_data);
    if (node->instance_id) free((void*)node->instance_id);
    free(node);
}

static const node_ops_t encoder_mp3_ops = {
    .init = encoder_mp3_init,
    .get_caps = encoder_mp3_get_caps,
    .set_caps = encoder_mp3_set_caps,
    .process = encoder_mp3_process,
    .flush = encoder_mp3_flush,
    .destroy = encoder_mp3_destroy,
};

static const node_descriptor_t encoder_mp3_desc = {
    .name = "encoder_mp3",
    .ops = &encoder_mp3_ops,
    .num_input_ports = 1,
    .num_output_ports = 1,
};

media_node_t* encoder_mp3_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &encoder_mp3_desc;
    node->instance_id = strdup(instance_id ? instance_id : "enc_mp3");
    node->num_input_ports = 1;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(encoder_mp3_priv_t));
    if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
    return node;
}

void encoder_mp3_destroy_fn(media_node_t *node) {
    encoder_mp3_destroy(node);
}
