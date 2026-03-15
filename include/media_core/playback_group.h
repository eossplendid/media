/**
 * @file playback_group.h
 * @brief 动态多源播放组：支持多线程并发添加源，当多源同时存在时自动插入 mixer 软混后统一输出。
 */
#ifndef MEDIA_CORE_PLAYBACK_GROUP_H
#define MEDIA_CORE_PLAYBACK_GROUP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct session session_t;
typedef struct playback_group playback_group_t;

/** 播放组构建配置，NULL 或各字段为 0 时使用默认值 */
typedef struct {
    uint32_t output_sample_rate;  /** 输出采样率，0=默认 48000 */
    uint32_t output_channels;     /** 输出声道数，0=默认 1 */
} playback_group_config_t;

/** 创建播放组，config 可为 NULL（使用默认 48K 单声道） */
playback_group_t* playback_group_create(session_t *session, const playback_group_config_t *config);

/** 销毁播放组（会停止并 detach 当前 pipeline） */
void playback_group_destroy(playback_group_t *pg);

/**
 * 添加音频源（线程安全）。
 * @param path WAV 文件路径
 * @return 源槽位 id (0,1,2,...)，失败返回 -1
 * 当从 1 源变为 2+ 源时，自动停止当前 pipeline、重建为 mixer 拓扑、重启。
 */
int playback_group_add_source(playback_group_t *pg, const char *path);

/**
 * 移除源（可选，用于提前结束某路）。
 * @param source_id 由 add_source 返回的 id
 */
void playback_group_remove_source(playback_group_t *pg, int source_id);

/**
 * 启动播放。若已有源则立即开始；若尚无源则等待。
 * 内部使用 pipeline_id=1。
 */
int playback_group_start(playback_group_t *pg);

/** 停止播放 */
void playback_group_stop(playback_group_t *pg);

/** 当前活跃源数量 */
int playback_group_source_count(const playback_group_t *pg);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CORE_PLAYBACK_GROUP_H */
