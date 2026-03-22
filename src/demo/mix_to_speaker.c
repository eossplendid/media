/**
 * @file mix_to_speaker.c
 * @brief Demo: file (48k) + mic (16k) -> resampler -> mixer -> speaker.
 */
#include "media_core/session.h"
#include "media_core/pipeline.h"
#include "media_core/factory.h"
#include "media_core/link.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

extern media_node_t* source_mic_create(const char *, const node_config_t *);
extern void source_mic_destroy_fn(media_node_t *);
extern media_node_t* source_file_create(const char *, const node_config_t *);
extern void source_file_destroy_fn(media_node_t *);
extern media_node_t* sink_speaker_create(const char *, const node_config_t *);
extern void sink_speaker_destroy_fn(media_node_t *);
extern media_node_t* resampler_create(const char *, const node_config_t *);
extern void resampler_destroy_fn(media_node_t *);
extern media_node_t* mixer_create(const char *, const node_config_t *);
extern void mixer_destroy_fn(media_node_t *);

int main(int argc, char **argv) {
    const char *wav_path = argc > 1 ? argv[1] : "record.wav";
    session_t *sess = session_create();
    if (!sess) { fprintf(stderr, "session_create failed\n"); return 1; }
    pipeline_t *pipe = pipeline_create(sess, 1);
    if (!pipe) { fprintf(stderr, "pipeline_create failed\n"); session_destroy(sess); return 1; }

    factory_register_node_type("source_mic", (node_create_fn)source_mic_create, source_mic_destroy_fn);
    factory_register_node_type("source_file", (node_create_fn)source_file_create, source_file_destroy_fn);
    factory_register_node_type("filter_resampler", (node_create_fn)resampler_create, resampler_destroy_fn);
    factory_register_node_type("filter_mixer", (node_create_fn)mixer_create, mixer_destroy_fn);
    factory_register_node_type("sink_speaker", (node_create_fn)sink_speaker_create, sink_speaker_destroy_fn);

    node_config_t mic_cfg = { .sample_rate = 16000, .channels = 1 };
    node_config_t file_cfg = { .path = wav_path };
    node_config_t resamp_cfg = { .sample_rate = 48000 };
    node_config_t mix_cfg = { .input_count = 2 };
    node_config_t spk_cfg = { .sample_rate = 48000, .channels = 1 };

    if (pipeline_add_node(pipe, "source_mic", "mic", &mic_cfg) != 0) goto fail;
    if (pipeline_add_node(pipe, "source_file", "file", &file_cfg) != 0) goto fail;
    if (pipeline_add_node(pipe, "filter_resampler", "resamp", &resamp_cfg) != 0) goto fail;
    if (pipeline_add_node(pipe, "filter_mixer", "mix", &mix_cfg) != 0) goto fail;
    if (pipeline_add_node(pipe, "sink_speaker", "spk", &spk_cfg) != 0) goto fail;

    pipeline_link(pipe, "mic", 0, "resamp", 0);
    pipeline_link(pipe, "resamp", 0, "mix", 0);
    pipeline_link(pipe, "file", 0, "mix", 1);
    pipeline_link(pipe, "mix", 0, "spk", 0);

    printf("Mix %s + mic -> speaker (15 sec)...\n", wav_path);
    session_start_pipeline(sess, 1);
    sleep(15);
    session_stop_pipeline(sess, 1);
    { struct timespec ts = { 0, 200000000 }; nanosleep(&ts, NULL); }
    session_destroy(sess);
    printf("Done.\n");
    return 0;
fail:
    session_destroy(sess);
    return 1;
}
