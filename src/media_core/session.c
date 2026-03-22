/****************************************************************************
 * src/media_core/session.c
 *
 * Multi-pipeline management and per-pipeline thread.
 *
 * 本模块管理多个流水线及各自的运行线程：
 *   - 每个 pipeline 在独立线程中运行 pipeline_start
 *   - 相同 mix_target 的 pipeline 会合并为 mixer->sink 拓扑
 *   - session_start_pipeline 启动前检查 mix_target 冲突并自动合并
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

#include "../../include/media_core/session.h"
#include "../../include/media_core/pipeline.h"
#include "../../include/media_core/factory.h"
#include "../../include/media_core/link.h"
#include "pipeline_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define MAX_PIPELINES 16  /* 单 session 最大 pipeline 数 */

struct session
{
  pipeline_t *pipelines[MAX_PIPELINES];
  int num_pipelines;
  pthread_t threads[MAX_PIPELINES];
  int thread_used[MAX_PIPELINES];  /* 对应 slot 是否已创建线程 */
};

/* 线程入口：阻塞执行 pipeline_start */
static void *pipeline_thread_fn(void *arg)
{
  pipeline_t *pipe = (pipeline_t *)arg;
  pipeline_start(pipe);
  return NULL;
}

/* 创建 session 实例 */
session_t *session_create(void)
{
  session_t *s = (session_t *)calloc(1, sizeof(session_t));
  return s;
}

/* 将 pipeline 加入 session（通常由 pipeline_create 内部调用） */
void session_attach(session_t *session, pipeline_t *pipe)
{
  if (!session || !pipe || session->num_pipelines >= MAX_PIPELINES) return;
  session->pipelines[session->num_pipelines++] = pipe;
}

/* 按 id 查找 pipeline */
pipeline_t *session_get_pipeline(session_t *session, pipeline_id_t id)
{
  if (!session) return NULL;
  for (int i = 0; i < session->num_pipelines; i++)
    if (session->pipelines[i] && pipeline_get_id(session->pipelines[i]) == id)
      return session->pipelines[i];
  return NULL;
}

pipeline_id_t session_get_pipeline_by_mix_target(session_t *session,
                                                  const char *mix_target)
{
  if (!session || !mix_target || !mix_target[0]) return 0;
  for (int i = 0; i < session->num_pipelines; i++)
    {
      pipeline_t *p = session->pipelines[i];
      if (!p) continue;
      const char *t = pipeline_get_mix_target(p);
      if (t && strcmp(t, mix_target) == 0)
        return pipeline_get_id(p);
    }
  return 0;
}

/* 合并两个同 mix_target 的 pipeline：提取源节点，构建 a_ 与 b_ 前缀节点
   -> mixer -> sink；中间节点（resampler 等）保留在各源链中 */
