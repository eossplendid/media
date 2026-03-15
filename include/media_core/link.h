/**
 * @file link.h
 * @brief Pipeline link: connect output port of one node to input port of another.
 */
#ifndef MEDIA_CORE_LINK_H
#define MEDIA_CORE_LINK_H

#include "node.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque pipeline */
typedef struct pipeline pipeline_t;

/**
 * Link output port of from_node to input port of to_node.
 * @return 0 on success, negative on error (e.g. invalid id or port index)
 */
int pipeline_link(pipeline_t *pipe, const char *from_node_id, int from_port,
                  const char *to_node_id, int to_port);

/**
 * Remove a link (allowed only when pipeline is stopped).
 * @return 0 on success
 */
int pipeline_remove_link(pipeline_t *pipe, const char *from_node_id, int from_port,
                         const char *to_node_id, int to_port);

/**
 * Remove a node and all its links (allowed only when pipeline is stopped).
 * @return 0 on success
 */
int pipeline_remove_node(pipeline_t *pipe, const char *node_id);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CORE_LINK_H */
