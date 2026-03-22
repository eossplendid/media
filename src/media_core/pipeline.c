/****************************************************************************
 * src/media_core/pipeline.c
 *
 * Pipeline create/add_node/start/stop/destroy and process loop.
 *
 * 本模块实现媒体流水线核心逻辑：
 *   - 节点增删、链接建立
 *   - 拓扑排序与执行模式（Push/Pull）
 *   - 能力协商：自动插入 resampler/frame_adapter/format_converter
 *   - 主循环：按 topo 顺序驱动各节点 process
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

#include "../../include/media_core/pipeline.h"
#include "../../include/media_core/session.h"
#include "../../include/media_core/factory.h"
#include "../../include/media_core/link.h"
#include "../../include/media_core/node.h"
#include "../../include/media_core/media_debug.h"
#include "pipeline_internal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* 根据 node_id 查找节点在 pipe 中的下标，找不到返回 -1 */
static int find_node_index(const pipeline_t *pipe, const char *node_id)
{
  for (int i = 0; i < pipe->num_nodes; i++)
    if (pipe->node_ids[i] && strcmp(pipe->node_ids[i], node_id) == 0)
      return i;
  return -1;
}

/* 创建流水线实例，关联 session 并自动 attach */
pipeline_t *pipeline_create(session_t *session, pipeline_id_t id)
{
  pipeline_t *pipe = (pipeline_t *)calloc(1, sizeof(pipeline_t));
  if (!pipe) return NULL;
  pipe->session = session;
  pipe->id = id;
  if (session)
    session_attach(session, pipe);
  return pipe;
}

/* 向流水线添加节点：按类型名创建，配置端口数，禁止 duplicate id */
int pipeline_add_node(pipeline_t *pipe, const char *node_type_name,
                      const char *node_id, const node_config_t *config)
{
  if (!pipe || !node_type_name || !node_id || pipe->running)
    return -1;
  if (pipe->num_nodes >= MAX_NODES) return -1;
  for (int i = 0; i < pipe->num_nodes; i++)
    if (strcmp(pipe->node_ids[i], node_id) == 0)
      return -1;  /* duplicate id */
  media_node_t *node = factory_create_node(node_type_name, node_id, config);
  if (!node) return -1;
  node->pipeline = pipe;
  pipe->nodes[pipe->num_nodes] = node;
  pipe->node_ids[pipe->num_nodes] = strdup(node_id);
  if (!pipe->node_ids[pipe->num_nodes])
    {
      factory_destroy_node(node);
      return -1;
    }
  if (config)
    pipe->node_configs[pipe->num_nodes] = *config;
  else
    memset(&pipe->node_configs[pipe->num_nodes], 0, sizeof(node_config_t));
  pipe->num_nodes++;
  if (node->desc)
    {
      node->num_input_ports = node->desc->num_input_ports;
      node->num_output_ports = node->desc->num_output_ports;
    }
  return 0;
}

/* 拓扑排序：按入度为 0 优先，每次选一个后更新下游入度，结果写入 order[] */
static int topo_order(const pipeline_t *pipe, int *order)
{
  int in_degree[MAX_NODES]; /* 每节点入边数 */
  memset(in_degree, 0, sizeof(in_degree));
  /* 根据 link 计算各节点入度 */
  for (int i = 0; i < pipe->num_links; i++)
    {
      const link_entry_t *e = &pipe->links[i];
      if (!e->active) continue;
      int to_idx = -1;
      for (int j = 0; j < pipe->num_nodes; j++)
        if (strcmp(pipe->node_ids[j], e->to_id) == 0)
          {
            to_idx = j;
            break;
          }
      if (to_idx >= 0) in_degree[to_idx]++;
    }
  int out = 0;
  /* Kahn 算法：每轮选入度为 0 的节点，加入拓扑序并更新后继入度 */
  for (int round = 0; round < pipe->num_nodes; round++)
    {
      int chosen = -1;
      for (int i = 0; i < pipe->num_nodes; i++)
        {
          int used = 0;
          for (int k = 0; k < out; k++)
            if (order[k] == i)
              {
                used = 1;
                break;
              }
          if (used) continue;
          if (in_degree[i] == 0)
            {
              chosen = i;
              break;
            }
        }
      if (chosen < 0) return -1;
      order[out++] = chosen;
      for (int i = 0; i < pipe->num_links; i++)
        {
          const link_entry_t *e = &pipe->links[i];
          if (!e->active) continue;
          if (strcmp(pipe->node_ids[chosen], e->from_id) != 0) continue;
          for (int j = 0; j < pipe->num_nodes; j++)
            if (strcmp(pipe->node_ids[j], e->to_id) == 0)
              {
                in_degree[j]--;
                break;
              }
        }
    }
  return out;
}

