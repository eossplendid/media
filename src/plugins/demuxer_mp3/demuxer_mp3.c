/****************************************************************************
 * src/plugins/demuxer_mp3/demuxer_mp3.c
 *
 * MP3 demuxer node (1 in, 1 out); skips ID3, outputs raw MP3 frames.
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
  uint8_t *pending;
  size_t pending_size;
  size_t pending_cap;
  int id3_skipped;
} demuxer_mp3_priv_t;

#define PENDING_INIT_CAP 8192

/* Skip ID3v2 header, return bytes to skip (0 if no ID3) */
static size_t skip_id3(const uint8_t *data, size_t size)
{
  if (size < 10) return 0;
  if (memcmp(data, "ID3", 3) != 0) return 0;
  /* ID3v2 size is syncsafe integer in bytes 6-9 */
  size_t id3_size = ((size_t)(data[6] & 0x7F) << 21) |
           ((size_t)(data[7] & 0x7F) << 14) |
           ((size_t)(data[8] & 0x7F) << 7) |
           (size_t)(data[9] & 0x7F);
  id3_size += 10;  /* header size */
  return (id3_size <= size) ? id3_size : size;
}

static int append_pending(demuxer_mp3_priv_t *p, const uint8_t *data, size_t size)
{
  if (p->pending_size + size > p->pending_cap)
  {
    size_t new_cap = p->pending_cap ? p->pending_cap * 2 : PENDING_INIT_CAP;
    while (new_cap < p->pending_size + size) new_cap *= 2;
    uint8_t *n = (uint8_t *)realloc(p->pending, new_cap);
    if (!n) return -1;
    p->pending = n;
    p->pending_cap = new_cap;
  }
  memcpy(p->pending + p->pending_size, data, size);
  p->pending_size += size;
  return 0;
}

static int demuxer_mp3_init(media_node_t *node, const node_config_t *config)
{
  (void)node;(void)config;
  return 0;
}

static int demuxer_mp3_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps)
{
  (void)node;
  if (!out_caps) return -1;
  if (port_index == 0)
  {
    memset(out_caps, 0, sizeof(*out_caps));
    out_caps->codec = MEDIA_CODEC_MP3;
  }
  return 0;
}

static int demuxer_mp3_set_caps(media_node_t *node, int port_index, const media_caps_t *caps)
{
  (void)node;(void)port_index;(void)caps;
  return 0;
}

static int demuxer_mp3_process(media_node_t *node)
{
  demuxer_mp3_priv_t *p = (demuxer_mp3_priv_t *)node->private_data;
  media_buffer_t *in_buf = node->input_buffers[0];
  if (!p) return 0;

  if (in_buf && in_buf->data && in_buf->size > 0)
  {
    const uint8_t *data = in_buf->data;
    size_t size = in_buf->size;
    size_t offset = 0;

    if (!p->id3_skipped)
    {
      size_t skip = skip_id3(data, size);
      if (skip > 0)
      {
        offset = skip;
        p->id3_skipped = 1;

      else
      {
        /* Check for ID3 in first 3 bytes only */
        if (size >= 3 && memcmp(data, "ID3", 3) == 0)
        {
          /* Incomplete ID3, buffer and wait */
          if (append_pending(p, data, size) != 0) return -1;
          return 0;
        }
        p->id3_skipped = 1;
      }
    }

    if (p->pending_size > 0)
    {
      if (append_pending(p, data + offset, size - offset) != 0) return -1;
      data = p->pending;
      size = p->pending_size;
      offset = 0;
      p->pending_size = 0;

    else
    {
      data += offset;
      size -= offset;
    }

    if (size == 0) return 0;

    media_buffer_t *out_buf = media_buffer_alloc(size);
    if (!out_buf) return -1;
    memcpy(out_buf->data, data, size);
    out_buf->size = size;
    out_buf->caps.codec = MEDIA_CODEC_MP3;
    node->output_buffers[0] = out_buf;
    if (media_debug_enabled()) fprintf(stderr, "[demuxer_mp3] in=%zu out=%zu\n", (size_t)in_buf->size, size);
  }
  return 0;
}

static int demuxer_mp3_flush(media_node_t *node)
{
  demuxer_mp3_priv_t *p = (demuxer_mp3_priv_t *)node->private_data;
  if (p)
  {
    p->pending_size = 0;
    p->id3_skipped = 0;
  }
  if (node->output_buffers[0])
  {
    media_buffer_free(node->output_buffers[0]);
    node->output_buffers[0] = NULL;
  }
  return 0;
}

static void demuxer_mp3_destroy(media_node_t *node)
{
  demuxer_mp3_priv_t *p = (demuxer_mp3_priv_t *)node->private_data;
  if (p)
  {
    free(p->pending);
    free(p);
  }
  if (node->instance_id) free((void*)node->instance_id);
  free(node);
}

static const node_ops_t demuxer_mp3_ops =
{
  .init = demuxer_mp3_init,
  .get_caps = demuxer_mp3_get_caps,
  .set_caps = demuxer_mp3_set_caps,
  .process = demuxer_mp3_process,
  .flush = demuxer_mp3_flush,
  .destroy = demuxer_mp3_destroy,
};

static const node_descriptor_t demuxer_mp3_desc =
{
  .name = "demuxer_mp3",
  .ops = &demuxer_mp3_ops,
  .num_input_ports = 1,
  .num_output_ports = 1,
};

media_node_t* demuxer_mp3_create(const char *instance_id, const node_config_t *config)
{
  media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
  if (!node) return NULL;
  node->desc = &demuxer_mp3_desc;
  node->instance_id = strdup(instance_id ? instance_id : "demux_mp3");
  node->num_input_ports = 1;
  node->num_output_ports = 1;
  node->private_data = calloc(1, sizeof(demuxer_mp3_priv_t));
  if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
  (void)config;
  return node;
}

void demuxer_mp3_destroy_fn(media_node_t *node)
{
  demuxer_mp3_destroy(node);
}
