/**
 * @file config_schema.c
 * @brief Config getter implementation for node_config_t struct and kv extension.
 */
#include "../../include/media_core/config_schema.h"
#include "../../include/media_core/node.h"
#include <string.h>

uint32_t config_get_uint32(const void *config, const char *key, uint32_t default_val) {
    if (!config || !key) return default_val;
    const node_config_t *cfg = (const node_config_t *)config;

    if (strcmp(key, "sample_rate") == 0)
        return cfg->sample_rate != 0 ? cfg->sample_rate : default_val;
    if (strcmp(key, "channels") == 0)
        return cfg->channels != 0 ? cfg->channels : default_val;
    if (strcmp(key, "input_count") == 0)
        return cfg->input_count != 0 ? cfg->input_count : default_val;
    if (strcmp(key, "frame_ms") == 0)
        return cfg->frame_ms != 0 ? cfg->frame_ms : default_val;
    if (strcmp(key, "format") == 0)
        return cfg->format;

    return default_val;
}

const char* config_get_string(const void *config, const char *key, const char *default_val) {
    if (!config || !key) return default_val;
    const node_config_t *cfg = (const node_config_t *)config;

    if (strcmp(key, "device") == 0)
        return (cfg->device && cfg->device[0]) ? cfg->device : default_val;
    if (strcmp(key, "path") == 0)
        return (cfg->path && cfg->path[0]) ? cfg->path : default_val;

    return default_val;
}