/* 按 link 把上游输出缓冲拷贝到本节点输入端口（由 process 前调用） */
static void feed_inputs(pipeline_t *pipe, media_node_t *node,
                        const char *node_id);

/* 判断是否 sink 节点：无任何出边（即无 link 的 from_id 指向该节点） */
static int is_sink_node(const pipeline_t *pipe, int node_idx)
{
  const char *nid = pipe->node_ids[node_idx];
  for (int i = 0; i < pipe->num_links; i++)
    {
      if (!pipe->links[i].active) continue;
      if (strcmp(pipe->links[i].from_id, nid) == 0) return 0;
    }
  return 1;
}

/* Pull 模式：从 sink 递归向上游拉取；先递归上游 produce，再 process 本节点 */
static int pull_chain(pipeline_t *pipe, int to_idx)
{
  if (pipe->pull_processed[to_idx]) return 0; /* 已处理则跳过 */
  media_node_t *node = pipe->nodes[to_idx];
  const char *nid = pipe->node_ids[to_idx];
  for (int i = 0; i < pipe->num_links; i++)
    {
      link_entry_t *e = &pipe->links[i];
      if (!e->active || strcmp(e->to_id, nid) != 0) continue;
      int from_idx = find_node_index(pipe, e->from_id);
      if (from_idx >= 0)
        {
          int r = pull_chain(pipe, from_idx); /* 递归拉取上游 */
          if (r != 0) return r;
        }
    }
  /* 清空输出端口，feed_inputs 填充输入，再 process */
  for (int p = 0; p < node->num_output_ports; p++)
    node->output_buffers[p] = NULL;
  feed_inputs(pipe, node, nid);
  if (node->desc && node->desc->ops && node->desc->ops->process)
    {
      int r = node->desc->ops->process(node);
      if (r != 0) return r;
    }
  pipe->pull_processed[to_idx] = 1;
  return 0;
}

/* 根据 link 将上游 output_buffers 拷贝到本节点 input_buffers */
static void feed_inputs(pipeline_t *pipe, media_node_t *node,
                       const char *node_id)
{
  for (int p = 0; p < node->num_input_ports; p++)
    node->input_buffers[p] = NULL;
  for (int i = 0; i < pipe->num_links; i++)
    {
      link_entry_t *e = &pipe->links[i];
      if (!e->active || strcmp(e->to_id, node_id) != 0) continue;
      int to_port = e->to_port;
      if (to_port < 0 || to_port >= node->num_input_ports) continue;
      int from_idx = -1;
      for (int j = 0; j < pipe->num_nodes; j++)
        if (strcmp(pipe->node_ids[j], e->from_id) == 0)
          {
            from_idx = j;
            break;
          }
      if (from_idx < 0) continue;
      media_node_t *from_node = pipe->nodes[from_idx];
      if (e->from_port < 0 ||
          e->from_port >= from_node->num_output_ports) continue;
      node->input_buffers[to_port] = from_node->output_buffers[e->from_port];
    }
}

/* 设置 Push（源驱动）或 Pull（宿驱动）模式 */
void pipeline_set_mode(pipeline_t *pipe, pipeline_mode_t mode)
{
  if (pipe) pipe->mode = mode;
}

pipeline_mode_t pipeline_get_mode(const pipeline_t *pipe)
{
  return pipe ? pipe->mode : PIPELINE_MODE_PUSH;
}

/* 停止主循环（设置 running=0，线程会退出） */
void pipeline_stop(pipeline_t *pipe)
{
  if (pipe) pipe->running = 0;
}

/* 查询流水线是否正在运行 */
int pipeline_is_running(const pipeline_t *pipe)
{
  return pipe && pipe->running;
}

pipeline_id_t pipeline_get_id(const pipeline_t *pipe)
{
  return pipe ? pipe->id : 0;
}