static int merge_pipelines_for_mix(session_t *session, pipeline_t *pipe_old,
                                   pipeline_t *pipe_new)
{
  pipeline_id_t keep_id = pipeline_get_id(pipe_old);
  const char *mt = pipeline_get_mix_target(pipe_old);

  /* 从 pipe_old 中找 sink 节点（无出边）及其配置 */

  int sink_idx = -1;
  node_config_t spk_cfg = { .sample_rate = 48000, .channels = 1 };
  for (int i = 0; i < pipe_old->num_nodes; i++)
    {
      int has_out = 0;
      for (int j = 0; j < pipe_old->num_links; j++)
        {
          if (!pipe_old->links[j].active) continue;
          if (strcmp(pipe_old->links[j].from_id, pipe_old->node_ids[i]) == 0)
            {
              has_out = 1;
              break;
            }
        }
      if (!has_out)
        {
          sink_idx = i;
          spk_cfg = pipe_old->node_configs[i];
          break;
        }
    }

  /* 收集两个 pipeline 中直接连到 sink 的 feeder 节点 id */

  char *feeders_old[MAX_NODES];
  int n_old = 0;
  char *feeders_new[MAX_NODES];
  int n_new = 0;
  const char *sink_id_old = sink_idx >= 0 ? pipe_old->node_ids[sink_idx] : NULL;
  const char *sink_id_new = NULL;
  for (int i = 0; i < pipe_new->num_nodes; i++)
    {
      int has_out = 0;
      for (int j = 0; j < pipe_new->num_links; j++)
        {
          if (!pipe_new->links[j].active) continue;
          if (strcmp(pipe_new->links[j].from_id, pipe_new->node_ids[i]) == 0)
            {
              has_out = 1;
              break;
            }
        }
      if (!has_out)
        {
          sink_id_new = pipe_new->node_ids[i];
          break;
        }
    }
  for (int i = 0; i < pipe_old->num_links; i++)
    {
      if (!pipe_old->links[i].active) continue;
      if (sink_id_old && strcmp(pipe_old->links[i].to_id, sink_id_old) == 0)
        feeders_old[n_old++] = pipe_old->links[i].from_id;
    }
  for (int i = 0; i < pipe_new->num_links; i++)
    {
      if (!pipe_new->links[i].active) continue;
      if (sink_id_new && strcmp(pipe_new->links[i].to_id, sink_id_new) == 0)
        feeders_new[n_new++] = pipe_new->links[i].from_id;
    }
  if (n_old == 0) feeders_old[n_old++] = (char *)pipe_old->node_ids[0];
  if (n_new == 0) feeders_new[n_new++] = (char *)pipe_new->node_ids[0];

  pipeline_t *merged = pipeline_create(session, keep_id);
  if (!merged) return -1;
  pipeline_set_mix_target(merged, mt);

  /* Add all nodes from pipe_old with a_ prefix (except sink) */

  for (int i = 0; i < pipe_old->num_nodes; i++)
    {
      media_node_t *n = pipe_old->nodes[i];
      if (!n || !n->desc) continue;
      if (sink_id_old && strcmp(pipe_old->node_ids[i], sink_id_old) == 0)
        continue;
      char aid[80];
      snprintf(aid, sizeof(aid), "a_%s", pipe_old->node_ids[i]);
      pipeline_add_node(merged, n->desc->name, aid,
                        &pipe_old->node_configs[i]);
    }
  for (int i = 0; i < pipe_old->num_links; i++)
    {
      link_entry_t *e = &pipe_old->links[i];
      if (!e->active) continue;
      if (sink_id_old && strcmp(e->to_id, sink_id_old) == 0) continue;
      char from[80];
      char to[80];
      snprintf(from, sizeof(from), "a_%s", e->from_id);
      snprintf(to, sizeof(to), "a_%s", e->to_id);
      pipeline_link(merged, from, e->from_port, to, e->to_port);
    }

  /* Add all nodes from pipe_new with b_ prefix (except sink) */

  for (int i = 0; i < pipe_new->num_nodes; i++)
    {
      media_node_t *n = pipe_new->nodes[i];
      if (!n || !n->desc) continue;
      if (sink_id_new && strcmp(pipe_new->node_ids[i], sink_id_new) == 0)
        continue;
      char bid[80];
      snprintf(bid, sizeof(bid), "b_%s", pipe_new->node_ids[i]);
      pipeline_add_node(merged, n->desc->name, bid,
                        &pipe_new->node_configs[i]);
    }
  for (int i = 0; i < pipe_new->num_links; i++)
    {
      link_entry_t *e = &pipe_new->links[i];
      if (!e->active) continue;
      if (sink_id_new && strcmp(e->to_id, sink_id_new) == 0) continue;
      char from[80];
      char to[80];
      snprintf(from, sizeof(from), "b_%s", e->from_id);
      snprintf(to, sizeof(to), "b_%s", e->to_id);
      pipeline_link(merged, from, e->from_port, to, e->to_port);
    }

  /* Add mixer and sink, link feeders to mixer */

  int mix_inputs = n_old + n_new;
  node_config_t mix_cfg = { .input_count = (uint32_t)mix_inputs,
                            .sample_rate = 48000 };
  pipeline_add_node(merged, "filter_mixer", "mix", &mix_cfg);
  pipeline_add_node(merged, "sink_speaker", "spk", &spk_cfg);

  int port = 0;
  for (int i = 0; i < n_old; i++)
    {
      char from[80];
      snprintf(from, sizeof(from), "a_%s", feeders_old[i]);
      pipeline_link(merged, from, 0, "mix", port++);
    }
  for (int i = 0; i < n_new; i++)
    {
      char from[80];
      snprintf(from, sizeof(from), "b_%s", feeders_new[i]);
      pipeline_link(merged, from, 0, "mix", port++);
    }
  pipeline_link(merged, "mix", 0, "spk", 0);

  session_detach(session, pipe_old);
  session_detach(session, pipe_new);
  pipeline_destroy(pipe_old);
  pipeline_destroy(pipe_new);
  session_attach(session, merged);
  return session_start_pipeline(session, keep_id);
}

