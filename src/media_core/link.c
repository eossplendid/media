/****************************************************************************
 * src/media_core/link.c
 *
 * pipeline_link, pipeline_remove_link, pipeline_remove_node.
 *
 * 本模块实现流水线内节点间的连接管理：
 *   - pipeline_link：建立 from_port -> to_port 的有向连接
 *   - pipeline_remove_link：按端口精确删除一条 link
 *   - pipeline_remove_node：删除节点并清除其所有相关 link
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

#include "../../include/media_core/link.h"
#include "../../include/media_core/factory.h"
#include "pipeline_internal.h"
#include <string.h>
#include <stdlib.h>

/* 根据 node_id 查找节点下标，供本文件内部使用 */
static int find_node_index(const pipeline_t *pipe, const char *node_id)
{
  for (int i = 0; i < pipe->num_nodes; i++)
    {
      if (pipe->node_ids[i] && strcmp(pipe->node_ids[i], node_id) == 0)
        return i;
    }
  return -1;
}

/* 建立连接：from 的 from_port -> to 的 to_port，运行中禁止修改 */
int pipeline_link(pipeline_t *pipe, const char *from_node_id, int from_port,
                  const char *to_node_id, int to_port)
{
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

/* 删除指定的一条 link（软删除，置 active=0） */
int pipeline_remove_link(pipeline_t *pipe, const char *from_node_id, int from_port,
                         const char *to_node_id, int to_port)
{
  if (!pipe || pipe->running) return -1;
  for (int i = 0; i < pipe->num_links; i++)
    {
      link_entry_t *e = &pipe->links[i];
      if (!e->active) continue;
      if (strcmp(e->from_id, from_node_id) == 0 && e->from_port == from_port &&
          strcmp(e->to_id, to_node_id) == 0 && e->to_port == to_port)
        {
          e->active = 0;
          return 0;
        }
    }
  return -1;
}

/* 删除节点：清除其所有 link，从数组移除并销毁节点实例 */
int pipeline_remove_node(pipeline_t *pipe, const char *node_id)
{
  if (!pipe || !node_id || pipe->running) return -1;
  int idx = find_node_index(pipe, node_id);
  if (idx < 0) return -1;
  /* 将该节点涉及的所有 link 置为无效 */
  for (int i = 0; i < pipe->num_links; i++)
    {
      link_entry_t *e = &pipe->links[i];
      if (!e->active) continue;
      if (strcmp(e->from_id, node_id) == 0 || strcmp(e->to_id, node_id) == 0)
        e->active = 0;
    }
  /* 将最后一个节点与当前槽位交换，实现 O(1) 删除 */
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
