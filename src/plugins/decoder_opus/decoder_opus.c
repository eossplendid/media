/****************************************************************************
 * src/plugins/decoder_opus/decoder_opus.c
 *
 * Opus decoder node (1 in, 1 out); Opus packets -> PCM S16.
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

#if defined(HAVE_OPUS)
#include <opus/opus.h>
#endif

#define DECODER_OPUS_MAX_FRAMES 5760

typedef struct
{
#if defined(HAVE_OPUS)
  OpusDecoder *dec;
  uint32_t sample_rate;
  uint32_t channels;
#endif
} decoder_opus_priv_t;

static int decoder_opus_init(media_node_t *node, const node_config_t *config)
{
  (void)config;
  decoder_opus_priv_t *p = (decoder_opus_priv_t *)node->private_data;
  if (!p) return -1;
#if defined(HAVE_OPUS)
  p->sample_rate = 48000;
  p->channels = 1;
  int err;
  p->dec = opus_decoder_create(48000, 1, &err);
  if (!p->dec || err != OPUS_OK) return -1;
#endif
  return 0;
}

static int decoder_opus_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps)
{
  decoder_opus_priv_t *p = (decoder_opus_priv_t *)node->private_data;
  if (!p || !out_caps) return -1;
  if (port_index == 0)
  {
    memset(out_caps, 0, sizeof(*out_caps));
#if defined(HAVE_OPUS)
    out_caps->sample_rate = p->sample_rate ? p->sample_rate : 48000;
    out_caps->channels = p->channels ? p->channels : 1;
#endif
    out_caps->format = MEDIA_FORMAT_S16;
    out_caps->bytes_per_sample = 2 * (p ? p->channels : 1);
    out_caps->codec = MEDIA_CODEC_NONE;
  }
  return 0;
}

static int decoder_opus_set_caps(media_node_t *node, int port_index, const media_caps_t *caps)
{
  decoder_opus_priv_t *p = (decoder_opus_priv_t *)node->private_data;
  if (!p || !caps) return -1;
#if defined(HAVE_OPUS)
  if (port_index == 0 && caps->channels)
  {
    uint32_t ch = caps->channels;
    if (ch != p->channels)
    {
      if (p->dec)
      {
        opus_decoder_destroy(p->dec);
        p->dec = NULL;
      }
      p->channels = ch;
      int err;
      p->dec = opus_decoder_create(48000, (int)ch, &err);
      if (!p->dec || err != OPUS_OK) return -1;
    }
  }
#endif
  return 0;
}

static int decoder_opus_process(media_node_t *node)
{
#if defined(HAVE_OPUS)
  decoder_opus_priv_t *p = (decoder_opus_priv_t *)node->private_data;
  media_buffer_t *in_buf = node->input_buffers[0];
  if (!p || !in_buf || !in_buf->data || in_buf->size == 0) return 0;
  if (in_buf->caps.channels && in_buf->caps.channels != p->channels)
  {
    if (p->dec) opus_decoder_destroy(p->dec);
    p->channels = in_buf->caps.channels;
    int err;
    p->dec = opus_decoder_create(48000, (int)p->channels, &err);
    if (!p->dec || err != OPUS_OK) return 0;
  }
  if (!p->dec) return 0;

  int max_samples = DECODER_OPUS_MAX_FRAMES * (int)p->channels;
  media_buffer_t *out_buf = media_buffer_alloc((size_t)max_samples * sizeof(int16_t));
  if (!out_buf) return -1;
  int n = opus_decode(p->dec, in_buf->data, (opus_int32)in_buf->size,
            (opus_int16 *)out_buf->data, max_samples, 0);
  if (n < 0)
  {
    media_buffer_free(out_buf);
    return 0;
  }
  out_buf->size = (size_t)n * sizeof(int16_t);
  out_buf->pts = in_buf->pts;
  out_buf->caps.sample_rate = 48000;
  out_buf->caps.channels = p->channels;
  out_buf->caps.format = MEDIA_FORMAT_S16;
  out_buf->caps.bytes_per_sample = 2 * p->channels;
  out_buf->caps.codec = MEDIA_CODEC_NONE;
  node->output_buffers[0] = out_buf;
  return 0;
#else
  (void)node;
  return 0;
#endif
}

static int decoder_opus_flush(media_node_t *node)
{
  if (node->output_buffers[0])
  {
    media_buffer_free(node->output_buffers[0]);
    node->output_buffers[0] = NULL;
  }
  return 0;
}

static void decoder_opus_destroy(media_node_t *node)
{
  decoder_opus_priv_t *p = (decoder_opus_priv_t *)node->private_data;
  if (p)
  {
#if defined(HAVE_OPUS)
    if (p->dec) opus_decoder_destroy(p->dec);
#endif
    free(p);
  }
  if (node->instance_id) free((void *)node->instance_id);
  free(node);
}

static const node_ops_t decoder_opus_ops =
{
  .init = decoder_opus_init,
  .get_caps = decoder_opus_get_caps,
  .set_caps = decoder_opus_set_caps,
  .process = decoder_opus_process,
  .flush = decoder_opus_flush,
  .destroy = decoder_opus_destroy,
};

static const node_descriptor_t decoder_opus_desc =
{
  .name = "decoder_opus",
  .ops = &decoder_opus_ops,
  .num_input_ports = 1,
  .num_output_ports = 1,
};

media_node_t *decoder_opus_create(const char *instance_id, const node_config_t *config)
{
  media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
  if (!node) return NULL;
  node->desc = &decoder_opus_desc;
  node->instance_id = strdup(instance_id ? instance_id : "dec_opus");
  node->num_input_ports = 1;
  node->num_output_ports = 1;
  node->private_data = calloc(1, sizeof(decoder_opus_priv_t));
  if (!node->private_data)
  {
    free((void *)node->instance_id);
    free(node);
    return NULL;
  }
  (void)config;
  return node;
}

void decoder_opus_destroy_fn(media_node_t *node)
{
  decoder_opus_destroy(node);
}
