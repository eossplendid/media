/****************************************************************************
 * src/plugins/demuxer_ogg/demuxer_ogg.c
 *
 * OGG container demuxer (1 in, 1 out); parses OGG pages, outputs Opus pa
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

#if defined(HAVE_OGG)
#include <ogg/ogg.h>
#endif

#define DEMUX_OGG_PENDING_MAX 128  /* 避免 source 读速高于消费时丢包 */

typedef struct
{
  uint8_t *data;
  size_t size;
  int64_t pts;
  uint32_t sample_rate;
  uint32_t channels;
} demux_pending_t;

typedef struct
{
#if defined(HAVE_OGG)
  ogg_sync_state oy;
  ogg_stream_state os;
  int stream_inited;
  uint32_t sample_rate;
  uint32_t channels;
  demux_pending_t pending[DEMUX_OGG_PENDING_MAX];
  int pending_count;
#endif
} demuxer_ogg_priv_t;

static int demuxer_ogg_init(media_node_t *node, const node_config_t *config)
{
  demuxer_ogg_priv_t *p = (demuxer_ogg_priv_t *)node->private_data;
  (void)config;
  if (!p) return -1;
#if defined(HAVE_OGG)
  ogg_sync_init(&p->oy);
  p->stream_inited = 0;
  p->sample_rate = 48000;
  p->channels = 1;
#endif
  return 0;
}

static int demuxer_ogg_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps)
{
  (void)node;
  if (!out_caps) return -1;
  if (port_index == 0)
  {
    memset(out_caps, 0, sizeof(*out_caps));
    out_caps->codec = MEDIA_CODEC_OPUS;
  }
  return 0;
}

static int demuxer_ogg_set_caps(media_node_t *node, int port_index, const media_caps_t *caps)
{
  (void)node;
  (void)port_index;
  (void)caps;
  return 0;
}

static int demuxer_ogg_process(media_node_t *node)
{
#if defined(HAVE_OGG)
  demuxer_ogg_priv_t *p = (demuxer_ogg_priv_t *)node->private_data;
  media_buffer_t *in_buf = node->input_buffers[0];
  if (!p) return 0;

  /* 必须优先消费输入，否则有 pending 时新数据会被丢弃（本 tick 末尾 input 会释放） */
  if (in_buf && in_buf->data && in_buf->size > 0)
  {
    char *buf = ogg_sync_buffer(&p->oy, (long)in_buf->size);
    if (!buf) return -1;
    memcpy(buf, in_buf->data, in_buf->size);
    if (ogg_sync_wrote(&p->oy, (long)in_buf->size) != 0) return -1;
  }

  ogg_page og;
  while (ogg_sync_pageout(&p->oy, &og) == 1)
  {
    if (!p->stream_inited)
    {
      ogg_stream_init(&p->os, ogg_page_serialno(&og));
      p->stream_inited = 1;
    }
    if (ogg_stream_pagein(&p->os, &og) != 0) continue;

    ogg_packet op;
    while (ogg_stream_packetout(&p->os, &op) == 1)
    {
      if (op.bytes <= 0) continue;
      if (op.b_o_s)
      {
        if (op.bytes >= 19 && memcmp(op.packet, "OpusHead", 8) == 0)
        {
          p->channels = op.packet[9];
          p->sample_rate = (uint32_t)(op.packet[12] | (op.packet[13] << 8) |
                   (op.packet[14] << 16) | (op.packet[15] << 24));
          if (!p->sample_rate) p->sample_rate = 48000;
        }
        continue;
      }
      if (op.bytes >= 8 && memcmp(op.packet, "OpusTags", 8) == 0)
        continue;
      uint8_t *copy = (uint8_t *)malloc((size_t)op.bytes);
      if (!copy) return -1;
      memcpy(copy, op.packet, (size_t)op.bytes);
      if (p->pending_count < DEMUX_OGG_PENDING_MAX)
      {
        p->pending[p->pending_count].data = copy;
        p->pending[p->pending_count].size = (size_t)op.bytes;
        p->pending[p->pending_count].pts = op.granulepos;
        p->pending[p->pending_count].sample_rate = p->sample_rate;
        p->pending[p->pending_count].channels = p->channels;
        p->pending_count++;

      else
      {
        free(copy);
      }
    }
  }
  if (p->pending_count > 0)
  {
    demux_pending_t *q = &p->pending[0];
    media_buffer_t *out_buf = media_buffer_alloc(q->size);
    if (!out_buf) return -1;
    memcpy(out_buf->data, q->data, q->size);
    out_buf->size = q->size;
    out_buf->pts = q->pts;
    out_buf->caps.codec = MEDIA_CODEC_OPUS;
    out_buf->caps.sample_rate = q->sample_rate;
    out_buf->caps.channels = q->channels;
    free(q->data);
    p->pending_count--;
    memmove(&p->pending[0], &p->pending[1], (size_t)p->pending_count * sizeof(p->pending[0]));
    node->output_buffers[0] = out_buf;
    return 0;
  }
#else
  (void)node;
#endif
  return 0;
}

static int demuxer_ogg_flush(media_node_t *node)
{
#if defined(HAVE_OGG)
  demuxer_ogg_priv_t *p = (demuxer_ogg_priv_t *)node->private_data;
  if (p)
  {
    if (p->stream_inited)
    {
      ogg_stream_clear(&p->os);
      p->stream_inited = 0;
    }
    ogg_sync_reset(&p->oy);
  }
#endif
  if (node->output_buffers[0])
  {
    media_buffer_free(node->output_buffers[0]);
    node->output_buffers[0] = NULL;
  }
  return 0;
}

static void demuxer_ogg_destroy(media_node_t *node)
{
  demuxer_ogg_priv_t *p = (demuxer_ogg_priv_t *)node->private_data;
  if (p)
  {
#if defined(HAVE_OGG)
    if (p->stream_inited) ogg_stream_clear(&p->os);
    ogg_sync_clear(&p->oy);
#endif
    free(p);
  }
  if (node->instance_id) free((void *)node->instance_id);
  free(node);
}

static const node_ops_t demuxer_ogg_ops =
{
  .init = demuxer_ogg_init,
  .get_caps = demuxer_ogg_get_caps,
  .set_caps = demuxer_ogg_set_caps,
  .process = demuxer_ogg_process,
  .flush = demuxer_ogg_flush,
  .destroy = demuxer_ogg_destroy,
};

static const node_descriptor_t demuxer_ogg_desc =
{
  .name = "demuxer_ogg",
  .ops = &demuxer_ogg_ops,
  .num_input_ports = 1,
  .num_output_ports = 1,
};

media_node_t *demuxer_ogg_create(const char *instance_id, const node_config_t *config)
{
  media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
  if (!node) return NULL;
  node->desc = &demuxer_ogg_desc;
  node->instance_id = strdup(instance_id ? instance_id : "demux_ogg");
  node->num_input_ports = 1;
  node->num_output_ports = 1;
  node->private_data = calloc(1, sizeof(demuxer_ogg_priv_t));
  if (!node->private_data)
  {
    free((void *)node->instance_id);
    free(node);
    return NULL;
  }
  (void)config;
  return node;
}

void demuxer_ogg_destroy_fn(media_node_t *node)
{
  demuxer_ogg_destroy(node);
}
