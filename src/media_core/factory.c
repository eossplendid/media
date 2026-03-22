/****************************************************************************
 * src/media_core/factory.c
 *
 * Node type registration and create by name.
 *
 * 本模块实现节点类型的注册与按名创建：
 *   - 插件通过 factory_register_node_type 注册 create/destroy 回调
 *   - pipeline 添加节点时通过 factory_create_node 按类型名创建实例
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

#include "../../include/media_core/factory.h"
#include "../../include/media_core/node.h"
#include <string.h>
#include <stdlib.h>

#define MAX_TYPES 24  /* 最大注册节点类型数 */

typedef struct reg
{
  char name[64];
  node_create_fn create_fn;
  node_destroy_fn destroy_fn;
} reg_t;

static reg_t g_regs[MAX_TYPES];
static int g_num_regs;

/* 通过 plugin_descriptor 注册节点类型（内部转为 register_node_type） */
int factory_register_plugin(const plugin_descriptor_t *desc, node_create_fn create_fn,
                            node_destroy_fn destroy_fn)
{
  if (!desc || !create_fn || !destroy_fn || !desc->name) return -1;
  return factory_register_node_type(desc->name, create_fn, destroy_fn);
}

/* 注册节点类型：同名可覆盖，满则返回 -1 */
int factory_register_node_type(const char *name, node_create_fn create_fn,
                               node_destroy_fn destroy_fn)
{
  if (!name || !create_fn || !destroy_fn || g_num_regs >= MAX_TYPES)
    return -1;
  size_t n = strlen(name);
  if (n >= sizeof(g_regs[0].name)) return -1;
  for (int i = 0; i < g_num_regs; i++)
    {
      if (strcmp(g_regs[i].name, name) == 0)
        {
          g_regs[i].create_fn = create_fn;
          g_regs[i].destroy_fn = destroy_fn;
          return 0;
        }
    }
  memcpy(g_regs[g_num_regs].name, name, n + 1);
  g_regs[g_num_regs].create_fn = create_fn;
  g_regs[g_num_regs].destroy_fn = destroy_fn;
  g_num_regs++;
  return 0;
}

/* 按类型名创建节点实例，未注册类型返回 NULL */
media_node_t *factory_create_node(const char *type_name, const char *instance_id,
                                  const node_config_t *config)
{
  if (!type_name || !instance_id) return NULL;
  for (int i = 0; i < g_num_regs; i++)
    {
      if (strcmp(g_regs[i].name, type_name) != 0) continue;
      return g_regs[i].create_fn(instance_id, config);
    }
  return NULL;
}

void factory_unregister_node_type(const char *name)
{
  for (int i = 0; i < g_num_regs; i++)
    {
      if (strcmp(g_regs[i].name, name) != 0) continue;
      memmove(&g_regs[i], &g_regs[i + 1], (g_num_regs - 1 - i) * sizeof(reg_t));
      g_num_regs--;
      return;
    }
}

/* 根据节点 desc->name 查找注册的 destroy_fn 并调用 */
void factory_destroy_node(media_node_t *node)
{
  if (!node) return;
  for (int i = 0; i < g_num_regs; i++)
    {
      if (node->desc && strcmp(g_regs[i].name, node->desc->name) == 0)
        {
          g_regs[i].destroy_fn(node);
          return;
        }
    }
}
