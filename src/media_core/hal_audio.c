/****************************************************************************
 * src/media_core/hal_audio.c
 *
 * HAL audio registration and ops accessor.
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

#include "../../include/media_core/hal_audio.h"
#include <stddef.h>

static const hal_audio_in_ops_t *g_audio_in_ops;
static const hal_audio_out_ops_t *g_audio_out_ops;

void hal_register_audio_in(const hal_audio_in_ops_t *ops)
{
  g_audio_in_ops = ops;
}

void hal_register_audio_out(const hal_audio_out_ops_t *ops)
{
  g_audio_out_ops = ops;
}

const hal_audio_in_ops_t *hal_get_audio_in_ops(void)
{
  return g_audio_in_ops;
}

const hal_audio_out_ops_t *hal_get_audio_out_ops(void)
{
  return g_audio_out_ops;
}
