/****************************************************************************
 * src/media_core/config_schema.c
 *
 * Config getter implementation for node_config_t struct and kv extension.
 *
 * 本模块提供统一的配置读取接口，支持 node_config_t 中的常用字段：
 *   - config_get_uint32：sample_rate、channels、frame_ms、format 等
 *   - config_get_string：device、path 等
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 ****************************************************************************/

#include "../../include/media_core/config_schema.h"
#include "../../include/media_core/node.h"
#include <string.h>

uint32_t config_get_uint32(const void *config, const char *key,
                          uint32_t default_val)
{
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

/* 从 node_config_t 按 key 读取字符串，未设置时返回 default_val */
const char *config_get_string(const void *config, const char *key,
                              const char *default_val)
{
  if (!config || !key) return default_val;
  const node_config_t *cfg = (const node_config_t *)config;

  if (strcmp(key, "device") == 0)
    return (cfg->device && cfg->device[0]) ? cfg->device : default_val;
  if (strcmp(key, "path") == 0)
    return (cfg->path && cfg->path[0]) ? cfg->path : default_val;

  return default_val;
}
