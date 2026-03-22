/**
 * @file audio_focus.h
 * @brief Audio focus: GAIN/TRANSIENT/DUCKABLE, request/abandon.
 */
#ifndef MEDIA_CORE_AUDIO_FOCUS_H
#define MEDIA_CORE_AUDIO_FOCUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct session session_t;
typedef uint32_t pipeline_id_t;

typedef enum {
    AUDIO_FOCUS_GAIN = 0,      /* 独占焦点 */
    AUDIO_FOCUS_TRANSIENT,     /* 短时（如提示音） */
    AUDIO_FOCUS_DUCKABLE       /* 可降噪（如导航时音乐降低） */
} audio_focus_type_t;

/**
 * 请求音频焦点。
 * @return 0 成功，负值失败
 */
int audio_focus_request(session_t *session, pipeline_id_t id, audio_focus_type_t type);

/**
 * 放弃焦点。
 */
void audio_focus_abandon(session_t *session, pipeline_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* MEDIA_CORE_AUDIO_FOCUS_H */
