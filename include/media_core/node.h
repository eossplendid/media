/**
 * @file node.h
 * @brief Node abstraction: node_ops_t, media_node_t, ports, media_buffer_t.
 */
#ifndef MEDIA_CORE_NODE_H
#define MEDIA_CORE_NODE_H

#include "../media_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque node instance (defined in .c) */
typedef struct media_node media_node_t;

struct pipeline;

/** Configuration blob for node init (key-value or struct per node type) */
typedef struct node_config {
    const char *device;
    const char *path;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t input_count;  /* for mixer */
    uint32_t frame_ms;     /* 期望输入帧长(ms)，0=不限制；prepare 时用于协商 */
    void *opaque;
} node_config_t;

/** Single buffer passed between nodes */
typedef struct media_buffer {
    uint8_t *data;
    size_t size;
    media_ts_t pts;
    media_caps_t caps;
    void (*release)(struct media_buffer *buf);  /* optional; NULL = pipeline frees */
    void *opaque;
} media_buffer_t;

/** Node operations (vtable) */
typedef struct node_ops {
    int (*init)(media_node_t *node, const node_config_t *config);
    int (*prepare)(media_node_t *node, int port_index, const media_caps_t *upstream_caps, media_caps_t *out_caps);  /* 可选：协商 caps，port=输入端口时填写 out_caps 为协商结果 */
    int (*get_caps)(media_node_t *node, int port_index, media_caps_t *out_caps);
    int (*set_caps)(media_node_t *node, int port_index, const media_caps_t *caps);
    int (*process)(media_node_t *node);
    int (*flush)(media_node_t *node);
    void (*destroy)(media_node_t *node);
} node_ops_t;

/** Node descriptor: type name + ops + port counts */
typedef struct node_descriptor {
    const char *name;
    const node_ops_t *ops;
    int num_input_ports;
    int num_output_ports;
} node_descriptor_t;

/** Allocate a buffer (node or pipeline frees; release callback optional). */
media_buffer_t* media_buffer_alloc(size_t size);
/** Free buffer (and data unless release was set). */
void media_buffer_free(media_buffer_t *buf);

/** Node instance (opaque in API; fields used by core) */
struct media_node {
    const node_descriptor_t *desc;
    char *instance_id;
    void *private_data;
    struct pipeline *pipeline;
    int num_input_ports;
    int num_output_ports;
    /** Per-input: buffer from upstream (set by pipeline before process) */
    media_buffer_t *input_buffers[4];
    /** Per-output: buffer produced for downstream (set by node in process) */
    media_buffer_t *output_buffers[4];
};

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CORE_NODE_H */
