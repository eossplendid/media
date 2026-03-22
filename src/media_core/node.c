/****************************************************************************
 * src/media_core/node.c
 *
 * Description
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
#include "../../include/media_core/node.h"
#include "../../include/media_types.h"
#include <stdlib.h>
#include <string.h>

/* 分配 media_buffer：内部 malloc data，带 release 回调可自定义释放逻辑 */
media_buffer_t *media_buffer_alloc(size_t size)
{
 media_buffer_t *buf = (media_buffer_t *)calloc(1, sizeof(media_buffer_t));
 if (!buf) return NULL;
 buf->data = (uint8_t *)malloc(size);
 if (!buf->data)
  {
   free(buf);
   return NULL;
  }
 buf->size = size;
 buf->release = NULL;
 return buf;
}

/* 释放 media_buffer：若有 release 回调则调用，否则 free(data) */
void media_buffer_free(media_buffer_t *buf)
{
 if (!buf) return;
 if (buf->release)
  buf->release(buf);
 else
  {
   free(buf->data);
   buf->data = NULL;
   buf->size = 0;
  }
 free(buf);
}

/* 根据 sample_rate/channels/format 填充 caps，并计算 bytes_per_sample */
void media_caps_from_format(media_caps_t *caps, uint32_t sample_rate,
              uint32_t channels, media_format_t format)
{
 if (!caps) return;
 caps->sample_rate = sample_rate;
 caps->channels = channels;
 caps->format = format;
 caps->bytes_per_sample = media_format_bytes(format) * channels;
}
