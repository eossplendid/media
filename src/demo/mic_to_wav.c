/****************************************************************************
 * src/demo/mic_to_wav.c
 *
 * Demo: microphone -> WAV file (single pipeline).
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
#include <time.h>
#include <limits.h>

extern media_node_t* source_mic_create(const char *, const node_config_t *);
extern void source_mic_destroy_fn(media_node_t *);
extern media_node_t* sink_wav_create(const char *, const node_config_t *);
extern void sink_wav_destroy_fn(media_node_t *);
extern media_node_t* filter_frame_adapter_create(const char *, const node_config_t *);
extern void filter_frame_adapter_destroy_fn(media_node_t *);

int main(int argc, char **argv)
{
  const char *out_path = "record.wav";
  uint32_t sink_frame_ms = 0;  /* 0=不限制，20=与 mic 一致；10=自动插入 frame_adapter */
  int use_pull = 0;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--10ms") == 0) sink_frame_ms = 10;
    else if (strcmp(argv[i], "--20ms") == 0) sink_frame_ms = 20;
    else if (strcmp(argv[i], "--pull") == 0) use_pull = 1;
    else { out_path = argv[i]; break; }
  }

  session_t *sess = session_create();
  if (!sess) { fprintf(stderr, "session_create failed\n"); return 1; }
  pipeline_t *pipe = pipeline_create(sess, 1);
  if (!pipe) { fprintf(stderr, "pipeline_create failed\n"); session_destroy(sess); return 1; }

  factory_register_node_type("source_mic", (node_create_fn)source_mic_create, source_mic_destroy_fn);
  factory_register_node_type("filter_frame_adapter", (node_create_fn)filter_frame_adapter_create, filter_frame_adapter_destroy_fn);
  factory_register_node_type("sink_wav", (node_create_fn)sink_wav_create, sink_wav_destroy_fn);

  node_config_t mic_cfg = { .sample_rate = 16000, .channels = 1 };
  node_config_t wav_cfg = { .path = out_path, .frame_ms = sink_frame_ms };

  if (pipeline_add_node(pipe, "source_mic", "mic", &mic_cfg) != 0)
  {
    fprintf(stderr, "pipeline_add_node source_mic failed. WSL: install libasound2-plugins pulseaudio, set PULSE_SERVER\n");
    goto fail;
  }
  if (pipeline_add_node(pipe, "sink_wav", "wav", &wav_cfg) != 0)
  {
    fprintf(stderr, "pipeline_add_node sink_wav failed\n"); goto fail;
  }
  if (pipeline_link(pipe, "mic", 0, "wav", 0) != 0)
  {
    fprintf(stderr, "pipeline_link failed\n"); goto fail;
  }
  if (use_pull) pipeline_set_mode(pipe, PIPELINE_MODE_PULL);

  { char cwd[PATH_MAX]; if (getcwd(cwd, sizeof(cwd))) printf("Recording to %s/%s (5 seconds)...\n", cwd, out_path); else printf("Recording to %s (5 seconds)...\n", out_path); }
  session_start_pipeline(sess, 1);
  sleep(5);
  session_stop_pipeline(sess, 1);
  { struct timespec ts = { 0, 200 * 1000 * 1000 }; nanosleep(&ts, NULL); }
  /* session_destroy 会销毁所有 pipeline，勿再单独调用 pipeline_destroy */
  session_destroy(sess);
  printf("Done.\n");
  return 0;
fail:
  session_destroy(sess);
  return 1;
}
