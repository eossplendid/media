/**
 * @brief Internal pipeline/link structures (shared by pipeline.c and link.c).
 */
#ifndef MEDIA_CORE_PIPELINE_INTERNAL_H
#define MEDIA_CORE_PIPELINE_INTERNAL_H

#include "../../include/media_core/node.h"
#include "../../include/media_core/session.h"
#include "../../include/media_core/pipeline.h"
#include <stdint.h>
#include <stddef.h>

#define MAX_NODES 32
#define MAX_LINKS 64

typedef struct link_entry {
    char from_id[64];
    int from_port;
    char to_id[64];
    int to_port;
    int active;  /* 0 = removed */
} link_entry_t;

struct pipeline {
    session_t *session;
    uint32_t id;
    int running;
    pipeline_mode_t mode;
    char mix_target[64];  /* 非空时表示输出到该 bus，创建第二条时自动协商 mixer */
    uint8_t pull_processed[MAX_NODES];  /* pull 模式下每轮已处理标记 */
    media_node_t *nodes[MAX_NODES];
    char *node_ids[MAX_NODES];
    node_config_t node_configs[MAX_NODES];
    int node_tick_period[MAX_NODES];  /* 0=每 tick 运行，N=每 N tick 运行一次 */
    uint32_t base_tick_ms;             /* 最小帧长，pipeline 每 tick 对应 base_tick_ms */
    int num_nodes;
    link_entry_t links[MAX_LINKS];
    int num_links;
};

#endif
