/**
 * @file plugin.h
 * @brief Plugin descriptor: capabilities, schema, unified node registration.
 */
#ifndef MEDIA_CORE_PLUGIN_H
#define MEDIA_CORE_PLUGIN_H

#include "node.h"
#include "config_schema.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Plugin capability bitmask */
typedef enum {
    PLUGIN_CAP_SOURCE = 1,
    PLUGIN_CAP_SINK = 2,
    PLUGIN_CAP_FILTER = 4,
    PLUGIN_CAP_DECODER = 8,
    PLUGIN_CAP_VOLUME = 16,
    PLUGIN_CAP_HAL_AUDIO = 32
} plugin_cap_t;

/** Extended plugin descriptor: type name + ops + capabilities + optional schema */
typedef struct plugin_descriptor {
    const char *name;
    const node_ops_t *ops;
    int num_input_ports;
    int num_output_ports;
    uint32_t capabilities;
    const plugin_config_schema_t *config_schema;
} plugin_descriptor_t;

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CORE_PLUGIN_H */
