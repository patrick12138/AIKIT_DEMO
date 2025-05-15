#pragma once

#include "Common.h"
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define FRAME_LEN 16000 * 2 / 100 // 16k采样率, 16bit采样位宽, 10ms帧长

#ifdef __cplusplus
extern "C" {
#endif

	// 语音唤醒相关常量
#define IVW_ABILITY "e867a88f2" // 语音唤醒能力ID
#define CNENIVW_ABILITY "e75f07b62" // 离线命令词识别能力ID

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
		// 获取唤醒状态的详细信息（用于调试）
	AIKITDLL_API const char* GetWakeupStatusDetails();

	// 测试函数
	AIKITDLL_API int TestIvw70(const AIKIT_Callbacks& cbs);

	AIKITDLL_API int Ivw70Microphone(const AIKIT_Callbacks& cbs);
	
	// 测试唤醒检测功能
	AIKITDLL_API int TestWakeupDetection();

#ifdef __cplusplus
}
#endif

// 内部使用的函数
namespace AIKITDLL {
	// 当前的唤醒标志
	extern std::atomic<int> wakeupFlag;

	// 从麦克风进行语音唤醒的内部实现
	int ivw_microphone(const char* abilityID, int threshold, int timeoutMs);

	// 从文件进行语音唤醒的内部实现
	int ivw_file(const char* abilityID, const char* audioFilePath, int threshold);
}