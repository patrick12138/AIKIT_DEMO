#pragma once
#include "aikit_biz_api.h"
#include "aikit_biz_config.h"
#include "Common.h"
#include "winrec.h"
#include <windows.h>

namespace esr {
    int esr_microphone(const char* abilityID);
    void TestEsrMicrophone(const AIKIT_Callbacks& cbs);
}

// 语音识别器结构体
struct EsrRecognizer {
    struct recorder* recorder;   // 录音设备句柄
    EsrAudioSource aud_src;      // 音频来源
    AIKIT_DataStatus audio_status; // 音频数据状态
    const char* ABILITY;         // 能力ID
    AIKIT::AIKIT_DataBuilder* dataBuilder;    // 数据构建器
    AIKIT::AIKIT_ParamBuilder* paramBuilder;  // 参数构建器
    AIKIT_HANDLE* handle;        // AIKIT句柄
    int state;                   // 状态
};

// 内部状态定义
enum {
    ESR_STATE_INIT,
    ESR_STATE_STARTED
};

#define FRAME_LEN_ESR 640 // 16k采样率的16bit音频，一帧的大小为640B, 时长20ms
