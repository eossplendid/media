/**
 * @file mic_to_speaker.c
 * @brief Demo: microphone -> speaker (single pipeline).
 */
#include "media_core/session.h"
#include "media_core/pipeline.h"
#include "media_core/factory.h"
#include "media_core/link.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <time.h>
#endif

extern media_node_t* source_mic_create(const char *, const node_config_t *);
extern void source_mic_destroy_fn(media_node_t *);
extern media_node_t* sink_speaker_create(const char *, const node_config_t *);
extern void sink_speaker_destroy_fn(media_node_t *);

int main(void) {
    session_t *sess = session_create();
    if (!sess) { fprintf(stderr, "session_create failed\n"); return 1; }
    pipeline_t *pipe = pipeline_create(sess, 1);
    if (!pipe) { fprintf(stderr, "pipeline_create failed\n"); session_destroy(sess); return 1; }

    factory_register_node_type("source_mic", (node_create_fn)source_mic_create, source_mic_destroy_fn);
    factory_register_node_type("sink_speaker", (node_create_fn)sink_speaker_create, sink_speaker_destroy_fn);

    node_config_t mic_cfg = { .sample_rate = 16000, .channels = 1 };
    node_config_t spk_cfg = { .sample_rate = 16000, .channels = 1 };

    if (pipeline_add_node(pipe, "source_mic", "mic", &mic_cfg) != 0) goto fail;
    if (pipeline_add_node(pipe, "sink_speaker", "spk", &spk_cfg) != 0) goto fail;
    if (pipeline_link(pipe, "mic", 0, "spk", 0) != 0) goto fail;

    printf("Mic -> Speaker (10 sec)...\n");
    session_start_pipeline(sess, 1);
#ifdef _WIN32
    Sleep(10000);
#else
    sleep(10);
#endif
    session_stop_pipeline(sess, 1);
#ifdef _WIN32
    Sleep(200);
#else
    { struct timespec ts = { 0, 200000000 }; nanosleep(&ts, NULL); }
#endif
    session_destroy(sess);
    printf("Done.\n");
    return 0;
fail:
    session_destroy(sess);
    return 1;
}
