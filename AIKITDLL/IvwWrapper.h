#pragma once

#include "Common.h"
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

// 当前唤醒功能的句柄
static AIKIT_HANDLE* g_wakeupHandle = nullptr;

#define FRAME_LEN 16000 * 2 / 100 // 16k采样率, 16bit采样位宽, 10ms帧长

#ifdef __cplusplus
extern "C" {
#endif

	// 语音唤醒相关常量
#define IVW_ABILITY "e867a88f2" // 语音唤醒能力ID
#define CNENIVW_ABILITY "e75f07b62" // 离线命令词识别能力ID

	// 语音唤醒控制函数
	AIKITDLL_API int StartWakeupDetection(int threshold);
	AIKITDLL_API int StopWakeupDetection();

	// 语音唤醒能力初始化
	AIKITDLL_API int Ivw70Init();

	// 语音唤醒资源释放
	AIKITDLL_API int Ivw70Uninit();

	// 语音唤醒测试函数 - 从麦克风输入
	AIKITDLL_API int IvwFromMicrophone(int threshold);

	// 语音唤醒测试函数 - 从文件输入
	AIKITDLL_API int IvwFromFile(const char* audioFilePath, int threshold);

	// 获取当前唤醒状态 (0-未唤醒, 1-已唤醒)
	AIKITDLL_API int GetWakeupStatus();

	// 重置唤醒状态为未唤醒
	AIKITDLL_API void ResetWakeupStatus();

	// 重置唤醒状态为未唤醒
	AIKITDLL_API int TestIvw70(const AIKIT_Callbacks& cbs);

	AIKITDLL_API int TestIvw70Microphone(const AIKIT_Callbacks& cbs);

#ifdef __cplusplus
}
#endif

// 内部使用的函数
namespace AIKITDLL {
	// 当前的唤醒标志
	extern std::atomic<int> wakeupFlag;

	// 会话管理
	int ivw_start_session(const char* abilityID, AIKIT_HANDLE** outHandle, int threshold);
	int ivw_stop_session(AIKIT_HANDLE* handle);

	// 从麦克风进行语音唤醒的内部实现
	int ivw_microphone(const char* abilityID, int threshold, int timeoutMs);

	// 从文件进行语音唤醒的内部实现
	int ivw_file(const char* abilityID, const char* audioFilePath, int threshold);
}