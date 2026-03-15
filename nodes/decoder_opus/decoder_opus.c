/**
 * @file decoder_opus.c
 * @brief Opus decoder node (1 in, 1 out); Opus packets -> PCM.
 *        Stub: requires libopus for full implementation.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t sample_rate;
    uint32_t channels;
} decoder_opus_priv_t;

static int decoder_opus_init(media_node_t *node, const node_config_t *config) {
    (void)node;(void)config;
    return 0;
}

static int decoder_opus_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    decoder_opus_priv_t *p = (decoder_opus_priv_t *)node->private_data;
    if (!p || !out_caps) return -1;
    if (port_index == 0) {
        out_caps->sample_rate = p->sample_rate ? p->sample_rate : 48000;
        out_caps->channels = p->channels ? p->channels : 1;
        out_caps->format = MEDIA_FORMAT_S16;
        out_caps->bytes_per_sample = 2 * p->channels;
        out_caps->codec = MEDIA_CODEC_NONE;
    }
    return 0;
}

static int decoder_opus_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    (void)node;(void)port_index;(void)caps;
    return 0;
}

static int decoder_opus_process(media_node_t *node) {
    (void)node;
    /* Stub: requires libopus; no output for now */
    return 0;
}

static int decoder_opus_flush(media_node_t *node) {
    if (node->output_buffers[0]) {
        media_buffer_free(node->output_buffers[0]);
        node->output_buffers[0] = NULL;
    }
    return 0;
}

static void decoder_opus_destroy(media_node_t *node) {
    if (node->private_data) free(node->private_data);
    if (node->instance_id) free((void*)node->instance_id);
    free(node);
}

static const node_ops_t decoder_opus_ops = {
    .init = decoder_opus_init,
    .get_caps = decoder_opus_get_caps,
    .set_caps = decoder_opus_set_caps,
    .process = decoder_opus_process,
    .flush = decoder_opus_flush,
    .destroy = decoder_opus_destroy,
};

static const node_descriptor_t decoder_opus_desc = {
    .name = "decoder_opus",
    .ops = &decoder_opus_ops,
    .num_input_ports = 1,
    .num_output_ports = 1,
};

media_node_t* decoder_opus_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &decoder_opus_desc;
    node->instance_id = strdup(instance_id ? instance_id : "dec_opus");
    node->num_input_ports = 1;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(decoder_opus_priv_t));
    if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
    (void)config;
    return node;
}

void decoder_opus_destroy_fn(media_node_t *node) {
    decoder_opus_destroy(node);
}
