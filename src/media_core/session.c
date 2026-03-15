/**
 * @file session.c
 * @brief Multi-pipeline management and per-pipeline thread.
 */
#include "../../include/media_core/session.h"
#include "../../include/media_core/pipeline.h"
#include "pipeline_internal.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#define MAX_PIPELINES 16

struct session {
    pipeline_t *pipelines[MAX_PIPELINES];
    int num_pipelines;
#ifdef _WIN32
    HANDLE threads[MAX_PIPELINES];
#else
    pthread_t threads[MAX_PIPELINES];
#endif
    int thread_used[MAX_PIPELINES];
};

#ifdef _WIN32
static DWORD WINAPI pipeline_thread_fn(LPVOID arg) {
    pipeline_t *pipe = (pipeline_t *)arg;
    pipeline_start(pipe);
    return 0;
}
#else
static void* pipeline_thread_fn(void *arg) {
    pipeline_t *pipe = (pipeline_t *)arg;
    pipeline_start(pipe);
    return NULL;
}
#endif

session_t* session_create(void) {
    session_t *s = (session_t *)calloc(1, sizeof(session_t));
    return s;
}

void session_attach(session_t *session, pipeline_t *pipe) {
    if (!session || !pipe || session->num_pipelines >= MAX_PIPELINES) return;
    session->pipelines[session->num_pipelines++] = pipe;
}

pipeline_t* session_get_pipeline(session_t *session, pipeline_id_t id) {
    if (!session) return NULL;
    for (int i = 0; i < session->num_pipelines; i++)
        if (session->pipelines[i] && pipeline_get_id(session->pipelines[i]) == id)
            return session->pipelines[i];
    return NULL;
}

int session_start_pipeline(session_t *session, pipeline_id_t id) {
    pipeline_t *pipe = session_get_pipeline(session, id);
    if (!pipe || pipeline_is_running(pipe)) return -1;
    int slot = -1;
    for (int i = 0; i < session->num_pipelines; i++)
        if (session->pipelines[i] == pipe) { slot = i; break; }
    if (slot < 0) return -1;
#ifdef _WIN32
    session->threads[slot] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)pipeline_thread_fn, pipe, 0, NULL);
    session->thread_used[slot] = (session->threads[slot] != NULL);
    return session->thread_used[slot] ? 0 : -1;
#else
    int r = pthread_create(&session->threads[slot], NULL, pipeline_thread_fn, pipe);
    session->thread_used[slot] = (r == 0);
    return r == 0 ? 0 : -1;
#endif
}

void session_stop_pipeline(session_t *session, pipeline_id_t id) {
    pipeline_t *pipe = session_get_pipeline(session, id);
    if (pipe) pipeline_stop(pipe);
}

int session_detach(session_t *session, pipeline_t *pipe) {
    if (!session || !pipe) return -1;
    int slot = -1;
    for (int i = 0; i < session->num_pipelines; i++)
        if (session->pipelines[i] == pipe) { slot = i; break; }
    if (slot < 0) return -1;
    pipeline_stop(pipe);
#ifdef _WIN32
    if (session->thread_used[slot] && session->threads[slot])
        WaitForSingleObject(session->threads[slot], INFINITE);
    session->thread_used[slot] = 0;
#else
    if (session->thread_used[slot])
        pthread_join(session->threads[slot], NULL);
    session->thread_used[slot] = 0;
#endif
    for (int i = slot; i < session->num_pipelines - 1; i++) {
        session->pipelines[i] = session->pipelines[i + 1];
        session->threads[i] = session->threads[i + 1];
        session->thread_used[i] = session->thread_used[i + 1];
    }
    session->pipelines[session->num_pipelines - 1] = NULL;
    session->num_pipelines--;
    return 0;
}

void session_destroy(session_t *session) {
    if (!session) return;
    for (int i = 0; i < session->num_pipelines; i++) {
        if (session->pipelines[i])
            pipeline_stop(session->pipelines[i]);
    }
#ifdef _WIN32
    for (int i = 0; i < session->num_pipelines; i++) {
        if (session->thread_used[i] && session->threads[i])
            WaitForSingleObject(session->threads[i], INFINITE);
    }
#else
    for (int i = 0; i < session->num_pipelines; i++) {
        if (session->thread_used[i])
            pthread_join(session->threads[i], NULL);
    }
#endif
    for (int i = 0; i < session->num_pipelines; i++) {
        if (session->pipelines[i]) {
            pipeline_destroy(session->pipelines[i]);
            session->pipelines[i] = NULL;
        }
    }
    session->num_pipelines = 0;
    free(session);
}
