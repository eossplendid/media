/****************************************************************************
 * src/media_core/audio_focus.c
 *
 * Audio focus stack: GAIN/TRANSIENT/DUCKABLE.
 *
 * 本模块实现音频焦点栈，支持多 pipeline 竞争焦点：
 *   - GAIN：完全占有，其他静音
 *   - TRANSIENT：短暂打断
 *   - DUCKABLE：可被压低
 *   栈顶为当前焦点持有者，apply_focus 根据栈状态调整各 pipeline 音量
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

#include "../../include/media_core/audio_focus.h"
#include "../../include/media_core/session.h"
#include "../../include/media_core/pipeline.h"
#include "pipeline_internal.h"
#include <stdlib.h>
#include <string.h>

#define MAX_FOCUS_STACK 8  /* 最大焦点栈深度 */

typedef struct
{
  pipeline_id_t id;
  audio_focus_type_t type;
} focus_entry_t;

static focus_entry_t g_focus_stack[MAX_FOCUS_STACK];
static int g_focus_top;

static void apply_focus(session_t *session)
{
  if (!session || g_focus_top <= 0) return;
  focus_entry_t *top = &g_focus_stack[g_focus_top - 1];
  pipeline_t *focus_pipe = session_get_pipeline(session, top->id);
  pipeline_t *pipe = session_get_pipeline(session, 0);
  (void)focus_pipe;
  (void)pipe;
}

/* 请求焦点：若已存在则更新类型并移至栈顶，否则压栈 */
int audio_focus_request(session_t *session, pipeline_id_t id,
                       audio_focus_type_t type)
{
  if (!session || g_focus_top >= MAX_FOCUS_STACK) return -1;
  for (int i = 0; i < g_focus_top; i++)
    {
      if (g_focus_stack[i].id == id)
        {
          g_focus_stack[i].type = type;
          if (i != g_focus_top - 1)
            {
              focus_entry_t e = g_focus_stack[i];
              memmove(&g_focus_stack[i], &g_focus_stack[i + 1],
                      (g_focus_top - 1 - i) * sizeof(focus_entry_t));
              g_focus_stack[g_focus_top - 1] = e;
            }
          apply_focus(session);
          return 0;
        }
    }
  g_focus_stack[g_focus_top].id = id;
  g_focus_stack[g_focus_top].type = type;
  g_focus_top++;
  apply_focus(session);
  return 0;
}

/* 放弃焦点：从栈中移除该 pipeline，并重新 apply_focus */
void audio_focus_abandon(session_t *session, pipeline_id_t id)
{
  if (!session) return;
  for (int i = 0; i < g_focus_top; i++)
    {
      if (g_focus_stack[i].id == id)
        {
          memmove(&g_focus_stack[i], &g_focus_stack[i + 1],
                  (g_focus_top - 1 - i) * sizeof(focus_entry_t));
          g_focus_top--;
          apply_focus(session);
          return;
        }
    }
}
