/**
 * @file hal_audio_linux_alsa.c
 * @brief ALSA platform implementation for hal_audio_in and hal_audio_out.
 */
#include "media_core/hal_audio.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef __linux__
#include <alsa/asoundlib.h>
#endif

#ifdef __linux__
typedef struct {
    snd_pcm_t *pcm;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bytes_per_frame;
    uint32_t period_frames;
    int64_t next_pts_us;
    uint32_t frame_ms;
} alsa_handle_t;

static int alsa_in_open(void **handle, const hal_audio_config_t *config) {
    if (!handle || !config) return -1;
    alsa_handle_t *h = (alsa_handle_t *)calloc(1, sizeof(alsa_handle_t));
    if (!h) return -1;
    h->sample_rate = config->sample_rate ? config->sample_rate : 16000;
    h->channels = config->channels ? config->channels : 1;
    h->frame_ms = config->frame_ms ? config->frame_ms : 20;
    h->period_frames = config->period_frames ? config->period_frames :
        (h->sample_rate * h->frame_ms / 1000);
    h->bytes_per_frame = h->channels * (config->format == MEDIA_FORMAT_S16 ? 2 :
        config->format == MEDIA_FORMAT_F32 || config->format == MEDIA_FORMAT_S32 ? 4 : 1);

    int err = snd_pcm_open(&h->pcm, config->device && config->device[0] ? config->device : "default",
                           SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "[hal_alsa] snd_pcm_open(capture) failed: %s\n", snd_strerror(err));
        free(h);
        return -1;
    }
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(h->pcm, params);
    snd_pcm_hw_params_set_access(h->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(h->pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(h->pcm, params, h->channels);
    unsigned int rate = h->sample_rate;
    snd_pcm_hw_params_set_rate_near(h->pcm, params, &rate, NULL);
    snd_pcm_uframes_t period_size = (snd_pcm_uframes_t)h->period_frames;
    snd_pcm_hw_params_set_period_size_near(h->pcm, params, &period_size, NULL);
    snd_pcm_hw_params_set_periods(h->pcm, params, 4, 0);
    err = snd_pcm_hw_params(h->pcm, params);
    if (err < 0) {
        fprintf(stderr, "[hal_alsa] snd_pcm_hw_params(capture) failed: %s\n", snd_strerror(err));
        snd_pcm_close(h->pcm);
        free(h);
        return -1;
    }
    snd_pcm_prepare(h->pcm);
    snd_pcm_start(h->pcm);
    *handle = h;
    return 0;
}

static int alsa_in_read(void *handle, uint8_t *buf, size_t frames, int64_t *pts) {
    if (!handle || !buf) return -1;
    alsa_handle_t *h = (alsa_handle_t *)handle;
    snd_pcm_sframes_t n = snd_pcm_readi(h->pcm, buf, (snd_pcm_uframes_t)frames);
    if (n <= 0) {
        if (n == -EPIPE) snd_pcm_prepare(h->pcm);
        return (int)n;
    }
    if (pts) *pts = h->next_pts_us;
    h->next_pts_us += (int64_t)h->frame_ms * 1000;
    return (int)n;
}

static void alsa_in_close(void *handle) {
    if (!handle) return;
    alsa_handle_t *h = (alsa_handle_t *)handle;
    if (h->pcm) {
        snd_pcm_drop(h->pcm);
        snd_pcm_close(h->pcm);
    }
    free(h);
}

static int alsa_out_open(void **handle, const hal_audio_config_t *config) {
    if (!handle || !config) return -1;
    alsa_handle_t *h = (alsa_handle_t *)calloc(1, sizeof(alsa_handle_t));
    if (!h) return -1;
    h->sample_rate = config->sample_rate ? config->sample_rate : 48000;
    h->channels = config->channels ? config->channels : 1;
    h->bytes_per_frame = h->channels * 2;
    h->period_frames = config->period_frames ? config->period_frames : 1024;

    const char *devices[] = { "default", "pulse", "sysdefault:CARD=default", NULL };
    int err = -1;
    for (int i = 0; devices[i]; i++) {
        err = snd_pcm_open(&h->pcm, devices[i], SND_PCM_STREAM_PLAYBACK, 0);
        if (err >= 0) break;
    }
    if (err < 0) {
        fprintf(stderr, "[hal_alsa] snd_pcm_open(playback) failed: %s\n", snd_strerror(err));
        free(h);
        return -1;
    }
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(h->pcm, params);
    snd_pcm_hw_params_set_access(h->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(h->pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(h->pcm, params, h->channels);
    unsigned int rate = h->sample_rate;
    snd_pcm_hw_params_set_rate_near(h->pcm, params, &rate, NULL);
    snd_pcm_uframes_t period_size = 1024;
    int dir = 0;
    snd_pcm_hw_params_set_period_size_near(h->pcm, params, &period_size, &dir);
    unsigned int periods = 4;
    snd_pcm_hw_params_set_periods_near(h->pcm, params, &periods, &dir);
    err = snd_pcm_hw_params(h->pcm, params);
    if (err < 0) {
        fprintf(stderr, "[hal_alsa] snd_pcm_hw_params(playback) failed: %s\n", snd_strerror(err));
        snd_pcm_close(h->pcm);
        free(h);
        return -1;
    }
    snd_pcm_prepare(h->pcm);
    *handle = h;
    return 0;
}

static int alsa_out_write(void *handle, const uint8_t *buf, size_t frames) {
    if (!handle || !buf) return -1;
    alsa_handle_t *h = (alsa_handle_t *)handle;
    snd_pcm_sframes_t w = snd_pcm_writei(h->pcm, buf, (snd_pcm_uframes_t)frames);
    if (w == -EPIPE) {
        snd_pcm_prepare(h->pcm);
        w = snd_pcm_writei(h->pcm, buf, (snd_pcm_uframes_t)frames);
    }
    if (w == -ESTRPIPE) {
        snd_pcm_prepare(h->pcm);
        w = snd_pcm_writei(h->pcm, buf, (snd_pcm_uframes_t)frames);
    }
    return w < 0 ? (int)w : (int)w;
}

static void alsa_out_close(void *handle) {
    if (!handle) return;
    alsa_handle_t *h = (alsa_handle_t *)handle;
    if (h->pcm) {
        snd_pcm_drain(h->pcm);
        snd_pcm_close(h->pcm);
    }
    free(h);
}

static const hal_audio_in_ops_t alsa_in_ops = {
    .open = alsa_in_open,
    .read = alsa_in_read,
    .close = alsa_in_close,
};

static const hal_audio_out_ops_t alsa_out_ops = {
    .open = alsa_out_open,
    .write = alsa_out_write,
    .close = alsa_out_close,
};

static void __attribute__((constructor)) hal_alsa_register(void) {
    hal_register_audio_in(&alsa_in_ops);
    hal_register_audio_out(&alsa_out_ops);
}
#else
static void __attribute__((constructor)) hal_alsa_register(void) {
    (void)0;
}
#endif
