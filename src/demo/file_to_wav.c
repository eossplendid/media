/**
 * @file file_to_wav.c
 * @brief Demo: file -> demuxer -> decoder -> sink_wav(WAV muxer+file sink).
 *        根据输入文件类型自动选择 demuxer/decoder，解码后封装为 WAV 保存。
 *        用法: file_to_wav <input> <output.wav>
 *        支持: WAV(PCM)、MP3、OGG-Opus
 */
#include "media_core/session.h"
#include "media_core/pipeline.h"
#include "media_core/factory.h"
#include "media_core/link.h"
#include "media_types.h"
#include "media_core/media_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern media_node_t *source_file_create(const char *, const node_config_t *);
extern void source_file_destroy_fn(media_node_t *);
extern media_node_t *sink_wav_create(const char *, const node_config_t *);
extern void sink_wav_destroy_fn(media_node_t *);
extern media_node_t *demuxer_mp3_create(const char *, const node_config_t *);
extern void demuxer_mp3_destroy_fn(media_node_t *);
extern media_node_t *decoder_mp3_create(const char *, const node_config_t *);
extern void decoder_mp3_destroy_fn(media_node_t *);
extern media_node_t *demuxer_ogg_create(const char *, const node_config_t *);
extern void demuxer_ogg_destroy_fn(media_node_t *);
extern media_node_t *decoder_opus_create(const char *, const node_config_t *);
extern void decoder_opus_destroy_fn(media_node_t *);
extern media_node_t *resampler_create(const char *, const node_config_t *);
extern void resampler_destroy_fn(media_node_t *);
extern media_node_t *filter_format_converter_create(const char *,
                                                    const node_config_t *);
extern void filter_format_converter_destroy_fn(media_node_t *);

typedef enum
{
  FMT_WAV,
  FMT_MP3,
  FMT_OGG,
  FMT_UNKNOWN
} detected_format_t;

/* 通过 magic 检测文件格式 */
static detected_format_t detect_format(const char *path)
{
  FILE *f = fopen(path, "rb");
  if (!f) return FMT_UNKNOWN;
  uint8_t magic[12];
  size_t n = fread(magic, 1, sizeof(magic), f);
  fclose(f);
  if (n < 4) return FMT_UNKNOWN;
  if (n >= 12 && memcmp(magic, "RIFF", 4) == 0 &&
      memcmp(magic + 8, "WAVE", 4) == 0)
    return FMT_WAV;
  if (n >= 3 && memcmp(magic, "ID3", 3) == 0) return FMT_MP3;
  if (n >= 2 && magic[0] == 0xFF && (magic[1] & 0xE0) == 0xE0) return FMT_MP3;
  if (n >= 4 && memcmp(magic, "OggS", 4) == 0) return FMT_OGG;
  return FMT_UNKNOWN;
}

int main(int argc, char **argv)
{
  if (argc < 3)
    {
      fprintf(stderr, "Usage: file_to_wav <input> <output.wav>\n");
      fprintf(stderr, "  Supports: WAV(PCM), MP3, OGG-Opus\n");
      return 1;
    }
  const char *in_path = argv[1];
  const char *out_path = argv[2];

  if (media_debug_enabled())
    fprintf(stderr, "[file_to_wav] MEDIA_DEBUG=1\n");

  detected_format_t fmt = detect_format(in_path);
  if (fmt == FMT_UNKNOWN)
    {
      fprintf(stderr, "Unknown format or cannot open: %s\n", in_path);
      return 1;
    }

  const char *fmt_str = fmt == FMT_WAV ? "WAV" :
                        (fmt == FMT_MP3 ? "MP3" : "OGG");
  printf("Input:  %s [%s]\n", in_path, fmt_str);
  printf("Output: %s (WAV)\n", out_path);

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

  /* 公共节点类型 */
  factory_register_node_type("source_file",
                            (node_create_fn)source_file_create,
                            source_file_destroy_fn);
  factory_register_node_type("sink_wav",
                            (node_create_fn)sink_wav_create,
                            sink_wav_destroy_fn);
  factory_register_node_type("filter_resampler",
                            (node_create_fn)resampler_create,
                            resampler_destroy_fn);
  factory_register_node_type("filter_format_converter",
                            (node_create_fn)filter_format_converter_create,
                            filter_format_converter_destroy_fn);

  node_config_t file_cfg = { .path = in_path };
  node_config_t wav_cfg = { .path = out_path };

  if (fmt == FMT_WAV)
    {
      /* WAV: source_file 内部解析 WAV 并输出 PCM，直连 sink_wav */
      pipeline_add_node(pipe, "source_file", "file", &file_cfg);
      pipeline_add_node(pipe, "sink_wav", "wav", &wav_cfg);
      pipeline_link(pipe, "file", 0, "wav", 0);
      /* pipeline_prepare 会根据采样率/格式差异自动插入 resampler/format_converter */
    }
  else if (fmt == FMT_MP3)
    {
      /* MP3: file -> demuxer_mp3 -> decoder_mp3 -> sink_wav */
      factory_register_node_type("demuxer_mp3",
                                (node_create_fn)demuxer_mp3_create,
                                demuxer_mp3_destroy_fn);
      factory_register_node_type("decoder_mp3",
                                (node_create_fn)decoder_mp3_create,
                                decoder_mp3_destroy_fn);
      pipeline_add_node(pipe, "source_file", "file", &file_cfg);
      pipeline_add_node(pipe, "demuxer_mp3", "demux", NULL);
      pipeline_add_node(pipe, "decoder_mp3", "dec", NULL);
      pipeline_add_node(pipe, "sink_wav", "wav", &wav_cfg);
      pipeline_link(pipe, "file", 0, "demux", 0);
      pipeline_link(pipe, "demux", 0, "dec", 0);
      pipeline_link(pipe, "dec", 0, "wav", 0);
    }
  else
    {
      /* OGG-Opus: file -> demuxer_ogg -> decoder_opus -> sink_wav */
      factory_register_node_type("demuxer_ogg",
                                (node_create_fn)demuxer_ogg_create,
                                demuxer_ogg_destroy_fn);
      factory_register_node_type("decoder_opus",
                                (node_create_fn)decoder_opus_create,
                                decoder_opus_destroy_fn);
      pipeline_add_node(pipe, "source_file", "file", &file_cfg);
      pipeline_add_node(pipe, "demuxer_ogg", "demux", NULL);
      pipeline_add_node(pipe, "decoder_opus", "dec", NULL);
      pipeline_add_node(pipe, "sink_wav", "wav", &wav_cfg);
      pipeline_link(pipe, "file", 0, "demux", 0);
      pipeline_link(pipe, "demux", 0, "dec", 0);
      pipeline_link(pipe, "dec", 0, "wav", 0);
    }

  if (media_debug_enabled())
    fprintf(stderr, "[file_to_wav] starting pipeline...\n");

  session_start_pipeline(sess, 1);

  while (pipeline_is_running(pipe))
    usleep(100000);

  usleep(100000);
  session_destroy(sess);

  if (media_debug_enabled())
    fprintf(stderr, "[file_to_wav] pipeline stopped\n");

  printf("Done. Output written to %s\n", out_path);
  return 0;
}
