/**
 * @file demuxer_ogg.c
 * @brief OGG container demuxer (1 in, 1 out); parses OGG pages, outputs Opus packets.
 *        Stub: requires libopusfile for full implementation.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *pending;
    size_t pending_size;
    size_t pending_cap;
} demuxer_ogg_priv_t;

static int demuxer_ogg_init(media_node_t *node, const node_config_t *config) {
    (void)node;(void)config;
    return 0;
}

static int demuxer_ogg_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    (void)node;
    if (!out_caps) return -1;
    if (port_index == 0) {
        memset(out_caps, 0, sizeof(*out_caps));
        out_caps->codec = MEDIA_CODEC_OPUS;
    }
    return 0;
}

static int demuxer_ogg_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    (void)node;(void)port_index;(void)caps;
    return 0;
}

static int demuxer_ogg_process(media_node_t *node) {
    /* Stub: pass-through raw bytes; full impl needs libopusfile */
    media_buffer_t *in_buf = node->input_buffers[0];
    if (!in_buf || !in_buf->data || in_buf->size == 0) return 0;
    media_buffer_t *out_buf = media_buffer_alloc(in_buf->size);
    if (!out_buf) return -1;
    memcpy(out_buf->data, in_buf->data, in_buf->size);
    out_buf->size = in_buf->size;
    out_buf->caps.codec = MEDIA_CODEC_OPUS;
    node->output_buffers[0] = out_buf;
    return 0;
}

static int demuxer_ogg_flush(media_node_t *node) {
    if (node->output_buffers[0]) {
        media_buffer_free(node->output_buffers[0]);
        node->output_buffers[0] = NULL;
    }
    return 0;
}

static void demuxer_ogg_destroy(media_node_t *node) {
    demuxer_ogg_priv_t *p = (demuxer_ogg_priv_t *)node->private_data;
    if (p) {
        free(p->pending);
        free(p);
    }
    if (node->instance_id) free((void*)node->instance_id);
    free(node);
}

static const node_ops_t demuxer_ogg_ops = {
    .init = demuxer_ogg_init,
    .get_caps = demuxer_ogg_get_caps,
    .set_caps = demuxer_ogg_set_caps,
    .process = demuxer_ogg_process,
    .flush = demuxer_ogg_flush,
    .destroy = demuxer_ogg_destroy,
};

static const node_descriptor_t demuxer_ogg_desc = {
    .name = "demuxer_ogg",
    .ops = &demuxer_ogg_ops,
    .num_input_ports = 1,
    .num_output_ports = 1,
};

media_node_t* demuxer_ogg_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &demuxer_ogg_desc;
    node->instance_id = strdup(instance_id ? instance_id : "demux_ogg");
    node->num_input_ports = 1;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(demuxer_ogg_priv_t));
    if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
    (void)config;
    return node;
}

void demuxer_ogg_destroy_fn(media_node_t *node) {
    demuxer_ogg_destroy(node);
}
