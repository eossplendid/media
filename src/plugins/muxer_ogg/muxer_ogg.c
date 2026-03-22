/****************************************************************************
 * src/plugins/muxer_ogg/muxer_ogg.c
 *
 * Ogg Opus muxer (1 in, 1 out); Opus packets -> Ogg pages.
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
#include "media_core/config_schema.h"
#include "media_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(HAVE_OGG)
#include <ogg/ogg.h>
#endif

typedef struct
{
#if defined(HAVE_OGG)
  ogg_stream_state os;
  int header_done;
  uint32_t sample_rate;
  uint32_t channels;
  int64_t granulepos;
#endif
} muxer_ogg_priv_t;

static int muxer_ogg_init(media_node_t *node, const node_config_t *config)
{
  muxer_ogg_priv_t *p = (muxer_ogg_priv_t *)node->private_data;
  if (!p) return -1;
#if defined(HAVE_OGG)
  p->sample_rate = config_get_uint32(config, "sample_rate", 16000);
  p->channels = config_get_uint32(config, "channels", 1);
  p->header_done = 0;
  p->granulepos = 0;
  ogg_stream_init(&p->os, 1);
#endif
  return 0;
}

static int muxer_ogg_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps)
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

static int muxer_ogg_set_caps(media_node_t *node, int port_index, const media_caps_t *caps)
{
  muxer_ogg_priv_t *p = (muxer_ogg_priv_t *)node->private_data;
  if (!p || !caps) return -1;
#if defined(HAVE_OGG)
  if (port_index == 0)
  {
    if (caps->sample_rate) p->sample_rate = caps->sample_rate;
    if (caps->channels) p->channels = caps->channels;
  }
#endif
  return 0;
}

static int muxer_ogg_process(media_node_t *node)
{
#if defined(HAVE_OGG)
  muxer_ogg_priv_t *p = (muxer_ogg_priv_t *)node->private_data;
  media_buffer_t *in_buf = node->input_buffers[0];
  if (!p || !in_buf || !in_buf->data || in_buf->size == 0) return 0;

  if (!p->header_done)
  {
    uint8_t opushead[19];
    memcpy(opushead, "OpusHead", 8);
    opushead[8] = 1;
    opushead[9] = (uint8_t)(p->channels & 0xff);
    opushead[10] = 312 & 0xff;
    opushead[11] = (312 >> 8) & 0xff;
    uint32_t rate = p->sample_rate;
    opushead[12] = rate & 0xff;
    opushead[13] = (rate >> 8) & 0xff;
    opushead[14] = (rate >> 16) & 0xff;
    opushead[15] = (rate >> 24) & 0xff;
    opushead[16] = 0;
    opushead[17] = 0;
    opushead[18] = 0;

    ogg_packet op;
    op.packet = opushead;
    op.bytes = 19;
    op.b_o_s = 1;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;
    ogg_stream_packetin(&p->os, &op);

    const char *vendor = "media";
    size_t vendor_len = strlen(vendor);
    size_t tags_len = 8 + 4 + vendor_len + 4;
    uint8_t *opustags = (uint8_t *)malloc(tags_len);
    if (!opustags) return -1;
    memcpy(opustags, "OpusTags", 8);
    opustags[8] = vendor_len & 0xff;
    opustags[9] = (vendor_len >> 8) & 0xff;
    opustags[10] = (vendor_len >> 16) & 0xff;
    opustags[11] = (vendor_len >> 24) & 0xff;
    memcpy(opustags + 12, vendor, vendor_len);
    opustags[12 + vendor_len] = 0;
    opustags[13 + vendor_len] = 0;
    opustags[14 + vendor_len] = 0;
    opustags[15 + vendor_len] = 0;
    op.packet = opustags;
    op.bytes = (long)tags_len;
    op.b_o_s = 0;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 1;
    ogg_stream_packetin(&p->os, &op);
    free(opustags);
    p->header_done = 1;
  }

  /* 20ms at 48kHz = 960 samples per packet */
  int64_t samples = 960;
  p->granulepos += samples;

  ogg_packet op;
  op.packet = in_buf->data;
  op.bytes = (long)in_buf->size;
  op.b_o_s = 0;
  op.e_o_s = 0;
  op.granulepos = p->granulepos;
  op.packetno = 2;
  ogg_stream_packetin(&p->os, &op);

  uint8_t tmp[8192];
  size_t off = 0;
  ogg_page og;
  while (ogg_stream_pageout(&p->os, &og) == 1)
  {
    size_t need = (size_t)(og.header_len + og.body_len);
    if (off + need > sizeof(tmp)) break;
    memcpy(tmp + off, og.header, (size_t)og.header_len);
    off += (size_t)og.header_len;
    memcpy(tmp + off, og.body, (size_t)og.body_len);
    off += (size_t)og.body_len;
  }
  if (off == 0)
  {
    node->output_buffers[0] = NULL;
    return 0;
  }
  media_buffer_t *out_buf = media_buffer_alloc(off);
  if (!out_buf) return -1;
  memcpy(out_buf->data, tmp, off);
  out_buf->size = off;
  out_buf->pts = in_buf->pts;
  out_buf->caps.codec = MEDIA_CODEC_OPUS;
  node->output_buffers[0] = out_buf;
  return 0;
#else
  (void)node;
  return 0;
#endif
}

static int muxer_ogg_flush(media_node_t *node)
{
#if defined(HAVE_OGG)
  (void)node;
#endif
  if (node->output_buffers[0])
  {
    media_buffer_free(node->output_buffers[0]);
    node->output_buffers[0] = NULL;
  }
  return 0;
}

static void muxer_ogg_destroy(media_node_t *node)
{
  muxer_ogg_priv_t *p = (muxer_ogg_priv_t *)node->private_data;
  if (p)
  {
#if defined(HAVE_OGG)
    ogg_stream_clear(&p->os);
#endif
    free(p);
  }
  if (node->instance_id) free((void*)node->instance_id);
  free(node);
}

static const node_ops_t muxer_ogg_ops =
{
  .init = muxer_ogg_init,
  .get_caps = muxer_ogg_get_caps,
  .set_caps = muxer_ogg_set_caps,
  .process = muxer_ogg_process,
  .flush = muxer_ogg_flush,
  .destroy = muxer_ogg_destroy,
  .set_volume = NULL,
};

static const node_descriptor_t muxer_ogg_desc =
{
  .name = "muxer_ogg",
  .ops = &muxer_ogg_ops,
  .num_input_ports = 1,
  .num_output_ports = 1,
};

media_node_t* muxer_ogg_create(const char *instance_id, const node_config_t *config)
{
  media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
  if (!node) return NULL;
  node->desc = &muxer_ogg_desc;
  node->instance_id = strdup(instance_id ? instance_id : "mux_ogg");
  node->num_input_ports = 1;
  node->num_output_ports = 1;
  node->private_data = calloc(1, sizeof(muxer_ogg_priv_t));
  if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
  (void)config;
  return node;
}

void muxer_ogg_destroy_fn(media_node_t *node)
{
  muxer_ogg_destroy(node);
}
