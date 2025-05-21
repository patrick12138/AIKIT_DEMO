#pragma once

#include "aikit_biz_api.h"
#include "aikit_biz_config.h"
#include "Common.h"

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

	// 声明全局结果缓冲区和相关标志
	extern AIKITDLL_API char g_pgsResultBuffer[8192];
	extern AIKITDLL_API bool g_hasNewPgsResult;
	extern AIKITDLL_API char g_htkResultBuffer[8192];
	extern AIKITDLL_API bool g_hasNewHtkResult;
	extern AIKITDLL_API char g_plainResultBuffer[8192];
	extern AIKITDLL_API bool g_hasNewPlainResult;
	extern AIKITDLL_API char g_vadResultBuffer[8192];
	extern AIKITDLL_API bool g_hasNewVadResult;
	extern AIKITDLL_API char g_readableResultBuffer[8192];
	extern AIKITDLL_API bool g_hasNewReadableResult;

	// 声明临界区对象和初始化标志
	extern AIKITDLL_API CRITICAL_SECTION g_resultLock;
	extern AIKITDLL_API bool g_resultLockInitialized;

	// 初始化结果锁 (应在SDK初始化时调用)
	AIKITDLL_API void InitResultLock();

	// 获取plain格式的识别结果
	// 这个函数被CnenEsrWrapper.cpp中的旧esr_microphone循环使用，
	// 在新的回调模型下，C#层应该通过GetEsrResult和GetEsrStatus来获取最终结果。
	// 如果仍然需要在C++内部某个地方轮询最新plain结果，可以保留，否则可以考虑移除。
	// 暂时保留，以防某些内部逻辑仍然依赖它。
	AIKITDLL_API int GetPlainResult(char* buffer, int bufferSize, bool* isNewResult);

#ifdef __cplusplus
}
#endif