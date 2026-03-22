/**
 * @file sink_speaker.c
 * @brief Speaker sink (1 in, 0 out). Uses hal_audio_out when USE_ALSA; else paplay/pulse/tinyalsa.
 */
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_core/hal_audio.h"
#include "media_core/config_schema.h"
#include "media_core/media_debug.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef __linux__
#include <unistd.h>
#endif

#ifdef __linux__
#ifdef USE_PAPLAY_PIPE
/* popen */
#elif defined(USE_PULSE)
#include <pulse/simple.h>
#include <pulse/error.h>
#elif defined(USE_ALSA)
#include <alsa/asoundlib.h>
#else
#include <tinyalsa/pcm.h>
#endif
#endif

typedef struct {
    void *hal_handle;
#ifdef __linux__
#ifdef USE_PAPLAY_PIPE
    FILE *paplay_fp;
#elif defined(USE_PULSE)
    pa_simple *pa;
#elif defined(USE_ALSA)
    snd_pcm_t *pcm;
#else
    struct pcm *pcm;
#endif
#endif
    uint32_t sample_rate;
    uint32_t channels;
} sink_speaker_priv_t;

static int sink_speaker_init(media_node_t *node, const node_config_t *config) {
    sink_speaker_priv_t *p = (sink_speaker_priv_t *)node->private_data;
    if (!p) return -1;
    p->sample_rate = config_get_uint32(config, "sample_rate", 48000);
    if (!p->sample_rate) p->sample_rate = 48000;
    p->channels = config_get_uint32(config, "channels", 1);
    if (!p->channels) p->channels = 1;

#ifdef USE_ALSA
    const hal_audio_out_ops_t *ops = hal_get_audio_out_ops();
    if (ops && ops->open) {
        hal_audio_config_t hcfg = {
            .device = "default",
            .sample_rate = p->sample_rate,
            .channels = p->channels,
            .format = MEDIA_FORMAT_S16,
            .period_frames = 1024,
        };
        int r = ops->open(&p->hal_handle, &hcfg);
        if (r == 0) return 0;
    }
#endif

#ifdef __linux__
#ifdef USE_PAPLAY_PIPE
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "paplay --raw --format=s16le --channels=%u --rate=%u",
             (unsigned)p->channels, (unsigned)p->sample_rate);
    p->paplay_fp = popen(cmd, "w");
    if (!p->paplay_fp) {
        fprintf(stderr, "[sink_speaker] popen(paplay) failed\n");
        return -1;
    }
#elif defined(USE_PULSE)
    pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = p->sample_rate,
        .channels = (uint8_t)p->channels
    };
    int pa_err = 0;
    p->pa = pa_simple_new(NULL, "media", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &pa_err);
    if (!p->pa) {
        fprintf(stderr, "[sink_speaker] pa_simple_new failed: %s\n", pa_strerror(pa_err));
        return -1;
    }
