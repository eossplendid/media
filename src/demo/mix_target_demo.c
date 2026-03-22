/****************************************************************************
 * src/demo/mix_target_demo.c
 *
 * Demo: 两条 pipeline 分别创建，mix_target="speaker" 时自动协商 mixer 合并。
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
#include <unistd.h>
#include <time.h>

extern media_node_t* source_mic_create(const char *, const node_config_t *);
extern void source_mic_destroy_fn(media_node_t *);
extern media_node_t* source_file_create(const char *, const node_config_t *);
extern void source_file_destroy_fn(media_node_t *);
extern media_node_t* sink_speaker_create(const char *, const node_config_t *);
extern void sink_speaker_destroy_fn(media_node_t *);
extern media_node_t* resampler_create(const char *, const node_config_t *);
extern void resampler_destroy_fn(media_node_t *);
extern media_node_t* mixer_create(const char *, const node_config_t *);
extern void mixer_destroy_fn(media_node_t *);

int main(int argc, char **argv)
{
  const char *wav_path = argc > 1 ? argv[1] : "record.wav";
  session_t *sess = session_create();
  if (!sess) { fprintf(stderr, "session_create failed\n"); return 1; }

  factory_register_node_type("source_mic", (node_create_fn)source_mic_create, source_mic_destroy_fn);
  factory_register_node_type("source_file", (node_create_fn)source_file_create, source_file_destroy_fn);
  factory_register_node_type("filter_resampler", (node_create_fn)resampler_create, resampler_destroy_fn);
  factory_register_node_type("filter_mixer", (node_create_fn)mixer_create, mixer_destroy_fn);
  factory_register_node_type("sink_speaker", (node_create_fn)sink_speaker_create, sink_speaker_destroy_fn);

  /* Pipeline 1: mic -> spk, mix_target=speaker */
  pipeline_t *pipe1 = pipeline_create(sess, 1);
  if (!pipe1) { fprintf(stderr, "pipeline_create 1 failed\n"); session_destroy(sess); return 1; }
  pipeline_set_mix_target(pipe1, "speaker");

  node_config_t mic_cfg = { .sample_rate = 16000, .channels = 1 };
  node_config_t spk_cfg = { .sample_rate = 48000, .channels = 1 };
  pipeline_add_node(pipe1, "source_mic", "mic", &mic_cfg);
  pipeline_add_node(pipe1, "sink_speaker", "spk", &spk_cfg);
  pipeline_link(pipe1, "mic", 0, "spk", 0);

  /* Pipeline 2: file -> spk, mix_target=speaker (将与 1 自动合并) */
  pipeline_t *pipe2 = pipeline_create(sess, 2);
  if (!pipe2) { fprintf(stderr, "pipeline_create 2 failed\n"); session_destroy(sess); return 1; }
  pipeline_set_mix_target(pipe2, "speaker");

  node_config_t file_cfg = { .path = wav_path };
  pipeline_add_node(pipe2, "source_file", "file", &file_cfg);
  pipeline_add_node(pipe2, "sink_speaker", "spk", &spk_cfg);
  pipeline_link(pipe2, "file", 0, "spk", 0);

  printf(" mix_target demo: start pipe1 (mic), then pipe2 (file) -> auto merge to mixer\n");
  session_start_pipeline(sess, 1);
  sleep(3);
  session_start_pipeline(sess, 2);
  sleep(12);
  session_stop_pipeline(sess, 1);
  { struct timespec ts = { 0, 500000000 }; nanosleep(&ts, NULL); }
  session_destroy(sess);
  printf("Done.\n");
  return 0;
}
