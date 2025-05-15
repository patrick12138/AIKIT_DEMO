#pragma once

#include "Common.h"
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define FRAME_LEN_ESR 320 // 处理ESR数据时使用的帧长
#ifdef __cplusplus
extern "C" {
#endif

	// 导出定义
#ifdef _WIN32
#ifdef AIKITDLL_EXPORTS
#define AIKITDLL_API __declspec(dllexport)
#else
#define AIKITDLL_API __declspec(dllimport)
#endif
#else
#define AIKITDLL_API
#endif

	// CNEN-ESR相关常量
#define ESR_ABILITY "e75f07b62" // 离线命令词识别能力ID

// ESR结果回调相关错误码
#define ESR_HAS_RESULT 6001

	// ESR能力初始化
	AIKITDLL_API int CnenEsrInit();

	// ESR资源释放
	AIKITDLL_API int CnenEsrUninit();

	// ESR测试函数 - 从麦克风输入
	AIKITDLL_API int EsrFromMicrophone();

	//// ESR测试函数 - 从文件输入
	//AIKITDLL_API int EsrFromFile(const char* audioFilePath);

	// 主测试函数
	AIKITDLL_API void TestEsr(const AIKIT_Callbacks& cbs);

	AIKITDLL_API void TestEsrMicrophone(const AIKIT_Callbacks& cbs);

	void ResetEsrStatus();

#ifdef __cplusplus
}
#endif

// 内部使用的函数
namespace AIKITDLL {
	// 麦克风输入的ESR处理函数
	int esr_microphone(const char* abilityID);

	// 从文件进行ESR处理的内部实现
	int esr_file(const char* abilityID, const char* audioFilePath, int fsa_count, long* readLen);
}