/* 设置混音目标名：多 pipeline 共享同一 mix_target 时 session 会合并 */
void pipeline_set_mix_target(pipeline_t *pipe, const char *mix_target)
{
  if (!pipe) return;
  if (mix_target)
    {
      strncpy(pipe->mix_target, mix_target, sizeof(pipe->mix_target) - 1);
      pipe->mix_target[sizeof(pipe->mix_target) - 1] = '\0';
    }
  else
    {
      pipe->mix_target[0] = '\0';
    }
}

const char *pipeline_get_mix_target(const pipeline_t *pipe)
{
  return (pipe && pipe->mix_target[0]) ? pipe->mix_target : NULL;
}

/* Collect link upstream output caps (incl. frame_ms) */
static void get_upstream_output_caps(const pipeline_t *pipe,
                                      const char *from_id, int from_port,
                                      media_caps_t *out_caps)
{
  memset(out_caps, 0, sizeof(*out_caps));
  int idx = find_node_index(pipe, from_id);
  if (idx < 0) return;
  media_node_t *node = pipe->nodes[idx];
  if (!node || !node->desc || !node->desc->ops ||
      !node->desc->ops->get_caps) return;
  node->desc->ops->get_caps(node, from_port, out_caps);
}

/* prepare：能力协商，依次检查 sample_rate/frame_ms/format，不匹配时自动插入
   resampler/frame_adapter/format_converter。需事先注册对应节点类型。 */
