/**
 * @file sink_speaker.c
 * @brief Speaker sink (1 in, 0 out); uses tinyalsa for playback.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <tinyalsa/pcm.h>
#else
#define PCM_OUT 0
struct pcm_config { unsigned int channels; unsigned int rate; unsigned int period_size; unsigned int period_count; int format; };
struct pcm { int dummy; };
static struct pcm *pcm_open(unsigned int c, unsigned int d, unsigned int f, const struct pcm_config *cfg) { (void)c;(void)d;(void)f;(void)cfg; return NULL; }
static struct pcm *pcm_open_by_name(const char *n, unsigned int f, const struct pcm_config *cfg) { (void)n;(void)f;(void)cfg; return NULL; }
static int pcm_prepare(struct pcm *p) { (void)p; return 0; }
static int pcm_start(struct pcm *p) { (void)p; return 0; }
static int pcm_writei(struct pcm *p, const void *d, unsigned int n) { (void)p;(void)d;(void)n; return 0; }
static int pcm_close(struct pcm *p) { (void)p; return 0; }
static int pcm_is_ready(struct pcm *p) { (void)p; return 0; }
enum pcm_format { PCM_FORMAT_S16_LE = 0 };
#endif

typedef struct {
#ifdef __linux__
    struct pcm *pcm;
#endif
    uint32_t sample_rate;
    uint32_t channels;
} sink_speaker_priv_t;

static int sink_speaker_init(media_node_t *node, const node_config_t *config) {
    sink_speaker_priv_t *p = (sink_speaker_priv_t *)node->private_data;
    if (!p) return -1;
    p->sample_rate = config && config->sample_rate ? config->sample_rate : 48000;
    p->channels = config && config->channels ? config->channels : 1;
#ifdef __linux__
    struct pcm_config cfg = {0};
    cfg.channels = p->channels;
    cfg.rate = p->sample_rate;
    cfg.period_size = 1024;
    cfg.period_count = 4;
    cfg.format = PCM_FORMAT_S16_LE;
    /* Prefer "default" so playback goes to system default output (e.g. PulseAudio) */
    p->pcm = pcm_open_by_name("default", PCM_OUT, &cfg);
    if (!p->pcm || !pcm_is_ready(p->pcm))
        p->pcm = pcm_open(0, 0, PCM_OUT, &cfg);
    if (!p->pcm || !pcm_is_ready(p->pcm)) return -1;
    if (pcm_prepare(p->pcm) != 0) return -1;
    if (pcm_start(p->pcm) != 0) return -1;
#endif
    return 0;
}

static int sink_speaker_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    (void)node;(void)port_index;(void)out_caps;
    return 0;
}

static int sink_speaker_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    (void)node;(void)port_index;(void)caps;
    return 0;
}

static int sink_speaker_process(media_node_t *node) {
    sink_speaker_priv_t *p = (sink_speaker_priv_t *)node->private_data;
    media_buffer_t *buf = node->input_buffers[0];
    if (!p) return 0;
    if (!buf || !buf->data || buf->size == 0) return 0;
#ifdef __linux__
    if (p->pcm) {
        unsigned int frames = (unsigned int)(buf->size / (p->channels * 2));
        if (frames > 0) {
            int w = pcm_writei(p->pcm, buf->data, frames);
            /* Restart on underrun (e.g. -EPIPE) so playback continues */
            if (w < 0 && pcm_prepare(p->pcm) == 0)
                pcm_start(p->pcm);
        }
    }
#endif
    return 0;
}

static int sink_speaker_flush(media_node_t *node) {
    (void)node;
    return 0;
}

static void sink_speaker_destroy(media_node_t *node) {
    sink_speaker_priv_t *p = (sink_speaker_priv_t *)node->private_data;
    if (p) {
#ifdef __linux__
        if (p->pcm) pcm_close(p->pcm);
#endif
        free(p);
    }
    if (node->instance_id) free((void*)node->instance_id);
    free(node);
}

static const node_ops_t sink_speaker_ops = {
    .init = sink_speaker_init,
    .get_caps = sink_speaker_get_caps,
    .set_caps = sink_speaker_set_caps,
    .process = sink_speaker_process,
    .flush = sink_speaker_flush,
    .destroy = sink_speaker_destroy,
};

static const node_descriptor_t sink_speaker_desc = {
    .name = "sink_speaker",
    .ops = &sink_speaker_ops,
    .num_input_ports = 1,
    .num_output_ports = 0,
};

media_node_t* sink_speaker_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &sink_speaker_desc;
    node->instance_id = strdup(instance_id ? instance_id : "spk");
    node->num_input_ports = 1;
    node->num_output_ports = 0;
    node->private_data = calloc(1, sizeof(sink_speaker_priv_t));
    if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
    (void)config;
    return node;
}

void sink_speaker_destroy_fn(media_node_t *node) {
    sink_speaker_destroy(node);
}
