/**
 * @file pipeline.h
 * @brief Pipeline create/start/stop/destroy, add/remove nodes and links.
 */
#ifndef MEDIA_CORE_PIPELINE_H
#define MEDIA_CORE_PIPELINE_H

#include "node.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque session (optional; pipeline can exist without session) */
typedef struct session session_t;

/** Pipeline id for session mapping */
typedef uint32_t pipeline_id_t;

/** Opaque pipeline */
typedef struct pipeline pipeline_t;

/** Pipeline 驱动模式：push=源驱动，pull=宿驱动 */
typedef enum {
    PIPELINE_MODE_PUSH = 0,  /** 源驱动：按 topo 顺序推进，源产生数据推向下游 */
    PIPELINE_MODE_PULL = 1   /** 宿驱动：宿请求数据时递归向上游拉取 */
} pipeline_mode_t;

/**
 * 设置 pipeline 驱动模式（需在 start 前调用，默认 PUSH）
 */
void pipeline_set_mode(pipeline_t *pipe, pipeline_mode_t mode);

/** 获取当前模式 */
pipeline_mode_t pipeline_get_mode(const pipeline_t *pipe);

/**
 * Create a pipeline. Optionally attach to session later via session_attach.
 * @param session may be NULL; if non-NULL, pipeline is registered with session
 * @param id pipeline id for session (ignored if session is NULL)
 */
pipeline_t* pipeline_create(session_t *session, pipeline_id_t id);

/**
 * Add a node to the pipeline (creates instance via factory).
 * @param node_type_name registered type (e.g. "source_mic")
 * @param node_id unique instance id within this pipeline (e.g. "mic", "spk")
 * @return 0 on success
 */
int pipeline_add_node(pipeline_t *pipe, const char *node_type_name,
                      const char *node_id, const node_config_t *config);

/**
 * Prepare pipeline: caps 协商，frame_ms 不匹配时自动插入 filter_frame_adapter。
 * 在 pipeline_start 前调用，或由 pipeline_start 自动调用。
 */
int pipeline_prepare(pipeline_t *pipe);

/** Start pipeline (run process loop). Blocks until stop or error if single-threaded. */
int pipeline_start(pipeline_t *pipe);

/** Request stop (sets flag; process loop exits). */
void pipeline_stop(pipeline_t *pipe);

/** Destroy pipeline and all nodes. Call after stop. */
void pipeline_destroy(pipeline_t *pipe);

/** Get pipeline id (as given to pipeline_create). */
pipeline_id_t pipeline_get_id(const pipeline_t *pipe);

/** Check if pipeline is currently running. */
int pipeline_is_running(const pipeline_t *pipe);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CORE_PIPELINE_H */