#elif defined(USE_ALSA)
    int err = -1;
    snd_pcm_hw_params_t *hw_params = NULL;
    const char *devices[] = { "default", "pulse", "sysdefault:CARD=default", NULL };

    for (int i = 0; devices[i]; i++) {
        err = snd_pcm_open(&p->pcm, devices[i], SND_PCM_STREAM_PLAYBACK, 0);
        if (err >= 0) break;
    }
    if (err < 0) {
        fprintf(stderr, "[sink_speaker] snd_pcm_open failed: %s\n", snd_strerror(err));
        return -1;
    }

    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(p->pcm, hw_params);
    snd_pcm_hw_params_set_access(p->pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(p->pcm, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(p->pcm, hw_params, p->channels);
    snd_pcm_hw_params_set_rate_near(p->pcm, hw_params, &p->sample_rate, 0);
    snd_pcm_uframes_t period_size = 1024;
    int dir = 0;
    snd_pcm_hw_params_set_period_size_near(p->pcm, hw_params, &period_size, &dir);
    unsigned int periods = 4;
    snd_pcm_hw_params_set_periods_near(p->pcm, hw_params, &periods, &dir);

    err = snd_pcm_hw_params(p->pcm, hw_params);
    if (err < 0) {
        fprintf(stderr, "[sink_speaker] snd_pcm_hw_params failed: %s\n", snd_strerror(err));
        snd_pcm_close(p->pcm);
        p->pcm = NULL;
        return -1;
    }
    snd_pcm_prepare(p->pcm);
#else
    struct pcm_config cfg = {0};
    cfg.channels = p->channels;
    cfg.rate = p->sample_rate;
    cfg.period_size = 1024;
    cfg.period_count = 4;
    cfg.format = PCM_FORMAT_S16_LE;
    p->pcm = pcm_open_by_name("default", PCM_OUT, &cfg);
    if (!p->pcm || !pcm_is_ready(p->pcm))
        p->pcm = pcm_open(0, 0, PCM_OUT, &cfg);
    if (!p->pcm || !pcm_is_ready(p->pcm)) {
        fprintf(stderr, "[sink_speaker] TinyALSA pcm_open failed\n");
        return -1;
    }
    if (pcm_prepare(p->pcm) != 0 || pcm_start(p->pcm) != 0) return -1;
#endif
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

    unsigned int frames = (unsigned int)(buf->size / (p->channels * 2));
    if (frames == 0) return 0;

    if (p->hal_handle) {
        const hal_audio_out_ops_t *ops = hal_get_audio_out_ops();
        if (ops && ops->write) {
            int w = ops->write(p->hal_handle, buf->data, frames);
            if (media_debug_enabled() && w >= 0)
                fprintf(stderr, "[sink_speaker] wrote %u frames (buf=%zu)\n", frames, buf->size);
            unsigned long us = (unsigned long)frames * 1000000UL / (p->sample_rate ? p->sample_rate : 48000);
            if (us > 0 && us < 500000) usleep((useconds_t)us);
        }
        return 0;
    }

#ifdef __linux__
#ifdef USE_PAPLAY_PIPE
    if (p->paplay_fp) {
        size_t n = fwrite(buf->data, 1, buf->size, p->paplay_fp);
        if (n != buf->size)
            fprintf(stderr, "[sink_speaker] fwrite paplay: %zu/%zu\n", n, buf->size);
        else if (media_debug_enabled())
            fprintf(stderr, "[sink_speaker] wrote %u frames (buf=%zu)\n", frames, buf->size);
        fflush(p->paplay_fp);
        { unsigned long us = (unsigned long)frames * 1000000UL / (p->sample_rate ? p->sample_rate : 48000);
          if (us > 0 && us < 500000) usleep((useconds_t)us); }
    }
#elif defined(USE_PULSE)
    if (p->pa) {
        int pa_err = 0;
        if (pa_simple_write(p->pa, buf->data, buf->size, &pa_err) < 0) {
            fprintf(stderr, "[sink_speaker] pa_simple_write error: %s\n", pa_strerror(pa_err));
        } else if (media_debug_enabled()) {
            fprintf(stderr, "[sink_speaker] wrote %u frames (buf=%zu)\n", frames, buf->size);
        }
        { unsigned long us = (unsigned long)frames * 1000000UL / (p->sample_rate ? p->sample_rate : 48000);
          if (us > 0 && us < 500000) usleep((useconds_t)us); }
    }
#elif defined(USE_ALSA)
    if (p->pcm) {
        snd_pcm_sframes_t w = snd_pcm_writei(p->pcm, buf->data, frames);
        if (w == -EPIPE) {
            snd_pcm_prepare(p->pcm);
            w = snd_pcm_writei(p->pcm, buf->data, frames);
        }
        if (w == -ESTRPIPE) {
            snd_pcm_prepare(p->pcm);
            w = snd_pcm_writei(p->pcm, buf->data, frames);
        }
        if (media_debug_enabled()) fprintf(stderr, "[sink_speaker] wrote %ld/%u frames (buf=%zu)\n", (long)w, frames, buf->size);
        if (w < 0) fprintf(stderr, "[sink_speaker] snd_pcm_writei error: %s\n", snd_strerror((int)w));
        else { unsigned long us = (unsigned long)frames * 1000000UL / (p->sample_rate ? p->sample_rate : 48000);
          if (us > 0 && us < 500000) usleep((useconds_t)us); }
    }
#else
    if (p->pcm) {
        int w = pcm_writei(p->pcm, buf->data, frames);
        if (w < 0 && pcm_prepare(p->pcm) == 0)
            pcm_start(p->pcm);
    }
#endif
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
        const hal_audio_out_ops_t *ops = hal_get_audio_out_ops();
        if (ops && p->hal_handle && ops->close)
            ops->close(p->hal_handle);
#ifdef __linux__
#ifdef USE_PAPLAY_PIPE
        if (p->paplay_fp) pclose(p->paplay_fp);
#elif defined(USE_PULSE)
        if (p->pa) {
            int pa_err = 0;
            pa_simple_drain(p->pa, &pa_err);
            pa_simple_free(p->pa);
        }
#elif defined(USE_ALSA)
        if (p->pcm) {
            snd_pcm_drain(p->pcm);
            snd_pcm_close(p->pcm);
        }
#else
        if (p->pcm) pcm_close(p->pcm);
#endif
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
    .set_volume = NULL,
};

static const config_param_t sink_speaker_params[] = {
    { "sample_rate", CONFIG_UINT32, 0, 48000 },
    { "channels", CONFIG_UINT32, 0, 1 },
};
static const plugin_config_schema_t sink_speaker_schema = {
    .params = sink_speaker_params,
    .num_params = 2,
};

static const plugin_descriptor_t sink_speaker_plugin = {
    .name = "sink_speaker",
    .ops = &sink_speaker_ops,
    .num_input_ports = 1,
    .num_output_ports = 0,
    .capabilities = PLUGIN_CAP_SINK | PLUGIN_CAP_HAL_AUDIO,
    .config_schema = &sink_speaker_schema,
};

media_node_t* sink_speaker_create(const char *instance_id, const node_config_t *config) {
    media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
    if (!node) return NULL;
    node->desc = (const node_descriptor_t *)&sink_speaker_plugin;
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
