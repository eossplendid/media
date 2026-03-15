/**
 * @file source_mic.c
 * @brief Microphone source node (0 in, 1 out); ALSA capture, 参考 cursor_repo/test_recorder.
 *
 * 配置：16kHz, 16bit, 单通道，20ms 帧（可配置）
 * 依赖：libasound2-dev (sudo apt install libasound2-dev)
 * WSL：需安装 libasound2-plugins pulseaudio，PULSE_SERVER 连接 Windows
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#ifdef __linux__
#include <alsa/asoundlib.h>
#endif

#define SOURCE_MIC_DEFAULT_FRAME_MS 20
#define SOURCE_MIC_DEFAULT_SAMPLE_RATE 16000
#define SOURCE_MIC_DEFAULT_CHANNELS 1

typedef struct {
#ifdef __linux__
    snd_pcm_t *pcm;
#endif
    char device[64];
    unsigned int sample_rate;
    unsigned int channels;
    unsigned int frame_ms;
    unsigned int frame_size;   /* bytes per frame (channels * 2 for S16) */
    size_t period_bytes;       /* bytes per period = frame_samples * frame_size */
    unsigned int frame_samples; /* samples per period (mono) or frames (stereo) */
    int64_t next_pts;           /* 下一帧 PTS (us)，用于 media_buffer_t.pts 维护 */
} source_mic_priv_t;

static int source_mic_init(media_node_t *node, const node_config_t *config) {
    source_mic_priv_t *p = (source_mic_priv_t *)node->private_data;
    if (!p) return -1;
    p->sample_rate = config && config->sample_rate ? config->sample_rate : SOURCE_MIC_DEFAULT_SAMPLE_RATE;
    p->channels = config && config->channels ? config->channels : SOURCE_MIC_DEFAULT_CHANNELS;
    p->frame_ms = SOURCE_MIC_DEFAULT_FRAME_MS;
    p->frame_size = p->channels * 2; /* S16 */
    /* ALSA frame = 1 sample per channel; frame_samples = frames to read per period */
    p->frame_samples = p->sample_rate * p->frame_ms / 1000;
    p->period_bytes = (size_t)p->frame_samples * p->frame_size;
    p->next_pts = 0;

    if (config && config->device && config->device[0])
        strncpy(p->device, config->device, sizeof(p->device) - 1);
    else
        strncpy(p->device, "default", sizeof(p->device) - 1);
    p->device[sizeof(p->device) - 1] = '\0';

#ifdef __linux__
    int err = snd_pcm_open(&p->pcm, p->device, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "[source_mic] snd_pcm_open(%s) failed: %s\n", p->device, snd_strerror(err));
        return -1;
    }
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(p->pcm, params);
    snd_pcm_hw_params_set_access(p->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(p->pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(p->pcm, params, p->channels);
    unsigned int rate = p->sample_rate;
    snd_pcm_hw_params_set_rate_near(p->pcm, params, &rate, NULL);
    snd_pcm_uframes_t period_size = (snd_pcm_uframes_t)p->frame_samples;
    snd_pcm_hw_params_set_period_size_near(p->pcm, params, &period_size, NULL);
    snd_pcm_hw_params_set_periods(p->pcm, params, 4, 0);
    err = snd_pcm_hw_params(p->pcm, params);
    if (err < 0) {
        fprintf(stderr, "[source_mic] snd_pcm_hw_params failed: %s\n", snd_strerror(err));
        snd_pcm_close(p->pcm);
        p->pcm = NULL;
        return -1;
    }
    err = snd_pcm_prepare(p->pcm);
    if (err < 0) {
        fprintf(stderr, "[source_mic] snd_pcm_prepare failed: %s\n", snd_strerror(err));
        snd_pcm_close(p->pcm);
        p->pcm = NULL;
        return -1;
    }
    err = snd_pcm_start(p->pcm);
    if (err < 0) {
        fprintf(stderr, "[source_mic] snd_pcm_start failed: %s\n", snd_strerror(err));
        snd_pcm_close(p->pcm);
        p->pcm = NULL;
        return -1;
    }
#endif
    return 0;
}

static int source_mic_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps) {
    (void)port_index;
    source_mic_priv_t *p = (source_mic_priv_t *)node->private_data;
    if (!p || !out_caps) return -1;
    out_caps->sample_rate = p->sample_rate;
    out_caps->channels = p->channels;
    out_caps->format = MEDIA_FORMAT_S16;
    out_caps->bytes_per_sample = 2 * p->channels;
    out_caps->frame_ms = SOURCE_MIC_DEFAULT_FRAME_MS;
    return 0;
}

static int source_mic_set_caps(media_node_t *node, int port_index, const media_caps_t *caps) {
    (void)node;(void)port_index;(void)caps;
    return 0;
}

static int source_mic_process(media_node_t *node) {
    source_mic_priv_t *p = (source_mic_priv_t *)node->private_data;
    if (!p) return -1;
    media_buffer_t *buf = media_buffer_alloc(p->period_bytes);
    if (!buf) return -1;
    buf->caps.sample_rate = p->sample_rate;
    buf->caps.channels = p->channels;
    buf->caps.format = MEDIA_FORMAT_S16;
    buf->caps.bytes_per_sample = p->frame_size;

#ifdef __linux__
    if (p->pcm) {
        snd_pcm_sframes_t n = snd_pcm_readi(p->pcm, buf->data,
            (snd_pcm_uframes_t)p->frame_samples);
        if (n <= 0) {
            if (n == -EPIPE) snd_pcm_prepare(p->pcm);
            media_buffer_free(buf);
            return 0;
        }
        buf->size = (size_t)n * p->frame_size;
        buf->pts = p->next_pts;
        p->next_pts += (int64_t)p->frame_ms * 1000;  /* 帧长(ms) -> us */
    } else {
        media_buffer_free(buf);
        return -1;
    }
#else
    media_buffer_free(buf);
    return -1;
#endif
    node->output_buffers[0] = buf;
    return 0;
}

static int source_mic_flush(media_node_t *node) {
    if (node->output_buffers[0]) {
        media_buffer_free(node->output_buffers[0]);
        node->output_buffers[0] = NULL;
    }
    return 0;
}

static void source_mic_destroy(media_node_t *node) {
    source_mic_priv_t *p = (source_mic_priv_t *)node->private_data;
    if (p) {
#ifdef __linux__
        if (p->pcm) {
            snd_pcm_drop(p->pcm);
            snd_pcm_close(p->pcm);
            p->pcm = NULL;
        }
#endif
        free(p);
    }
    if (node->instance_id) free((void*)node->instance_id);
    free(node);
}

static const node_ops_t source_mic_ops = {
    .init = source_mic_init,
    .get_caps = source_mic_get_caps,
    .set_caps = source_mic_set_caps,
    .process = source_mic_process,
    .flush = source_mic_flush,
    .destroy = source_mic_destroy,
};

static const node_descriptor_t source_mic_desc = {
    .name = "source_mic",
    .ops = &source_mic_ops,
    .num_input_ports = 0,
    .num_output_ports = 1,
};

media_node_t* source_mic_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = &source_mic_desc;
    node->instance_id = strdup(instance_id ? instance_id : "mic");
    node->num_input_ports = 0;
    node->num_output_ports = 1;
    node->private_data = calloc(1, sizeof(source_mic_priv_t));
    if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
    (void)config;
    return node;
}

void source_mic_destroy_fn(media_node_t *node) {
    source_mic_destroy(node);
}
