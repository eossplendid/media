/****************************************************************************
 * src/plugins/filter_resampler/filter_resampler.c
 *
 * Resampler (1 in, 1 out); linear interpolation, e.g. 16k -> 48k.
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
#include "media_core/media_debug.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct
{
  uint32_t rate_in;
  uint32_t rate_out;
  uint32_t channels;
  double ratio;
} resampler_priv_t;

static int resampler_init(media_node_t *node, const node_config_t *config)
{
  (void)config;
  resampler_priv_t *p = (resampler_priv_t *)node->private_data;
  if (!p) return -1;
  if (p->rate_in && p->rate_out) p->ratio = (double)p->rate_out / (double)p->rate_in;
  return 0;
}

static int resampler_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps)
{
  resampler_priv_t *p = (resampler_priv_t *)node->private_data;
  if (!p || !out_caps) return -1;
  if (port_index == 0)
  {
    out_caps->sample_rate = p->rate_out;
    out_caps->channels = p->channels;
    out_caps->format = MEDIA_FORMAT_S16;
    out_caps->bytes_per_sample = 2 * p->channels;
  }
  return 0;
}

static int resampler_set_caps(media_node_t *node, int port_index, const media_caps_t *caps)
{
  resampler_priv_t *p = (resampler_priv_t *)node->private_data;
  if (!p || !caps) return -1;
  if (port_index == 0)
  {
    p->rate_in = caps->sample_rate;
    p->channels = caps->channels ? caps->channels : 1;
    if (p->rate_out) p->ratio = (double)p->rate_out / (double)p->rate_in;
  }
  return 0;
}

/* Linear interpolation resample S16 mono/stereo */
static void resample_s16(const int16_t *in, size_t in_frames, int16_t *out, size_t out_frames,
            unsigned int channels, double ratio)
            {
  if (channels == 0) channels = 1;
  for (size_t i = 0; i < out_frames; i++)
  {
    double src_idx = (double)i / ratio;
    size_t i0 = (size_t)src_idx;
    size_t i1 = i0 + 1;
    if (i1 >= in_frames) i1 = in_frames - 1;
    double frac = src_idx - (double)i0;
    for (unsigned int c = 0; c < channels; c++)
    {
      int32_t a = in[i0 * channels + c];
      int32_t b = in[i1 * channels + c];
      out[i * channels + c] = (int16_t)(a + (int32_t)(frac * (b - a)));
    }
  }
}

static int resampler_process(media_node_t *node)
{
  resampler_priv_t *p = (resampler_priv_t *)node->private_data;
  media_buffer_t *in_buf = node->input_buffers[0];
  if (!p) return 0;
  if (!in_buf || !in_buf->data || in_buf->size == 0) return 0;
  /* 优先使用上游实际输出的采样率（decoder 等运行时才确定），prepare 时的 get_caps 可能返回默认值 */
  if (in_buf->caps.sample_rate)
  {
    p->rate_in = in_buf->caps.sample_rate;
    p->channels = in_buf->caps.channels ? in_buf->caps.channels : 1;
    if (p->rate_out) p->ratio = (double)p->rate_out / (double)p->rate_in;
  }
  if (p->ratio <= 0.0) return 0;
  unsigned int ch = p->channels ? p->channels : 1;
  size_t in_frames = in_buf->size / (2 * ch);
  size_t out_frames = (size_t)((double)in_frames * p->ratio);
  if (out_frames == 0) return 0;
  size_t out_size = out_frames * 2 * ch;
  media_buffer_t *out_buf = media_buffer_alloc(out_size);
  if (!out_buf) return -1;
  out_buf->caps.sample_rate = p->rate_out;
  out_buf->caps.channels = ch;
  out_buf->caps.format = MEDIA_FORMAT_S16;
  out_buf->caps.bytes_per_sample = 2 * ch;
  resample_s16((const int16_t *)in_buf->data, in_frames, (int16_t *)out_buf->data, out_frames, ch, p->ratio);
  out_buf->size = out_size;
  /* Pipeline frees output_buffers at end of each iteration */
  node->output_buffers[0] = out_buf;
  if (media_debug_enabled()) fprintf(stderr, "[filter_resampler] %zu->%zu frames %u->%u Hz\n", in_frames, out_frames, p->rate_in, p->rate_out);
  return 0;
}

static int resampler_flush(media_node_t *node)
{
  if (node->output_buffers[0])
  {
    media_buffer_free(node->output_buffers[0]);
    node->output_buffers[0] = NULL;
  }
  return 0;
}

static void resampler_destroy(media_node_t *node)
{
  if (node->private_data) free(node->private_data);
  if (node->instance_id) free((void*)node->instance_id);
  free(node);
}

static const node_ops_t resampler_ops =
{
  .init = resampler_init,
  .get_caps = resampler_get_caps,
  .set_caps = resampler_set_caps,
  .process = resampler_process,
  .flush = resampler_flush,
  .destroy = resampler_destroy,
};

static const node_descriptor_t resampler_desc =
{
  .name = "filter_resampler",
  .ops = &resampler_ops,
  .num_input_ports = 1,
  .num_output_ports = 1,
};

media_node_t* resampler_create(const char *instance_id, const node_config_t *config)
{
  media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
  if (!node) return NULL;
  node->desc = &resampler_desc;
  node->instance_id = strdup(instance_id ? instance_id : "resamp");
  node->num_input_ports = 1;
  node->num_output_ports = 1;
  node->private_data = calloc(1, sizeof(resampler_priv_t));
  if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
  resampler_priv_t *p = (resampler_priv_t *)node->private_data;
  p->rate_out = config && config->sample_rate ? config->sample_rate : 48000;
  return node;
}

void resampler_destroy_fn(media_node_t *node)
{
  resampler_destroy(node);
}
