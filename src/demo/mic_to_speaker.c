/****************************************************************************
 * src/demo/mic_to_speaker.c
 *
 * Demo: microphone -> speaker (single pipeline).
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
#include <unistd.h>
#include <time.h>

extern media_node_t* source_mic_create(const char *, const node_config_t *);
extern void source_mic_destroy_fn(media_node_t *);
extern media_node_t* sink_speaker_create(const char *, const node_config_t *);
extern void sink_speaker_destroy_fn(media_node_t *);

int main(void)
{
  session_t *sess = session_create();
  if (!sess) { fprintf(stderr, "session_create failed\n"); return 1; }
  pipeline_t *pipe = pipeline_create(sess, 1);
  if (!pipe) { fprintf(stderr, "pipeline_create failed\n"); session_destroy(sess); return 1; }

  factory_register_node_type("source_mic", (node_create_fn)source_mic_create, source_mic_destroy_fn);
  factory_register_node_type("sink_speaker", (node_create_fn)sink_speaker_create, sink_speaker_destroy_fn);

  node_config_t mic_cfg = { .sample_rate = 16000, .channels = 1 };
  node_config_t spk_cfg = { .sample_rate = 16000, .channels = 1 };

  if (pipeline_add_node(pipe, "source_mic", "mic", &mic_cfg) != 0) goto fail;
  if (pipeline_add_node(pipe, "sink_speaker", "spk", &spk_cfg) != 0) goto fail;
  if (pipeline_link(pipe, "mic", 0, "spk", 0) != 0) goto fail;

  printf("Mic -> Speaker (10 sec)...\n");
  session_start_pipeline(sess, 1);
  sleep(10);
  session_stop_pipeline(sess, 1);
  { struct timespec ts = { 0, 200000000 }; nanosleep(&ts, NULL); }
  session_destroy(sess);
  printf("Done.\n");
  return 0;
fail:
  session_destroy(sess);
  return 1;
}
