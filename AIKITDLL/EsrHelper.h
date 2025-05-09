#pragma once

#include "aikit_biz_api.h"
#include "aikit_biz_config.h"
#include "Common.h"
#include "winrec.h"

#ifdef __cplusplus
extern "C" {
#endif

	// 导出定义
#ifdef _WIN32
#if defined(AIKITDLL_EXPORTS) || defined(_USRDLL)
#define AIKITDLL_API __declspec(dllexport)
#else
#define AIKITDLL_API __declspec(dllimport)
#endif
#else
#define AIKITDLL_API
#endif

// 音频来源类型
	enum EsrAudioSource {
		ESR_MIC = 0,  // 麦克风
		ESR_FILE      // 文件
	};

	// 错误码定义
#define E_SR_NOACTIVEDEVICE -1001
#define E_SR_INVAL          -1002
#define E_SR_RECORDFAIL     -1003
#define E_SR_ALREADY        -1004

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

	// 初始化语音识别器
	AIKITDLL_API int EsrInit(struct EsrRecognizer* esr, enum EsrAudioSource aud_src, int devid);

	// 开始监听
	AIKITDLL_API int EsrStartListening(struct EsrRecognizer* esr);

	// 停止监听
	AIKITDLL_API int EsrStopListening(struct EsrRecognizer* esr);

	// 写入音频数据
	AIKITDLL_API int EsrWriteAudioData(struct EsrRecognizer* esr, char* data, unsigned int len);

	// 释放语音识别器资源
	AIKITDLL_API void EsrUninit(struct EsrRecognizer* esr);

	// 从文件获取ESR结果
	AIKITDLL_API int EsrFromFile(const char* abilityID, const char* audio_path, int fsa_count, long* readLen);

#ifdef __cplusplus
}
#endif

// 内部状态定义
enum {
	ESR_STATE_INIT,
	ESR_STATE_STARTED
};

// 帧长度定义
#define FRAME_LEN_ESR 640 // 16k采样率的16bit音频，一帧的大小为640B, 时长20ms