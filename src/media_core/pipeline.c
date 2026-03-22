/****************************************************************************
 * src/media_core/pipeline.c
 *
 * Pipeline create/add_node/start/stop/destroy and process loop.
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

static int find_node_index(const pipeline_t *pipe, const char *node_id)
{
  for (int i = 0; i < pipe->num_nodes; i++)
    if (pipe->node_ids[i] && strcmp(pipe->node_ids[i], node_id) == 0)
      return i;
  return -1;
}

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

/* Topological order: nodes with no incoming links first, then by dependency */
static int topo_order(const pipeline_t *pipe, int *order)
{
  int in_degree[MAX_NODES];
  memset(in_degree, 0, sizeof(in_degree));
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

/* Copy upstream output buffers to this node's input buffers per links */
static void feed_inputs(pipeline_t *pipe, media_node_t *node,
                        const char *node_id);

/* Sink node: no outgoing links */
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

/* Pull mode: recursive upstream pull, ensure upstream produce before process */
static int pull_chain(pipeline_t *pipe, int to_idx)
{
  if (pipe->pull_processed[to_idx]) return 0;
  media_node_t *node = pipe->nodes[to_idx];
  const char *nid = pipe->node_ids[to_idx];
  for (int i = 0; i < pipe->num_links; i++)
    {
      link_entry_t *e = &pipe->links[i];
      if (!e->active || strcmp(e->to_id, nid) != 0) continue;
      int from_idx = find_node_index(pipe, e->from_id);
      if (from_idx >= 0)
        {
          int r = pull_chain(pipe, from_idx);
          if (r != 0) return r;
        }
    }
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

void pipeline_set_mode(pipeline_t *pipe, pipeline_mode_t mode)
{
  if (pipe) pipe->mode = mode;
}

pipeline_mode_t pipeline_get_mode(const pipeline_t *pipe)
{
  return pipe ? pipe->mode : PIPELINE_MODE_PUSH;
}

void pipeline_stop(pipeline_t *pipe)
{
  if (pipe) pipe->running = 0;
}

int pipeline_is_running(const pipeline_t *pipe)
{
  return pipe && pipe->running;
}

pipeline_id_t pipeline_get_id(const pipeline_t *pipe)
{
  return pipe ? pipe->id : 0;
}

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

/* prepare: caps negotiation, auto-insert filter_frame_adapter when frame_ms
   mismatches. Requires "filter_frame_adapter" node type registered. */
int pipeline_prepare(pipeline_t *pipe)
{
  if (!pipe || pipe->running) return -1;

  pipe->base_tick_ms = 10;
  for (int i = 0; i < pipe->num_nodes; i++)
    pipe->node_tick_period[i] = 1;

  /* First pass: insert filter_resampler when sample_rate mismatch */
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
        continue;
      uint32_t req_rate = pipe->node_configs[to_idx].sample_rate;

      if (req_rate != 0 && up_caps.sample_rate != 0 &&
          up_caps.sample_rate != req_rate)
        {
          if (pipe->num_nodes >= MAX_NODES) return -1;
          if (pipe->num_links >= MAX_LINKS - 1) return -1;

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

  /* Second pass: insert filter_frame_adapter when frame_ms mismatch */
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
        continue;
      uint32_t req_frame = pipe->node_configs[to_idx].frame_ms;

      if (req_frame != 0 && up_caps.frame_ms != 0 &&
          up_caps.frame_ms != req_frame)
        {
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

  /* Third pass: insert filter_format_converter when format mismatch */
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
        continue;
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

  /* Compute base_tick_ms and node_tick_period */
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

int pipeline_start(pipeline_t *pipe)
{
  if (!pipe || pipe->running || pipe->num_nodes == 0) return -1;
  pipeline_prepare(pipe);
  int order[MAX_NODES];
  int n = topo_order(pipe, order);
  if (n != pipe->num_nodes) return -1;

  if (media_debug_enabled())
    {
      fprintf(stderr, "[pipeline] nodes=%d topo=", pipe->num_nodes);
      for (int i = 0; i < n; i++)
        fprintf(stderr, "%s%s", pipe->node_ids[order[i]],
                i < n - 1 ? "->" : "");
      fprintf(stderr, "\n");
    }

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
                continue;
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
