/**
 * @file dual_file_to_speaker.c
 * @brief Demo: 双线程动态混音。线程1读文件1→喇叭，线程2读文件2→喇叭；
 *        当两线程同时存在时，自动插入 mixer 软混后统一从喇叭播放。
 */
#include "media_core/session.h"
#include "media_core/playback_group.h"
#include "media_core/factory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

extern media_node_t* source_file_create(const char *, const node_config_t *);
extern void source_file_destroy_fn(media_node_t *);
extern media_node_t* sink_speaker_create(const char *, const node_config_t *);
extern void sink_speaker_destroy_fn(media_node_t *);
extern media_node_t* mixer_create(const char *, const node_config_t *);
extern void mixer_destroy_fn(media_node_t *);
extern media_node_t* resampler_create(const char *, const node_config_t *);
extern void resampler_destroy_fn(media_node_t *);

typedef struct {
    playback_group_t *pg;
    const char *path;
    int delay_ms;
} thread_arg_t;

static void* play_thread_fn(void *arg) {
    thread_arg_t *ta = (thread_arg_t *)arg;
    if (ta->delay_ms > 0)
        usleep(ta->delay_ms * 1000);
    int id = playback_group_add_source(ta->pg, ta->path);
    if (id >= 0) {
        printf("[thread] added source %d: %s\n", id, ta->path);
        playback_group_start(ta->pg);
    }
    return NULL;
}

int main(int argc, char **argv) {
    const char *file1 = "record.wav";
    const char *file2 = "record.wav";
    uint32_t output_rate = 48000;
    int file_idx = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--rate=", 7) == 0) {
            output_rate = (uint32_t)atoi(argv[i] + 7);
            if (output_rate < 8000) output_rate = 8000;
            if (output_rate > 192000) output_rate = 192000;
        } else if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            output_rate = (uint32_t)atoi(argv[++i]);
            if (output_rate < 8000) output_rate = 8000;
            if (output_rate > 192000) output_rate = 192000;
        } else if (file_idx == 0) {
            file1 = argv[i];
            file_idx = 1;
        } else {
            file2 = argv[i];
            break;
        }
    }

    session_t *sess = session_create();
    if (!sess) { fprintf(stderr, "session_create failed\n"); return 1; }

    factory_register_node_type("source_file", (node_create_fn)source_file_create, source_file_destroy_fn);
    factory_register_node_type("filter_mixer", (node_create_fn)mixer_create, mixer_destroy_fn);
    factory_register_node_type("filter_resampler", (node_create_fn)resampler_create, resampler_destroy_fn);
    factory_register_node_type("sink_speaker", (node_create_fn)sink_speaker_create, sink_speaker_destroy_fn);

    playback_group_config_t pg_cfg = { .output_sample_rate = output_rate, .output_channels = 1 };
    playback_group_t *pg = playback_group_create(sess, &pg_cfg);
    if (!pg) { fprintf(stderr, "playback_group_create failed\n"); session_destroy(sess); return 1; }

    thread_arg_t ta1 = { .pg = pg, .path = file1, .delay_ms = 500 };
    thread_arg_t ta2 = { .pg = pg, .path = file2, .delay_ms = 1500 };

    printf("Dual file to speaker: thread1=%s, thread2=%s, output=%uHz\n", file1, file2, output_rate);
    printf("t=0.5s: thread1 adds file1 -> speaker\n");
    printf("t=1.5s: thread2 adds file2 -> auto mixer -> speaker\n");
    printf("Playing 15 sec...\n");

    pthread_t t1, t2;
    pthread_create(&t1, NULL, play_thread_fn, &ta1);
    pthread_create(&t2, NULL, play_thread_fn, &ta2);
    sleep(15);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    playback_group_stop(pg);
    playback_group_destroy(pg);
    session_destroy(sess);
    printf("Done.\n");
    return 0;
}
