/**
 * @file factory.h
 * @brief Node type registration and per-instance creation (reentrant).
 */
#ifndef MEDIA_CORE_FACTORY_H
#define MEDIA_CORE_FACTORY_H

#include "node.h"
#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Create function: returns new node instance for given type and config */
typedef media_node_t* (*node_create_fn)(const char *instance_id, const node_config_t *config);
/** Destroy function: frees node and its private_data */
typedef void (*node_destroy_fn)(media_node_t *node);

/**
 * Register a node type by name. Reentrant: each pipeline_add_node creates a new instance.
 * @param name unique type name (e.g. "source_mic", "sink_wav")
 * @param create_fn creates node instance
 * @param destroy_fn destroys node instance
 * @return 0 on success
 */
int factory_register_node_type(const char *name, node_create_fn create_fn, node_destroy_fn destroy_fn);

/**
 * Register a plugin by descriptor (extended with capabilities and config schema).
 * Backward compatible: can pass plugin_descriptor with config_schema=NULL, capabilities=0.
 */
int factory_register_plugin(const plugin_descriptor_t *desc, node_create_fn create_fn, node_destroy_fn destroy_fn);

/**
 * Create a node instance by type name (used by pipeline_add_node).
 * @return new media_node_t or NULL
 */
media_node_t* factory_create_node(const char *type_name, const char *instance_id,
                                  const node_config_t *config);

/** Unregister by name (optional, for cleanup) */
void factory_unregister_node_type(const char *name);

/** Destroy a node instance (calls registered destroy_fn). Used by pipeline. */
void factory_destroy_node(media_node_t *node);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CORE_FACTORY_H */
