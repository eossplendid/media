/**
 * @file source_file.c
 * @brief WAV file source (0 in, 1 out); reads WAV header + PCM.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    FILE *fp;
    char *path;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bytes_per_sample;
    size_t read_size;
} source_file_priv_t;

static int parse_wav_header(FILE *fp, uint32_t *rate, uint32_t *channels, uint32_t *bps) {
    uint8_t h[44];
    if (fread(h, 1, 44, fp) != 44) return -1;
    if (memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0) return -1;
    if (memcmp(h + 12, "fmt ", 4) != 0) return -1;
    uint16_t fmt = (uint16_t)(h[20] | (h[21] << 8));
    if (fmt != 1) return -1; /* PCM */
    *channels = h[22] | (h[23] << 8);
    *rate = (uint32_t)(h[24] | (h[25]<<8) | (h[26]<<16) | (h[27]<<24));
    uint16_t bps_sample = h[34] | (h[35]<<8);
    *bps = *channels * (bps_sample / 8);
    if (memcmp(h + 36, "data", 4) != 0) return -1;
    return 0;
}

static int source_file_init(media_node_t *node, const node_config_t *config) {
    source_file_priv_t *p = (source_file_priv_t *)node->private_data;
    if (!p || !config || !config->path) return -1;
    if (!p->path) {
        p->path = strdup(config->path);
        if (!p->path) return -1;
    }
    p->fp = fopen(p->path, "rb");
    if (!p->fp) return -1;
    if (parse_wav_header(p->fp, &p->sample_rate, &p->channels, &p->bytes_per_sample) != 0) {
        fclose(p->fp);
        p->fp = NULL;
        return -1;
    }
    p->read_size = 4096;
    return 0;
}

static int source_file_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    (void)port_index;
    source_file_priv_t *p = (source_file_priv_t *)node->private_data;
    if (!p || !out_caps) return -1;
    out_caps->sample_rate = p->sample_rate;
    out_caps->channels = p->channels;
    out_caps->format = MEDIA_FORMAT_S16;
    out_caps->bytes_per_sample = p->bytes_per_sample;
    return 0;
}

static int source_file_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    (void)node;(void)port_index;(void)caps;
    return 0;
}

static int source_file_process(media_node_t *node) {
    source_file_priv_t *p = (source_file_priv_t *)node->private_data;
    if (!p || !p->fp) return 0;
    media_buffer_t *buf = media_buffer_alloc(p->read_size);
    if (!buf) return -1;
    buf->caps.sample_rate = p->sample_rate;
    buf->caps.channels = p->channels;
    buf->caps.format = MEDIA_FORMAT_S16;
    buf->caps.bytes_per_sample = p->bytes_per_sample;
    size_t n = fread(buf->data, 1, p->read_size, p->fp);
    buf->size = n;
    /* Pipeline frees output_buffers at end of each iteration */
    node->output_buffers[0] = buf;
    if (n == 0) return 0; /* EOF, stop pipeline could be signaled by app */
    return 0;
}

static int source_file_flush(media_node_t *node) {
    if (node->output_buffers[0]) {
        media_buffer_free(node->output_buffers[0]);
        node->output_buffers[0] = NULL;
    }
    return 0;
}

static void source_file_destroy(media_node_t *node) {
    source_file_priv_t *p = (source_file_priv_t *)node->private_data;
    if (p) {
        if (p->fp) fclose(p->fp);
        if (p->path) free(p->path);
        free(p);
    }
    if (node->instance_id) free((void*)node->instance_id);
    free(node);
}

static const node_ops_t source_file_ops = {
    .init = source_file_init,
    .get_caps = source_file_get_caps,
    .set_caps = source_file_set_caps,
    .process = source_file_process,
    .flush = source_file_flush,
    .destroy = source_file_destroy,
};

static const node_descriptor_t source_file_desc = {
    .name = "source_file",
    .ops = &source_file_ops,
    .num_input_ports = 0,
    .num_output_ports = 1,
};

/* prepare 前 probe：若未 init 则临时打开文件读取 header，供 pipeline_prepare 协商 */
static void probe_if_needed(source_file_priv_t *p, const char *path) {
    if (!p || p->sample_rate != 0 || !path) return;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    if (parse_wav_header(f, &p->sample_rate, &p->channels, &p->bytes_per_sample) == 0) {
        /* probe 成功，后续 init 会重新打开 */
    } else {
        p->sample_rate = 0;
    }
    fclose(f);
}

media_node_t* source_file_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &source_file_desc;
    node->instance_id = strdup(instance_id ? instance_id : "file");
    node->num_input_ports = 0;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(source_file_priv_t));
    if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
    source_file_priv_t *p = (source_file_priv_t *)node->private_data;
    if (config && config->path) {
        p->path = strdup(config->path);
        if (p->path) probe_if_needed(p, p->path);
    }
    return node;
}

void source_file_destroy_fn(media_node_t *node) {
    source_file_destroy(node);
}
