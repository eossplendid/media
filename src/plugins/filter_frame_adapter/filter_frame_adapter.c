/****************************************************************************
 * src/plugins/filter_frame_adapter/filter_frame_adapter.c
 *
 * 帧长适配节点 (1 in, 1 out)；将上游 N ms 帧拆分为下游 M ms 帧。
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
#include "media_core/node.h"
#include "media_core/factory.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct
{
  uint32_t sample_rate;
  uint32_t channels;
  uint32_t bytes_per_sample;
  uint32_t input_frame_ms;
  uint32_t output_frame_ms;
  size_t output_frame_bytes;
  uint8_t *buf;
  size_t buf_size;
  size_t buf_used;
} frame_adapter_priv_t;

static int frame_adapter_init(media_node_t *node, const node_config_t *config)
{
  frame_adapter_priv_t *p = (frame_adapter_priv_t *)node->private_data;
  if (!p) return -1;
  p->output_frame_ms = config && config->frame_ms ? config->frame_ms : 10;
  return 0;
}

static int frame_adapter_prepare(media_node_t *node, int port_index,
                const media_caps_t *upstream_caps, media_caps_t *out_caps)
                {
  (void)port_index;
  frame_adapter_priv_t *p = (frame_adapter_priv_t *)node->private_data;
  if (!p || !upstream_caps || !out_caps) return -1;
  *out_caps = *upstream_caps;
  out_caps->frame_ms = p->output_frame_ms;
  return 0;
}

static int frame_adapter_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps)
{
  frame_adapter_priv_t *p = (frame_adapter_priv_t *)node->private_data;
  if (!p || !out_caps) return -1;
  if (port_index == 0)
  {
    out_caps->sample_rate = p->sample_rate;
    out_caps->channels = p->channels;
    out_caps->format = MEDIA_FORMAT_S16;
    out_caps->bytes_per_sample = p->bytes_per_sample;
    out_caps->frame_ms = p->output_frame_ms;
  }
  return 0;
}

static int frame_adapter_set_caps(media_node_t *node, int port_index, const media_caps_t *caps)
{
  frame_adapter_priv_t *p = (frame_adapter_priv_t *)node->private_data;
  if (port_index != 0 || !p || !caps) return -1;
  p->sample_rate = caps->sample_rate;
  p->channels = caps->channels ? caps->channels : 1;
  p->bytes_per_sample = caps->bytes_per_sample ? caps->bytes_per_sample : 2 * p->channels;
  p->input_frame_ms = caps->frame_ms ? caps->frame_ms : 20;
  return 0;
}

static int frame_adapter_process(media_node_t *node)
{
  frame_adapter_priv_t *p = (frame_adapter_priv_t *)node->private_data;
  media_buffer_t *in_buf = node->input_buffers[0];
  if (!p) return 0;

  /* 将输入追加到缓冲 */
  if (in_buf && in_buf->data && in_buf->size > 0)
  {
    if (p->sample_rate == 0 && in_buf->caps.sample_rate)
    {
      p->sample_rate = in_buf->caps.sample_rate;
      p->channels = in_buf->caps.channels ? in_buf->caps.channels : 1;
      p->bytes_per_sample = in_buf->caps.bytes_per_sample ? in_buf->caps.bytes_per_sample : 2 * p->channels;
      p->input_frame_ms = in_buf->caps.frame_ms ? in_buf->caps.frame_ms : 20;
      p->output_frame_bytes = (size_t)p->sample_rate * p->channels * 2 * p->output_frame_ms / 1000;
    }
    size_t need = p->buf_used + in_buf->size;
    if (need > p->buf_size)
    {
      size_t new_sz = p->buf_size ? p->buf_size * 2 : 4096;
      while (new_sz < need) new_sz *= 2;
      uint8_t *n = (uint8_t *)realloc(p->buf, new_sz);
      if (!n) return -1;
      p->buf = n;
      p->buf_size = new_sz;
    }
    memcpy(p->buf + p->buf_used, in_buf->data, in_buf->size);
    p->buf_used += in_buf->size;
  }

  /* 若有足够数据则输出一帧 */
  if (p->output_frame_bytes == 0) return 0;
  if (p->buf_used < p->output_frame_bytes) return 0;

  media_buffer_t *out = media_buffer_alloc(p->output_frame_bytes);
  if (!out) return -1;
  memcpy(out->data, p->buf, p->output_frame_bytes);
  out->size = p->output_frame_bytes;
  out->caps.sample_rate = p->sample_rate;
  out->caps.channels = p->channels;
  out->caps.format = MEDIA_FORMAT_S16;
  out->caps.bytes_per_sample = p->bytes_per_sample;
  out->caps.frame_ms = p->output_frame_ms;

  memmove(p->buf, p->buf + p->output_frame_bytes, p->buf_used - p->output_frame_bytes);
  p->buf_used -= p->output_frame_bytes;

  node->output_buffers[0] = out;
  return 0;
}

static int frame_adapter_flush(media_node_t *node)
{
  frame_adapter_priv_t *p = (frame_adapter_priv_t *)node->private_data;
  if (p) p->buf_used = 0;
  if (node->output_buffers[0])
  {
    media_buffer_free(node->output_buffers[0]);
    node->output_buffers[0] = NULL;
  }
  return 0;
}

static void frame_adapter_destroy(media_node_t *node)
{
  frame_adapter_priv_t *p = (frame_adapter_priv_t *)node->private_data;
  if (p)
  {
    free(p->buf);
    free(p);
  }
  if (node->instance_id) free((void*)node->instance_id);
  free(node);
}

static const node_ops_t frame_adapter_ops =
{
  .init = frame_adapter_init,
  .prepare = frame_adapter_prepare,
  .get_caps = frame_adapter_get_caps,
  .set_caps = frame_adapter_set_caps,
  .process = frame_adapter_process,
  .flush = frame_adapter_flush,
  .destroy = frame_adapter_destroy,
};

static const node_descriptor_t frame_adapter_desc =
{
  .name = "filter_frame_adapter",
  .ops = &frame_adapter_ops,
  .num_input_ports = 1,
  .num_output_ports = 1,
};

media_node_t* filter_frame_adapter_create(const char *instance_id, const node_config_t *config)
{
  media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
  if (!node) return NULL;
  node->desc = &frame_adapter_desc;
  node->instance_id = strdup(instance_id ? instance_id : "frame_adapter");
  node->num_input_ports = 1;
  node->num_output_ports = 1;
  frame_adapter_priv_t *p = (frame_adapter_priv_t *)calloc(1, sizeof(frame_adapter_priv_t));
  if (!p) { free((void*)node->instance_id); free(node); return NULL; }
  p->output_frame_ms = config && config->frame_ms ? config->frame_ms : 10;
  node->private_data = p;
  return node;
}

void filter_frame_adapter_destroy_fn(media_node_t *node)
{
  frame_adapter_destroy(node);
}
