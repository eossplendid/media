/****************************************************************************
 * src/plugins/sink_file/sink_file.c
 *
 * Raw file sink (1 in, 0 out); writes input buffer bytes to file.
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

typedef struct
{
  FILE *fp;
  char *path;
} sink_file_priv_t;

static int sink_file_init(media_node_t *node, const node_config_t *config)
{
  sink_file_priv_t *p = (sink_file_priv_t *)node->private_data;
  if (!p || !config) return -1;
  const char *path = config_get_string(config, "path", NULL);
  if (!path || !path[0]) return -1;
  p->path = strdup(path);
  if (!p->path) return -1;
  p->fp = fopen(p->path, "wb");
  if (!p->fp)
  {
    free(p->path);
    p->path = NULL;
    return -1;
  }
  return 0;
}

static int sink_file_get_caps(media_node_t *node, int port_index, media_caps_t *out_caps)
{
  (void)node;
  (void)port_index;
  (void)out_caps;
  return 0;
}

static int sink_file_set_caps(media_node_t *node, int port_index, const media_caps_t *caps)
{
  (void)node;
  (void)port_index;
  (void)caps;
  return 0;
}

static int sink_file_process(media_node_t *node)
{
  sink_file_priv_t *p = (sink_file_priv_t *)node->private_data;
  media_buffer_t *buf = node->input_buffers[0];
  if (!p || !p->fp) return 0;
  if (!buf || !buf->data || buf->size == 0) return 0;
  fwrite(buf->data, 1, buf->size, p->fp);
  fflush(p->fp);
  return 0;
}

static int sink_file_flush(media_node_t *node)
{
  (void)node;
  return 0;
}

static void sink_file_destroy(media_node_t *node)
{
  sink_file_priv_t *p = (sink_file_priv_t *)node->private_data;
  if (p)
  {
    if (p->fp) fclose(p->fp);
    if (p->path) free(p->path);
    free(p);
  }
  if (node->instance_id) free((void *)node->instance_id);
  free(node);
}

static const node_ops_t sink_file_ops =
{
  .init = sink_file_init,
  .get_caps = sink_file_get_caps,
  .set_caps = sink_file_set_caps,
  .process = sink_file_process,
  .flush = sink_file_flush,
  .destroy = sink_file_destroy,
};

static const node_descriptor_t sink_file_desc =
{
  .name = "sink_file",
  .ops = &sink_file_ops,
  .num_input_ports = 1,
  .num_output_ports = 0,
};

media_node_t *sink_file_create(const char *instance_id, const node_config_t *config)
{
  media_node_t *node = (media_node_t *)calloc(1, sizeof(media_node_t));
  if (!node) return NULL;
  node->desc = &sink_file_desc;
  node->instance_id = strdup(instance_id ? instance_id : "file");
  node->num_input_ports = 1;
  node->num_output_ports = 0;
  node->private_data = calloc(1, sizeof(sink_file_priv_t));
  if (!node->private_data)
  {
    free((void *)node->instance_id);
    free(node);
    return NULL;
  }
  return node;
}

void sink_file_destroy_fn(media_node_t *node)
{
  sink_file_destroy(node);
}
