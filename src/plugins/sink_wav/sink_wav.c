/****************************************************************************
 * src/plugins/sink_wav/sink_wav.c
 *
 * WAV file sink (1 in, 0 out); writes 44-byte header + PCM.
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
#include <stdio.h>

typedef struct
{
  FILE *fp;
  char *path;
  uint32_t sample_rate;
  uint32_t channels;
  uint32_t bytes_per_sample;
  int header_written;
  size_t data_bytes;
} sink_wav_priv_t;

static void write_wav_header(FILE *fp, uint32_t sample_rate, uint32_t channels,
              uint16_t bits_per_sample, uint32_t data_bytes)
              {
  uint8_t h[44];
  memcpy(h, "RIFF", 4);
  uint32_t file_len = 36 + (uint32_t)data_bytes;
  h[4] = (uint8_t)(file_len);
  h[5] = (uint8_t)(file_len >> 8);
  h[6] = (uint8_t)(file_len >> 16);
  h[7] = (uint8_t)(file_len >> 24);
  memcpy(h + 8, "WAVE", 4);
  memcpy(h + 12, "fmt ", 4);
  h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0;  /* chunk size 16 */
  h[20] = 1; h[21] = 0;  /* PCM */
  h[22] = (uint8_t)channels; h[23] = 0;
  h[24] = (uint8_t)(sample_rate); h[25] = (uint8_t)(sample_rate >> 8);
  h[26] = (uint8_t)(sample_rate >> 16); h[27] = (uint8_t)(sample_rate >> 24);
  uint32_t bps = sample_rate * channels * (bits_per_sample / 8);
  h[28] = (uint8_t)(bps); h[29] = (uint8_t)(bps >> 8);
  h[30] = (uint8_t)(bps >> 16); h[31] = (uint8_t)(bps >> 24);
  uint16_t block = (uint16_t)(channels * (bits_per_sample / 8));
  h[32] = (uint8_t)block; h[33] = (uint8_t)(block >> 8);
  h[34] = bits_per_sample; h[35] = 0;
  memcpy(h + 36, "data", 4);
  uint32_t dl = (uint32_t)data_bytes;
  h[40] = (uint8_t)(dl); h[41] = (uint8_t)(dl >> 8);
  h[42] = (uint8_t)(dl >> 16); h[43] = (uint8_t)(dl >> 24);
  fwrite(h, 1, 44, fp);
}

static int sink_wav_init(media_node_t *node, const node_config_t *config)
{
  sink_wav_priv_t *p = (sink_wav_priv_t *)node->private_data;
  if (!p || !config || !config->path) return -1;
  p->path = strdup(config->path);
  if (!p->path) return -1;
  p->fp = fopen(p->path, "wb");
  if (!p->fp) { free(p->path); p->path = NULL; return -1; }
  p->header_written = 0;
  p->data_bytes = 0;
  return 0;
}

static int sink_wav_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps)
{
  (void)node;(void)port_index;(void)out_caps;
  return 0; /* sink has no output */
}

static int sink_wav_set_caps(media_node_t *node, int port_index, const media_caps_t *caps)
{
  sink_wav_priv_t *p = (sink_wav_priv_t *)node->private_data;
  if (port_index != 0 || !p || !caps) return -1;
  p->sample_rate = caps->sample_rate;
  p->channels = caps->channels;
  p->bytes_per_sample = caps->bytes_per_sample > 0 ? caps->bytes_per_sample
    : (uint32_t)media_format_bytes(caps->format) * caps->channels;
  return 0;
}

static int sink_wav_process(media_node_t *node)
{
  sink_wav_priv_t *p = (sink_wav_priv_t *)node->private_data;
  media_buffer_t *buf = node->input_buffers[0];
  if (!p || !p->fp) return 0;
  if (!buf || !buf->data || buf->size == 0) return 0;
  if (!p->header_written)
  {
    if (buf->caps.sample_rate)
    {
      p->sample_rate = buf->caps.sample_rate;
      p->channels = buf->caps.channels ? buf->caps.channels : 1;
      p->bytes_per_sample = buf->caps.bytes_per_sample ? buf->caps.bytes_per_sample
        : 2 * p->channels;
    }
    uint16_t bps = (p->bytes_per_sample / (p->channels ? p->channels : 1)) * 8;
    if (bps == 0) bps = 16;
    write_wav_header(p->fp, p->sample_rate, p->channels, bps, (uint32_t)buf->size);
    p->header_written = 1;
    p->data_bytes = buf->size;
  } else
    p->data_bytes += buf->size;
  fwrite(buf->data, 1, buf->size, p->fp);
  fflush(p->fp);
  return 0;
}

static int sink_wav_flush(media_node_t *node)
{
  (void)node;
  return 0;
}

static void sink_wav_destroy(media_node_t *node)
{
  sink_wav_priv_t *p = (sink_wav_priv_t *)node->private_data;
  if (p)
  {
    if (p->fp && p->header_written)
    {
      /* Rewrite header with final data length */
      fseek(p->fp, 0, SEEK_SET);
      uint16_t bps = (p->bytes_per_sample / p->channels) * 8;
      if (bps == 0) bps = 16;
      write_wav_header(p->fp, p->sample_rate, p->channels, bps, (uint32_t)p->data_bytes);
    }
    if (p->fp) fclose(p->fp);
    if (p->path) free(p->path);
    free(p);
  }
  if (node->instance_id) free((void*)node->instance_id);
  free(node);
}

static const node_ops_t sink_wav_ops =
{
  .init = sink_wav_init,
  .get_caps = sink_wav_get_caps,
  .set_caps = sink_wav_set_caps,
  .process = sink_wav_process,
  .flush = sink_wav_flush,
  .destroy = sink_wav_destroy,
};

static const node_descriptor_t sink_wav_desc =
{
  .name = "sink_wav",
  .ops = &sink_wav_ops,
  .num_input_ports = 1,
  .num_output_ports = 0,
};

media_node_t* sink_wav_create(const char *instance_id, const node_config_t *config)
{
  media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
  if (!node) return NULL;
  node->desc = &sink_wav_desc;
  node->instance_id = strdup(instance_id ? instance_id : "wav");
  node->num_input_ports = 1;
  node->num_output_ports = 0;
  node->private_data = calloc(1, sizeof(sink_wav_priv_t));
  if (!node->private_data) { free((void*)node->instance_id); free(node); return NULL; }
  (void)config;
  return node;
}

void sink_wav_destroy_fn(media_node_t *node)
{
  sink_wav_destroy(node);
}
