/****************************************************************************
 * src/media_core/hal_audio.c
 *
 * HAL audio registration and ops accessor.
 *
 * 本模块为平台相关 HAL 提供注册与获取接口：
 *   - hal_register_audio_in/out：由 hal_alsa 等实现注册
 *   - hal_get_audio_in/out_ops：节点（如 source_mic）获取 ops 调用
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

static const hal_audio_in_ops_t *g_audio_in_ops;   /* 录音 HAL 操作集 */
static const hal_audio_out_ops_t *g_audio_out_ops; /* 播放 HAL 操作集 */

/* 注册录音 HAL（open/read/close 等） */
void hal_register_audio_in(const hal_audio_in_ops_t *ops)
{
  g_audio_in_ops = ops;
}

/* 注册播放 HAL（open/write/close 等） */
void hal_register_audio_out(const hal_audio_out_ops_t *ops)
{
  g_audio_out_ops = ops;
}

/* 获取录音 HAL 操作集，未注册返回 NULL */
const hal_audio_in_ops_t *hal_get_audio_in_ops(void)
{
  return g_audio_in_ops;
}

/* 获取播放 HAL 操作集，未注册返回 NULL */
const hal_audio_out_ops_t *hal_get_audio_out_ops(void)
{
  return g_audio_out_ops;
}
