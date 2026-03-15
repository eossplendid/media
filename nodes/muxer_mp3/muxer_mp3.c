/**
 * @file muxer_mp3.c
 * @brief MP3 muxer node (1 in, 1 out); concatenates MP3 frames.
 *        MP3 has no container; muxer passes through or adds ID3 header.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int add_id3;
} muxer_mp3_priv_t;

static int muxer_mp3_init(media_node_t *node, const node_config_t *config) {
    (void)node;(void)config;
    return 0;
}

static int muxer_mp3_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    (void)node;
    if (!out_caps) return -1;
    if (port_index == 0) {
        memset(out_caps, 0, sizeof(*out_caps));
        out_caps->codec = MEDIA_CODEC_MP3;
    }
    return 0;
}

static int muxer_mp3_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    (void)node;(void)port_index;(void)caps;
    return 0;
}

static int muxer_mp3_process(media_node_t *node) {
    media_buffer_t *in_buf = node->input_buffers[0];
    if (!in_buf || !in_buf->data || in_buf->size == 0) return 0;
    /* Pass-through: MP3 frames as-is */
    media_buffer_t *out_buf = media_buffer_alloc(in_buf->size);
    if (!out_buf) return -1;
    memcpy(out_buf->data, in_buf->data, in_buf->size);
    out_buf->size = in_buf->size;
    out_buf->caps.codec = MEDIA_CODEC_MP3;
    node->output_buffers[0] = out_buf;
    return 0;
}

static int muxer_mp3_flush(media_node_t *node) {
    if (node->output_buffers[0]) {
        media_buffer_free(node->output_buffers[0]);
        node->output_buffers[0] = NULL;
    }
    return 0;
}

static void muxer_mp3_destroy(media_node_t *node) {
    if (node->private_data) free(node->private_data);
    if (node->instance_id) free((void*)node->instance_id);
    free(node);
}

static const node_ops_t muxer_mp3_ops = {
    .init = muxer_mp3_init,
    .get_caps = muxer_mp3_get_caps,
    .set_caps = muxer_mp3_set_caps,
    .process = muxer_mp3_process,
    .flush = muxer_mp3_flush,
    .destroy = muxer_mp3_destroy,
};

static const node_descriptor_t muxer_mp3_desc = {
    .name = "muxer_mp3",
    .ops = &muxer_mp3_ops,
    .num_input_ports = 1,
    .num_output_ports = 1,
};

media_node_t* muxer_mp3_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &muxer_mp3_desc;
    node->instance_id = strdup(instance_id ? instance_id : "mux_mp3");
    node->num_input_ports = 1;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(muxer_mp3_priv_t));
    if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
    (void)config;
    return node;
}

void muxer_mp3_destroy_fn(media_node_t *node) {
    muxer_mp3_destroy(node);
}