int pipeline_prepare(pipeline_t *pipe)
{
  if (!pipe || pipe->running) return -1;

  /* 默认 base_tick_ms=10ms，后续根据最小 frame_ms 调整 */
  pipe->base_tick_ms = 10;
  for (int i = 0; i < pipe->num_nodes; i++)
    pipe->node_tick_period[i] = 1;

  /* 第一遍：采样率不匹配时在 link 间插入 filter_resampler */
  int num_links = pipe->num_links;
  for (int i = 0; i < num_links; i++)
    {
      link_entry_t *e = &pipe->links[i];
      if (!e->active) continue;

      media_caps_t up_caps;
      get_upstream_output_caps(pipe, e->from_id, e->from_port, &up_caps);

      int to_idx = find_node_index(pipe, e->to_id);
      if (to_idx < 0) continue;
      media_node_t *to_node = pipe->nodes[to_idx];
      if (to_node->desc &&
          strcmp(to_node->desc->name, "filter_resampler") == 0)
        continue; /* resampler 接受任意输入，不检查 */
      uint32_t req_rate = pipe->node_configs[to_idx].sample_rate;

      if (req_rate != 0 && up_caps.sample_rate != 0 &&
          up_caps.sample_rate != req_rate)
        {
          if (pipe->num_nodes >= MAX_NODES) return -1;
          if (pipe->num_links >= MAX_LINKS - 1) return -1;
          /* 创建 resampler 节点，替换原 link 为 from->resampler->to */
          char resamp_id[64];
          snprintf(resamp_id, sizeof(resamp_id), "_resampler_%d", i);
          node_config_t resamp_cfg = {0};
          resamp_cfg.sample_rate = req_rate;

          media_node_t *resamp = factory_create_node("filter_resampler",
                                                     resamp_id, &resamp_cfg);
          if (!resamp)
            {
              fprintf(stderr,
                      "[pipeline_prepare] filter_resampler 未注册，跳过\n");
            }
          else
            {
              if (resamp->desc && resamp->desc->ops &&
                  resamp->desc->ops->set_caps)
                {
                  media_caps_t in_caps = up_caps;
                  resamp->desc->ops->set_caps(resamp, 0, &in_caps);
                }
              resamp->pipeline = pipe;
              pipe->nodes[pipe->num_nodes] = resamp;
              pipe->node_ids[pipe->num_nodes] = strdup(resamp_id);
              pipe->node_configs[pipe->num_nodes] = resamp_cfg;
              pipe->node_tick_period[pipe->num_nodes] = 1;
              pipe->num_nodes++;

              pipeline_remove_link(pipe, e->from_id, e->from_port,
                                  e->to_id, e->to_port);
              pipeline_link(pipe, e->from_id, e->from_port, resamp_id, 0);
              pipeline_link(pipe, resamp_id, 0, e->to_id, e->to_port);
              num_links = pipe->num_links;
            }
        }
    }

  /* 第二遍：frame_ms 不匹配时插入 filter_frame_adapter（上游必须能整除下游） */
  num_links = pipe->num_links;
  for (int i = 0; i < num_links; i++)
    {
      link_entry_t *e = &pipe->links[i];
      if (!e->active) continue;

      media_caps_t up_caps;
      get_upstream_output_caps(pipe, e->from_id, e->from_port, &up_caps);

      int to_idx = find_node_index(pipe, e->to_id);
      if (to_idx < 0) continue;
      media_node_t *to_node = pipe->nodes[to_idx];
      if (to_node->desc &&
          strcmp(to_node->desc->name, "filter_frame_adapter") == 0)
        continue; /* adapter 接受任意输入，不检查 */
      uint32_t req_frame = pipe->node_configs[to_idx].frame_ms;

      if (req_frame != 0 && up_caps.frame_ms != 0 &&
          up_caps.frame_ms != req_frame)
        {
          /* 上游 frame_ms 必须能整除 req_frame，否则无法适配 */
          if (up_caps.frame_ms % req_frame != 0)
            {
              fprintf(stderr,
                      "[pipeline_prepare] frame_ms %u -> %u 不整除，跳过\n",
                      up_caps.frame_ms, req_frame);
              continue;
            }
          if (pipe->num_nodes >= MAX_NODES) return -1;
          if (pipe->num_links >= MAX_LINKS - 1) return -1;

          char adapter_id[64];
          snprintf(adapter_id, sizeof(adapter_id), "_frame_adapter_%d", i);
          node_config_t adapter_cfg = {0};
          adapter_cfg.frame_ms = req_frame;
          adapter_cfg.sample_rate = up_caps.sample_rate;
          adapter_cfg.channels = up_caps.channels;

          media_node_t *adapter = factory_create_node("filter_frame_adapter",
                                                     adapter_id, &adapter_cfg);
          if (!adapter)
            {
              fprintf(stderr,
                      "[pipeline_prepare] filter_frame_adapter 未注册，跳过\n");
              continue;
            }
          if (adapter->desc && adapter->desc->ops &&
              adapter->desc->ops->set_caps)
            {
              media_caps_t in_caps = up_caps;
              in_caps.frame_ms = up_caps.frame_ms;
              adapter->desc->ops->set_caps(adapter, 0, &in_caps);
            }

          adapter->pipeline = pipe;
          pipe->nodes[pipe->num_nodes] = adapter;
          pipe->node_ids[pipe->num_nodes] = strdup(adapter_id);
          pipe->node_configs[pipe->num_nodes] = adapter_cfg;
          pipe->node_tick_period[pipe->num_nodes] = 1;
          int adapter_idx = pipe->num_nodes++;
          (void)adapter_idx;

          pipeline_remove_link(pipe, e->from_id, e->from_port,
                              e->to_id, e->to_port);
          pipeline_link(pipe, e->from_id, e->from_port, adapter_id, 0);
          pipeline_link(pipe, adapter_id, 0, e->to_id, e->to_port);
          num_links = pipe->num_links;
        }
    }

  /* 第三遍：格式不匹配时插入 filter_format_converter */
  num_links = pipe->num_links;
  for (int i = 0; i < num_links; i++)
    {
      link_entry_t *e = &pipe->links[i];
      if (!e->active) continue;

      media_caps_t up_caps;
      get_upstream_output_caps(pipe, e->from_id, e->from_port, &up_caps);

      int to_idx = find_node_index(pipe, e->to_id);
      if (to_idx < 0) continue;
      media_node_t *to_node = pipe->nodes[to_idx];
      if (to_node->desc &&
          strcmp(to_node->desc->name, "filter_format_converter") == 0)
        continue; /* 已是 converter，不检查 */
      uint32_t req_format = pipe->node_configs[to_idx].format;

      if (req_format != 0 && up_caps.format != MEDIA_FORMAT_UNKNOWN &&
          (media_format_t)req_format != up_caps.format)
        {
          if (pipe->num_nodes >= MAX_NODES) return -1;
          if (pipe->num_links >= MAX_LINKS - 1) return -1;

          char conv_id[64];
          snprintf(conv_id, sizeof(conv_id), "_format_converter_%d", i);
          node_config_t conv_cfg = {0};
          conv_cfg.format = req_format;
          conv_cfg.sample_rate = up_caps.sample_rate;
          conv_cfg.channels = up_caps.channels;

          media_node_t *conv = factory_create_node("filter_format_converter",
                                                  conv_id, &conv_cfg);
          if (!conv)
            {
              fprintf(stderr,
                      "[pipeline_prepare] filter_format_converter 未注册\n");
              continue;
            }
          if (conv->desc && conv->desc->ops && conv->desc->ops->set_caps)
            {
              media_caps_t in_caps = up_caps;
              conv->desc->ops->set_caps(conv, 0, &in_caps);
            }

          conv->pipeline = pipe;
          pipe->nodes[pipe->num_nodes] = conv;
          pipe->node_ids[pipe->num_nodes] = strdup(conv_id);
          pipe->node_configs[pipe->num_nodes] = conv_cfg;
          pipe->node_tick_period[pipe->num_nodes] = 1;
          pipe->num_nodes++;

          pipeline_remove_link(pipe, e->from_id, e->from_port,
                              e->to_id, e->to_port);
          pipeline_link(pipe, e->from_id, e->from_port, conv_id, 0);
          pipeline_link(pipe, conv_id, 0, e->to_id, e->to_port);
          num_links = pipe->num_links;
        }
    }

  /* 计算 base_tick_ms：取所有输出 frame_ms 中的最小值 */
  uint32_t min_frame = 0;
  for (int i = 0; i < pipe->num_nodes; i++)
    {
      media_caps_t caps;
      memset(&caps, 0, sizeof(caps));
      media_node_t *node = pipe->nodes[i];
      if (node->desc && node->desc->ops && node->desc->ops->get_caps)
        {
          for (int p = 0; p < node->num_output_ports; p++)
            {
              if (node->desc->ops->get_caps(node, p, &caps) == 0 &&
                  caps.frame_ms != 0)
                {
                  if (min_frame == 0 || caps.frame_ms < min_frame)
                    min_frame = caps.frame_ms;
                }
            }
        }
    }
  if (min_frame != 0) pipe->base_tick_ms = min_frame;

  /* 计算 node_tick_period：该节点每 period 个 tick 运行一次（如 20ms 时 period=2） */
  for (int i = 0; i < pipe->num_nodes; i++)
    {
      media_caps_t caps;
      memset(&caps, 0, sizeof(caps));
      media_node_t *node = pipe->nodes[i];
      if (node->desc && node->desc->ops && node->desc->ops->get_caps)
        {
          for (int p = 0; p < node->num_output_ports; p++)
            {
              if (node->desc->ops->get_caps(node, p, &caps) == 0 &&
                  caps.frame_ms != 0)
                {
                  int period = (int)(caps.frame_ms / pipe->base_tick_ms);
                  if (period < 1) period = 1;
                  pipe->node_tick_period[i] = period;
                  break;
                }
            }
        }
    }
  return 0;
}

