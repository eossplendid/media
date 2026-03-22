/****************************************************************************
 * src/media_core/audio_focus.c
 *
 * Audio focus stack: GAIN/TRANSIENT/DUCKABLE.
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

#define MAX_FOCUS_STACK 8

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
