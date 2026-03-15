/**
 * @file session.h
 * @brief Multi-pipeline concurrency: session_create, start_pipeline, stop_pipeline.
 */
#ifndef MEDIA_CORE_SESSION_H
#define MEDIA_CORE_SESSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct session session_t;
typedef struct pipeline pipeline_t;
typedef uint32_t pipeline_id_t;

/** Create a session (holds multiple pipelines). */
session_t* session_create(void);

/**
 * Attach a pipeline to the session (pipeline was created with this session and id).
 * Used internally by pipeline_create(session, id).
 */
void session_attach(session_t *session, pipeline_t *pipe);

/** Start one pipeline by id (runs in its own thread). */
int session_start_pipeline(session_t *session, pipeline_id_t id);

/** Stop one pipeline by id. */
void session_stop_pipeline(session_t *session, pipeline_id_t id);

/** Get pipeline by id (NULL if not found). */
pipeline_t* session_get_pipeline(session_t *session, pipeline_id_t id);

/**
 * Detach and stop a pipeline (waits for its thread). Caller must pipeline_destroy.
 * Used when rebuilding pipeline topology (e.g. dynamic mixer insertion).
 */
int session_detach(session_t *session, pipeline_t *pipe);

/** Destroy session and all attached pipelines (stops them first). */
void session_destroy(session_t *session);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CORE_SESSION_H */
