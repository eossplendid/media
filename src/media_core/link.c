/**
 * @file link.c
 * @brief pipeline_link, pipeline_remove_link, pipeline_remove_node.
 */
#include "../../include/media_core/link.h"
#include "../../include/media_core/factory.h"
#include "pipeline_internal.h"
#include <string.h>
#include <stdlib.h>

static int find_node_index(const pipeline_t *pipe, const char *node_id) {
    for (int i = 0; i < pipe->num_nodes; i++)
        if (pipe->node_ids[i] && strcmp(pipe->node_ids[i], node_id) == 0)
            return i;
    return -1;
}

int pipeline_link(pipeline_t *pipe, const char *from_node_id, int from_port,
                  const char *to_node_id, int to_port) {
    if (!pipe || !from_node_id || !to_node_id || pipe->running)
        return -1;
    if (pipe->num_links >= MAX_LINKS) return -1;
    if (find_node_index(pipe, from_node_id) < 0 || find_node_index(pipe, to_node_id) < 0)
        return -1;
    link_entry_t *e = &pipe->links[pipe->num_links++];
    strncpy(e->from_id, from_node_id, sizeof(e->from_id) - 1);
    e->from_id[sizeof(e->from_id) - 1] = '\0';
    e->from_port = from_port;
    strncpy(e->to_id, to_node_id, sizeof(e->to_id) - 1);
    e->to_id[sizeof(e->to_id) - 1] = '\0';
    e->to_port = to_port;
    e->active = 1;
    return 0;
}

int pipeline_remove_link(pipeline_t *pipe, const char *from_node_id, int from_port,
                         const char *to_node_id, int to_port) {
    if (!pipe || pipe->running) return -1;
    for (int i = 0; i < pipe->num_links; i++) {
        link_entry_t *e = &pipe->links[i];
        if (!e->active) continue;
        if (strcmp(e->from_id, from_node_id) == 0 && e->from_port == from_port &&
            strcmp(e->to_id, to_node_id) == 0 && e->to_port == to_port) {
            e->active = 0;
            return 0;
        }
    }
    return -1;
}

int pipeline_remove_node(pipeline_t *pipe, const char *node_id) {
    if (!pipe || !node_id || pipe->running) return -1;
    int idx = find_node_index(pipe, node_id);
    if (idx < 0) return -1;
    /* Deactivate all links involving this node */
    for (int i = 0; i < pipe->num_links; i++) {
        link_entry_t *e = &pipe->links[i];
        if (!e->active) continue;
        if (strcmp(e->from_id, node_id) == 0 || strcmp(e->to_id, node_id) == 0)
            e->active = 0;
    }
    /* Remove node from array (swap with last) */
    media_node_t *node = pipe->nodes[idx];
    char *id_to_free = pipe->node_ids[idx];
    pipe->nodes[idx] = pipe->nodes[pipe->num_nodes - 1];
    pipe->node_ids[idx] = pipe->node_ids[pipe->num_nodes - 1];
    pipe->node_configs[idx] = pipe->node_configs[pipe->num_nodes - 1];
    pipe->nodes[pipe->num_nodes - 1] = NULL;
    pipe->node_ids[pipe->num_nodes - 1] = NULL;
    pipe->num_nodes--;
    free(id_to_free);
    factory_destroy_node(node);
    return 0;
}
