/**
 * @file config_schema.h
 * @brief Plugin config schema: param types, validation, and getter APIs.
 */
#ifndef MEDIA_CORE_CONFIG_SCHEMA_H
#define MEDIA_CORE_CONFIG_SCHEMA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CONFIG_UINT32,
    CONFIG_STRING,
    CONFIG_PTR
} config_value_type_t;

typedef struct config_param {
    const char *key;
    config_value_type_t type;
    int required;
    uint32_t default_uint32;
} config_param_t;

typedef struct plugin_config_schema {
    const config_param_t *params;
    int num_params;
} plugin_config_schema_t;

/** Key-value pair for extended config (used when opaque/kv_store is populated) */
typedef struct config_kv_pair {
    char key[32];
    config_value_type_t type;
    union {
        uint32_t u32;
        const char *str;
        void *ptr;
    } value;
} config_kv_pair_t;

/**
 * Get uint32 from config. Checks struct fields: sample_rate, channels, input_count, frame_ms, format.
 */
uint32_t config_get_uint32(const void *config, const char *key, uint32_t default_val);

/**
 * Get string from config. Returns config->device for "device", config->path for "path", etc.
 */
const char* config_get_string(const void *config, const char *key, const char *default_val);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CORE_CONFIG_SCHEMA_H */