/* 启动 pipeline：若 mix_target 冲突则先合并，再创建线程执行 pipeline_start */
int session_start_pipeline(session_t *session, pipeline_id_t id)
{
  pipeline_t *pipe = session_get_pipeline(session, id);
  if (!pipe || pipeline_is_running(pipe)) return -1;

  const char *mt = pipeline_get_mix_target(pipe);
  if (mt && mt[0])
    {
      pipeline_id_t existing = session_get_pipeline_by_mix_target(session, mt);
      if (existing != 0 && existing != id)
        {
          pipeline_t *old = session_get_pipeline(session, existing);
          if (old)
            {
              int r = merge_pipelines_for_mix(session, old, pipe);
              if (r == 0) return r;
            }
        }
    }

  int slot = -1;
  for (int i = 0; i < session->num_pipelines; i++)
    if (session->pipelines[i] == pipe)
      {
        slot = i;
        break;
      }
  if (slot < 0) return -1;
  int r = pthread_create(&session->threads[slot], NULL, pipeline_thread_fn,
                         pipe);
  session->thread_used[slot] = (r == 0);
  return r == 0 ? 0 : -1;
}

/* 停止指定 pipeline（设置 running=0，线程会退出） */
void session_stop_pipeline(session_t *session, pipeline_id_t id)
{
  pipeline_t *pipe = session_get_pipeline(session, id);
  if (pipe) pipeline_stop(pipe);
}

/* 从 session 分离 pipeline：stop、join 线程、从数组移除 */
int session_detach(session_t *session, pipeline_t *pipe)
{
  if (!session || !pipe) return -1;
  int slot = -1;
  for (int i = 0; i < session->num_pipelines; i++)
    if (session->pipelines[i] == pipe)
      {
        slot = i;
        break;
      }
  if (slot < 0) return -1;
  pipeline_stop(pipe);
  if (session->thread_used[slot])
    pthread_join(session->threads[slot], NULL);
  session->thread_used[slot] = 0;
  for (int i = slot; i < session->num_pipelines - 1; i++)
    {
      session->pipelines[i] = session->pipelines[i + 1];
      session->threads[i] = session->threads[i + 1];
      session->thread_used[i] = session->thread_used[i + 1];
    }
  session->pipelines[session->num_pipelines - 1] = NULL;
  session->num_pipelines--;
  return 0;
}

/* 销毁 session：stop 并 join 所有线程，销毁所有 pipeline */
void session_destroy(session_t *session)
{
  if (!session) return;
  for (int i = 0; i < session->num_pipelines; i++)
    {
      if (session->pipelines[i])
        pipeline_stop(session->pipelines[i]);
    }
  for (int i = 0; i < session->num_pipelines; i++)
    {
      if (session->thread_used[i])
        pthread_join(session->threads[i], NULL);
    }
  for (int i = 0; i < session->num_pipelines; i++)
    {
      if (session->pipelines[i])
        {
          pipeline_destroy(session->pipelines[i]);
          session->pipelines[i] = NULL;
        }
    }
  session->num_pipelines = 0;
  free(session);
}