/* 启动流水线：prepare、topo 排序、init 各节点、进入主循环（阻塞） */
int pipeline_start(pipeline_t *pipe)
{
  if (!pipe || pipe->running || pipe->num_nodes == 0) return -1;
  pipeline_prepare(pipe);
  int order[MAX_NODES];
  int n = topo_order(pipe, order);
  if (n != pipe->num_nodes) return -1; /* 有环则 topo 失败 */

  if (media_debug_enabled())
    {
      fprintf(stderr, "[pipeline] nodes=%d topo=", pipe->num_nodes);
      for (int i = 0; i < n; i++)
        fprintf(stderr, "%s%s", pipe->node_ids[order[i]],
                i < n - 1 ? "->" : "");
      fprintf(stderr, "\n");
    }

  /* 初始化所有节点 */
  for (int i = 0; i < pipe->num_nodes; i++)
    {
      media_node_t *node = pipe->nodes[i];
      if (node->desc && node->desc->ops && node->desc->ops->init)
        {
          int r = node->desc->ops->init(node, &pipe->node_configs[i]);
          if (r != 0)
            {
              if (media_debug_enabled())
                fprintf(stderr, "[pipeline] init failed: %s\n",
                        pipe->node_ids[i]);
              return r;
            }
        }
    }

  if (media_debug_enabled())
    fprintf(stderr, "[pipeline] all nodes inited, starting loop\n");
  pipe->running = 1;
  int tick = 0;

  /* Pull 模式：每轮从各 sink 递归拉取，拉取完后释放 output_buffers */
  if (pipe->mode == PIPELINE_MODE_PULL)
    {
      while (pipe->running)
        {
          memset(pipe->pull_processed, 0, sizeof(pipe->pull_processed));
          for (int i = 0; i < pipe->num_nodes; i++)
            {
              if (!pipe->running) break;
              if (!is_sink_node(pipe, i)) continue;
              int r = pull_chain(pipe, i);
              if (r != 0)
                {
                  pipe->running = 0;
                  return r;
                }
            }
          for (int i = 0; i < pipe->num_nodes; i++)
            {
              media_node_t *node = pipe->nodes[i];
              for (int p = 0; p < node->num_output_ports; p++)
                {
                  if (node->output_buffers[p])
                    {
                      media_buffer_free(node->output_buffers[p]);
                      node->output_buffers[p] = NULL;
                    }
                }
            }
        }
    }
  /* Push 模式：按 topo 顺序推进，tick 控制不同 frame_ms 节点的运行频率 */
  else
    {
      while (pipe->running)
        {
          for (int i = 0; i < n; i++)
            {
              if (!pipe->running) break;
              int idx = order[i];
              int period = pipe->node_tick_period[idx];
              if (period > 0 && (tick % period) != 0)
                continue; /* 本 tick 该节点不运行（如 period=2 时奇数 tick 跳过） */
              media_node_t *node = pipe->nodes[idx];
              const char *nid = pipe->node_ids[idx];
              for (int p = 0; p < node->num_output_ports; p++)
                node->output_buffers[p] = NULL;
              feed_inputs(pipe, node, nid);
              if (node->desc && node->desc->ops &&
                  node->desc->ops->process)
                {
                  int r = node->desc->ops->process(node);
                  if (r != 0)
                    {
                      if (media_debug_enabled())
                        fprintf(stderr,
                                "[pipeline] tick=%d node=%s ret=%d\n",
                                tick, nid, r);
                      pipe->running = 0;
                      return r;
                    }
                }
            }
          if (media_debug_enabled() && tick > 0 && tick % 100 == 0)
            fprintf(stderr, "[pipeline] tick=%d\n", tick);
          tick++;
          /* 每 tick 结束后释放所有 output_buffers */
          for (int i = 0; i < pipe->num_nodes; i++)
            {
              media_node_t *node = pipe->nodes[i];
              for (int p = 0; p < node->num_output_ports; p++)
                {
                  if (node->output_buffers[p])
                    {
                      media_buffer_free(node->output_buffers[p]);
                      node->output_buffers[p] = NULL;
                    }
                }
            }
        }
    }
  return 0;
}

