/**
 * @file playback_group.c
 * @brief 动态多源播放组实现：多源时自动插入 mixer。
 */
#include "../../include/media_core/playback_group.h"
#include "../../include/media_core/session.h"
#include "../../include/media_core/pipeline.h"
#include "../../include/media_core/factory.h"
#include "../../include/media_core/link.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#define MAX_SOURCES 8

struct playback_group {
    session_t *session;
    pipeline_t *pipe;
    pipeline_id_t pipe_id;
    uint32_t output_sample_rate;
    uint32_t output_channels;
    char *sources[MAX_SOURCES];
    int num_sources;
    int running;
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
};

static void rebuild_pipeline(playback_group_t *pg) {
    if (pg->pipe) {
        session_detach(pg->session, pg->pipe);
        pipeline_destroy(pg->pipe);
        pg->pipe = NULL;
    }
    if (pg->num_sources == 0) return;

    pg->pipe = pipeline_create(pg->session, pg->pipe_id);
    if (!pg->pipe) return;

    uint32_t rate = pg->output_sample_rate ? pg->output_sample_rate : 48000;
    uint32_t ch = pg->output_channels ? pg->output_channels : 1;

    if (pg->num_sources == 1) {
        node_config_t file_cfg = { .path = pg->sources[0] };
        node_config_t spk_cfg = { .sample_rate = rate, .channels = ch };
        pipeline_add_node(pg->pipe, "source_file", "file0", &file_cfg);
        pipeline_add_node(pg->pipe, "sink_speaker", "spk", &spk_cfg);
        pipeline_link(pg->pipe, "file0", 0, "spk", 0);
    } else {
        node_config_t mix_cfg = { .input_count = (uint32_t)pg->num_sources, .sample_rate = rate };
        node_config_t spk_cfg = { .sample_rate = rate, .channels = ch };
        for (int i = 0; i < pg->num_sources; i++) {
            char id[32];
            snprintf(id, sizeof(id), "file%d", i);
            node_config_t file_cfg = { .path = pg->sources[i] };
            pipeline_add_node(pg->pipe, "source_file", id, &file_cfg);
        }
        pipeline_add_node(pg->pipe, "filter_mixer", "mix", &mix_cfg);
        pipeline_add_node(pg->pipe, "sink_speaker", "spk", &spk_cfg);
        for (int i = 0; i < pg->num_sources; i++) {
            char id[32];
            snprintf(id, sizeof(id), "file%d", i);
            pipeline_link(pg->pipe, id, 0, "mix", i);
        }
        pipeline_link(pg->pipe, "mix", 0, "spk", 0);
    }
}

playback_group_t* playback_group_create(session_t *session, const playback_group_config_t *config) {
    if (!session) return NULL;
    playback_group_t *pg = (playback_group_t *)calloc(1, sizeof(playback_group_t));
    if (!pg) return NULL;
    pg->session = session;
    pg->pipe_id = 1;
    if (config) {
        pg->output_sample_rate = config->output_sample_rate;
        pg->output_channels = config->output_channels;
    }
#ifdef _WIN32
    InitializeCriticalSection(&pg->lock);
#else
    pthread_mutex_init(&pg->lock, NULL);
#endif
    return pg;
}

void playback_group_destroy(playback_group_t *pg) {
    if (!pg) return;
#ifdef _WIN32
    EnterCriticalSection(&pg->lock);
#else
    pthread_mutex_lock(&pg->lock);
#endif
    pg->running = 0;
    if (pg->pipe) {
        session_detach(pg->session, pg->pipe);
        pipeline_destroy(pg->pipe);
        pg->pipe = NULL;
    }
    for (int i = 0; i < pg->num_sources; i++) {
        free(pg->sources[i]);
        pg->sources[i] = NULL;
    }
    pg->num_sources = 0;
#ifdef _WIN32
    LeaveCriticalSection(&pg->lock);
    DeleteCriticalSection(&pg->lock);
#else
    pthread_mutex_unlock(&pg->lock);
    pthread_mutex_destroy(&pg->lock);
#endif
    free(pg);
}

int playback_group_add_source(playback_group_t *pg, const char *path) {
    if (!pg || !path) return -1;
#ifdef _WIN32
    EnterCriticalSection(&pg->lock);
#else
    pthread_mutex_lock(&pg->lock);
#endif
    if (pg->num_sources >= MAX_SOURCES) {
#ifdef _WIN32
        LeaveCriticalSection(&pg->lock);
#else
        pthread_mutex_unlock(&pg->lock);
#endif
        return -1;
    }
    pg->sources[pg->num_sources] = strdup(path);
    if (!pg->sources[pg->num_sources]) {
#ifdef _WIN32
        LeaveCriticalSection(&pg->lock);
#else
        pthread_mutex_unlock(&pg->lock);
#endif
        return -1;
    }
    pg->num_sources++;
    int was_running = pg->running;
    rebuild_pipeline(pg);
    if (was_running && pg->pipe)
        session_start_pipeline(pg->session, pg->pipe_id);
#ifdef _WIN32
    LeaveCriticalSection(&pg->lock);
#else
    pthread_mutex_unlock(&pg->lock);
#endif
    return pg->num_sources - 1;
}

void playback_group_remove_source(playback_group_t *pg, int source_id) {
    if (!pg || source_id < 0 || source_id >= pg->num_sources) return;
#ifdef _WIN32
    EnterCriticalSection(&pg->lock);
#else
    pthread_mutex_lock(&pg->lock);
#endif
    free(pg->sources[source_id]);
    for (int i = source_id; i < pg->num_sources - 1; i++)
        pg->sources[i] = pg->sources[i + 1];
    pg->sources[pg->num_sources - 1] = NULL;
    pg->num_sources--;
    int was_running = pg->running;
    rebuild_pipeline(pg);
    if (was_running && pg->pipe)
        session_start_pipeline(pg->session, pg->pipe_id);
#ifdef _WIN32
    LeaveCriticalSection(&pg->lock);
#else
    pthread_mutex_unlock(&pg->lock);
#endif
}

int playback_group_start(playback_group_t *pg) {
    if (!pg) return -1;
#ifdef _WIN32
    EnterCriticalSection(&pg->lock);
#else
    pthread_mutex_lock(&pg->lock);
#endif
    if (pg->num_sources == 0 || !pg->pipe) {
#ifdef _WIN32
        LeaveCriticalSection(&pg->lock);
#else
        pthread_mutex_unlock(&pg->lock);
#endif
        return -1;
    }
    pg->running = 1;
    int r = session_start_pipeline(pg->session, pg->pipe_id);
#ifdef _WIN32
    LeaveCriticalSection(&pg->lock);
#else
    pthread_mutex_unlock(&pg->lock);
#endif
    return r;
}

void playback_group_stop(playback_group_t *pg) {
    if (!pg) return;
#ifdef _WIN32
    EnterCriticalSection(&pg->lock);
#else
    pthread_mutex_lock(&pg->lock);
#endif
    pg->running = 0;
    session_stop_pipeline(pg->session, pg->pipe_id);
#ifdef _WIN32
    LeaveCriticalSection(&pg->lock);
#else
    pthread_mutex_unlock(&pg->lock);
#endif
}

int playback_group_source_count(const playback_group_t *pg) {
    return pg ? pg->num_sources : 0;
}
