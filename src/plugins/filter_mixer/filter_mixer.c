/**
 * @file filter_mixer.c
 * @brief Mixer N in 1 out; same format, mix by averaging.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>

#define MAX_MIXER_INPUTS 8

typedef struct {
    int input_count;
    uint32_t sample_rate;
    uint32_t channels;
} mixer_priv_t;

static int mixer_init(media_node_t *node, const node_config_t *config) {
    mixer_priv_t *p = (mixer_priv_t *)node->private_data;
    if (!p) return -1;
    p->input_count = config && config->input_count > 0 ? config->input_count : 2;
    if (p->input_count > MAX_MIXER_INPUTS) p->input_count = MAX_MIXER_INPUTS;
    if (config && config->sample_rate) p->sample_rate = config->sample_rate;  /* 目标采样率，prepare 用于协商 */
    return 0;
}

static int mixer_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    mixer_priv_t *p = (mixer_priv_t *)node->private_data;
    if (!p || !out_caps || port_index != 0) return -1;
    out_caps->sample_rate = p->sample_rate;
    out_caps->channels = p->channels;
    out_caps->format = MEDIA_FORMAT_S16;
    out_caps->bytes_per_sample = 2 * p->channels;
    return 0;
}

static int mixer_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    mixer_priv_t *p = (mixer_priv_t *)node->private_data;
    if (!p || !caps) return -1;
    if (port_index >= 0 && port_index < MAX_MIXER_INPUTS) {
        if (!p->sample_rate) p->sample_rate = caps->sample_rate;
        if (!p->channels) p->channels = caps->channels ? caps->channels : 1;
    }
    return 0;
}

static int mixer_process(media_node_t *node) {
    mixer_priv_t *p = (mixer_priv_t *)node->private_data;
    if (!p) return 0;
    for (int i = 0; i < p->input_count && i < node->num_input_ports; i++) {
        media_buffer_t *b = node->input_buffers[i];
        if (b && b->caps.sample_rate && !p->sample_rate) {
            p->sample_rate = b->caps.sample_rate;
            p->channels = b->caps.channels ? b->caps.channels : 1;
            break;
        }
    }
    if (p->channels == 0) return 0;
    size_t min_size = (size_t)-1;
    for (int i = 0; i < p->input_count && i < node->num_input_ports; i++) {
        media_buffer_t *b = node->input_buffers[i];
        if (b && b->size > 0 && b->size < min_size) min_size = b->size;
    }
    if (min_size == (size_t)-1 || min_size == 0) return 0;
    media_buffer_t *out = media_buffer_alloc(min_size);
    if (!out) return -1;
    out->caps.sample_rate = p->sample_rate;
    out->caps.channels = p->channels;
    out->caps.format = MEDIA_FORMAT_S16;
    out->caps.bytes_per_sample = 2 * p->channels;
    memset(out->data, 0, min_size);
    int active = 0;
    for (int i = 0; i < p->input_count && i < node->num_input_ports; i++) {
        media_buffer_t *b = node->input_buffers[i];
        if (!b || !b->data || b->size < min_size) continue;
        active++;
        int16_t *out16 = (int16_t *)out->data;
        const int16_t *in16 = (const int16_t *)b->data;
        size_t n = min_size / 2;
        for (size_t j = 0; j < n; j++)
            out16[j] += in16[j];
    }
    if (active > 1)
        for (size_t j = 0; j < min_size / 2; j++)
            ((int16_t *)out->data)[j] /= (int16_t)active;
    out->size = min_size;
    /* Pipeline frees output_buffers at end of each iteration */
    node->output_buffers[0] = out;
    return 0;
}

static int mixer_flush(media_node_t *node) {
    if (node->output_buffers[0]) {
        media_buffer_free(node->output_buffers[0]);
        node->output_buffers[0] = NULL;
    }
    return 0;
}

static void mixer_destroy(media_node_t *node) {
    if (node->private_data) free(node->private_data);
    if (node->instance_id) free((void*)node->instance_id);
    free(node);
}

static const node_ops_t mixer_ops = {
    .init = mixer_init,
    .get_caps = mixer_get_caps,
    .set_caps = mixer_set_caps,
    .process = mixer_process,
    .flush = mixer_flush,
    .destroy = mixer_destroy,
};

static const node_descriptor_t mixer_desc = {
    .name = "filter_mixer",
    .ops = &mixer_ops,
    .num_input_ports = MAX_MIXER_INPUTS,
    .num_output_ports = 1,
};

media_node_t* mixer_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &mixer_desc;
    node->instance_id = strdup(instance_id ? instance_id : "mix");
    node->num_input_ports = MAX_MIXER_INPUTS;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(mixer_priv_t));
    if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
    mixer_priv_t *p = (mixer_priv_t *)node->private_data;
    p->input_count = config && config->input_count > 0 ? config->input_count : 2;
    return node;
}

void mixer_destroy_fn(media_node_t *node) {
    mixer_destroy(node);
}