/* 设置指定节点的音量（需节点支持 set_volume） */
int pipeline_set_node_volume(pipeline_t *pipe, const char *node_id, float gain)
{
  if (!pipe || !node_id) return -1;
  int idx = find_node_index(pipe, node_id);
  if (idx < 0) return -1;
  media_node_t *node = pipe->nodes[idx];
  if (!node || !node->desc || !node->desc->ops ||
      !node->desc->ops->set_volume)
    return -1;
  return node->desc->ops->set_volume(node, gain);
}

/* 销毁流水线：先 stop，flush 各节点，释放所有节点和 link */
void pipeline_destroy(pipeline_t *pipe)
{
  if (!pipe) return;
  pipeline_stop(pipe);
  for (int i = 0; i < pipe->num_nodes; i++)
    {
      if (pipe->nodes[i] && pipe->nodes[i]->desc &&
          pipe->nodes[i]->desc->ops && pipe->nodes[i]->desc->ops->flush)
        pipe->nodes[i]->desc->ops->flush(pipe->nodes[i]);
      factory_destroy_node(pipe->nodes[i]);
      pipe->nodes[i] = NULL;
      free(pipe->node_ids[i]);
      pipe->node_ids[i] = NULL;
    }
  pipe->num_nodes = 0;
  pipe->num_links = 0;
  free(pipe);
}
