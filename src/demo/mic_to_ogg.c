/****************************************************************************
 * src/demo/mic_to_ogg.c
 *
 * Demo: mic -> opus encoder -> ogg muxer -> file (Ogg Opus).
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
#include "media_core/session.h"
#include "media_core/pipeline.h"
#include "media_core/factory.h"
#include "media_core/link.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

extern media_node_t *source_mic_create(const char *, const node_config_t *);
extern void source_mic_destroy_fn(media_node_t *);
extern media_node_t *encoder_opus_create(const char *, const node_config_t *);
extern void encoder_opus_destroy_fn(media_node_t *);
extern media_node_t *muxer_ogg_create(const char *, const node_config_t *);
extern void muxer_ogg_destroy_fn(media_node_t *);
extern media_node_t *sink_file_create(const char *, const node_config_t *);
extern void sink_file_destroy_fn(media_node_t *);

int main(int argc, char **argv)
{
  const char *out_path = "record.ogg";
  int duration_sec = 5;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--sec") == 0 && i + 1 < argc)
    {
      duration_sec = atoi(argv[++i]);

    else
    {
      out_path = argv[i];
    }
  }

  session_t *sess = session_create();
  if (!sess)
  {
    fprintf(stderr, "session_create failed\n");
    return 1;
  }
  pipeline_t *pipe = pipeline_create(sess, 1);
  if (!pipe)
  {
    fprintf(stderr, "pipeline_create failed\n");
    session_destroy(sess);
    return 1;
  }

  factory_register_node_type("source_mic", (node_create_fn)source_mic_create, source_mic_destroy_fn);
  factory_register_node_type("encoder_opus", (node_create_fn)encoder_opus_create, encoder_opus_destroy_fn);
  factory_register_node_type("muxer_ogg", (node_create_fn)muxer_ogg_create, muxer_ogg_destroy_fn);
  factory_register_node_type("sink_file", (node_create_fn)sink_file_create, sink_file_destroy_fn);

  node_config_t mic_cfg = {.sample_rate = 16000, .channels = 1};
  node_config_t enc_cfg = {.sample_rate = 16000, .channels = 1};
  node_config_t mux_cfg = {.sample_rate = 16000, .channels = 1};
  node_config_t file_cfg = {.path = out_path};

  if (pipeline_add_node(pipe, "source_mic", "mic", &mic_cfg) != 0)
  {
    fprintf(stderr, "pipeline_add_node source_mic failed. WSL: install libasound2-plugins pulseaudio\n");
    goto fail;
  }
  if (pipeline_add_node(pipe, "encoder_opus", "opus", &enc_cfg) != 0)
  {
    fprintf(stderr, "pipeline_add_node encoder_opus failed (need libopus)\n");
    goto fail;
  }
  if (pipeline_add_node(pipe, "muxer_ogg", "mux", &mux_cfg) != 0)
  {
    fprintf(stderr, "pipeline_add_node muxer_ogg failed (need libogg)\n");
    goto fail;
  }
  if (pipeline_add_node(pipe, "sink_file", "file", &file_cfg) != 0)
  {
    fprintf(stderr, "pipeline_add_node sink_file failed\n");
    goto fail;
  }
  if (pipeline_link(pipe, "mic", 0, "opus", 0) != 0 ||
    pipeline_link(pipe, "opus", 0, "mux", 0) != 0 ||
    pipeline_link(pipe, "mux", 0, "file", 0) != 0)
    {
    fprintf(stderr, "pipeline_link failed\n");
    goto fail;
  }

  {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)))
      printf("Recording to %s/%s (%d seconds)...\n", cwd, out_path, duration_sec);
    else
      printf("Recording to %s (%d seconds)...\n", out_path, duration_sec);
  }
  session_start_pipeline(sess, 1);
  sleep((unsigned)duration_sec);
  session_stop_pipeline(sess, 1);
  {
    struct timespec ts = {0, 200 * 1000 * 1000};
    nanosleep(&ts, NULL);
  }
  session_destroy(sess);
  printf("Done.\n");
  return 0;
fail:
  session_destroy(sess);
  return 1;
}
