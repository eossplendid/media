/****************************************************************************
 * src/media_core/playback_group.c
 *
 * Dynamic multi-source playback group: auto-insert mixer when multiple src.
 *
 * 本模块实现动态多源播放组：
 *   - 单源：file -> speaker
 *   - 多源：file0..N -> mixer -> speaker
 *   增删源时自动 rebuild_pipeline，运行中可动态调整
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

#include "../../include/media_core/playback_group.h"
#include "../../include/media_core/session.h"
#include "../../include/media_core/pipeline.h"
#include "../../include/media_core/factory.h"
#include "../../include/media_core/link.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define MAX_SOURCES 8  /* 最大同时播放源数 */

struct playback_group
{
  session_t *session;
  pipeline_t *pipe;
  pipeline_id_t pipe_id;
  uint32_t output_sample_rate;
  uint32_t output_channels;
  char *sources[MAX_SOURCES];
  int num_sources;
  int running;
  pthread_mutex_t lock;  /* 保护 num_sources、sources、rebuild */
};

/* 根据当前 sources 重建 pipeline：单源直连，多源插入 mixer */
static void rebuild_pipeline(playback_group_t *pg)
{
  if (pg->pipe)
    {
      session_detach(pg->session, pg->pipe);
      pipeline_destroy(pg->pipe);
      pg->pipe = NULL;
    }
  if (pg->num_sources == 0) return;

  /* 创建新 pipeline 并按源数量构建拓扑 */
  pg->pipe = pipeline_create(pg->session, pg->pipe_id);
  if (!pg->pipe) return;

  uint32_t rate = pg->output_sample_rate ? pg->output_sample_rate : 48000;
  uint32_t ch = pg->output_channels ? pg->output_channels : 1;

  /* 单源：file0 -> spk；多源：file0..N -> mix -> spk */
  if (pg->num_sources == 1)
    {
      node_config_t file_cfg = { .path = pg->sources[0] };
      node_config_t spk_cfg = { .sample_rate = rate, .channels = ch };
      pipeline_add_node(pg->pipe, "source_file", "file0", &file_cfg);
      pipeline_add_node(pg->pipe, "sink_speaker", "spk", &spk_cfg);
      pipeline_link(pg->pipe, "file0", 0, "spk", 0);
    }
  else
    {
      node_config_t mix_cfg = { .input_count = (uint32_t)pg->num_sources,
                                .sample_rate = rate };
      node_config_t spk_cfg = { .sample_rate = rate, .channels = ch };
      for (int i = 0; i < pg->num_sources; i++)
        {
          char id[32];
          snprintf(id, sizeof(id), "file%d", i);
          node_config_t file_cfg = { .path = pg->sources[i] };
          pipeline_add_node(pg->pipe, "source_file", id, &file_cfg);
        }
      pipeline_add_node(pg->pipe, "filter_mixer", "mix", &mix_cfg);
      pipeline_add_node(pg->pipe, "sink_speaker", "spk", &spk_cfg);
      for (int i = 0; i < pg->num_sources; i++)
        {
          char id[32];
          snprintf(id, sizeof(id), "file%d", i);
          pipeline_link(pg->pipe, id, 0, "mix", i);
        }
      pipeline_link(pg->pipe, "mix", 0, "spk", 0);
    }
}

playback_group_t *playback_group_create(session_t *session,
                                        const playback_group_config_t *config)
{
  if (!session) return NULL;
  playback_group_t *pg = (playback_group_t *)calloc(1,
                                                    sizeof(playback_group_t));
  if (!pg) return NULL;
  pg->session = session;
  pg->pipe_id = 1;
  if (config)
    {
      pg->output_sample_rate = config->output_sample_rate;
      pg->output_channels = config->output_channels;
    }
  pthread_mutex_init(&pg->lock, NULL);
  return pg;
}

/* 销毁播放组：detach pipeline，释放所有 source 路径，销毁 mutex */
void playback_group_destroy(playback_group_t *pg)
{
  if (!pg) return;
  pthread_mutex_lock(&pg->lock);
  pg->running = 0;
  if (pg->pipe)
    {
      session_detach(pg->session, pg->pipe);
      pipeline_destroy(pg->pipe);
      pg->pipe = NULL;
    }
  for (int i = 0; i < pg->num_sources; i++)
    {
      free(pg->sources[i]);
      pg->sources[i] = NULL;
    }
  pg->num_sources = 0;
  pthread_mutex_unlock(&pg->lock);
  pthread_mutex_destroy(&pg->lock);
  free(pg);
}

/* 添加播放源路径，运行中会 rebuild 并 restart；返回 source 下标 */
int playback_group_add_source(playback_group_t *pg, const char *path)
{
  if (!pg || !path) return -1;
  pthread_mutex_lock(&pg->lock);
  if (pg->num_sources >= MAX_SOURCES)
    {
      pthread_mutex_unlock(&pg->lock);
      return -1;
    }
  pg->sources[pg->num_sources] = strdup(path);
  if (!pg->sources[pg->num_sources])
    {
      pthread_mutex_unlock(&pg->lock);
      return -1;
    }
  pg->num_sources++;
  int was_running = pg->running;
  rebuild_pipeline(pg);
  if (was_running && pg->pipe)
    session_start_pipeline(pg->session, pg->pipe_id);
  pthread_mutex_unlock(&pg->lock);
  return pg->num_sources - 1;
}

/* 移除指定 source，若原在运行则 rebuild 后自动 restart */
void playback_group_remove_source(playback_group_t *pg, int source_id)
{
  if (!pg || source_id < 0 || source_id >= pg->num_sources) return;
  pthread_mutex_lock(&pg->lock);
  free(pg->sources[source_id]);
  for (int i = source_id; i < pg->num_sources - 1; i++)
    pg->sources[i] = pg->sources[i + 1];
  pg->sources[pg->num_sources - 1] = NULL;
  pg->num_sources--;
  int was_running = pg->running;
  rebuild_pipeline(pg);
  if (was_running && pg->pipe)
    session_start_pipeline(pg->session, pg->pipe_id);
  pthread_mutex_unlock(&pg->lock);
}

/* 启动播放：要求已有源且 pipeline 已 build */
int playback_group_start(playback_group_t *pg)
{
  if (!pg) return -1;
  pthread_mutex_lock(&pg->lock);
  if (pg->num_sources == 0 || !pg->pipe)
    {
      pthread_mutex_unlock(&pg->lock);
      return -1;
    }
  pg->running = 1;
  int r = session_start_pipeline(pg->session, pg->pipe_id);
  pthread_mutex_unlock(&pg->lock);
  return r;
}

/* 停止播放（设置 running=0 并 stop pipeline） */
void playback_group_stop(playback_group_t *pg)
{
  if (!pg) return;
  pthread_mutex_lock(&pg->lock);
  pg->running = 0;
  session_stop_pipeline(pg->session, pg->pipe_id);
  pthread_mutex_unlock(&pg->lock);
}

/* 返回当前源数量 */
int playback_group_source_count(const playback_group_t *pg)
{
  return pg ? pg->num_sources : 0;
}
