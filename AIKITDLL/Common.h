#pragma once

#include "aikit_biz_api.h"
#include "aikit_biz_config.h"
#include <string>
#include "CnenEsrWrapper.h"

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

	// 初始化SDK并配置回调函数
	AIKITDLL_API int InitializeSDK(const char* appID, const char* apiKey, const char* apiSecret, const char* workDir);

	// 清理SDK资源
	AIKITDLL_API void CleanupSDK();

#ifdef __cplusplus
}
#endif

// 定义事件类型
enum AIKIT_EVENT_TYPE {
    EVENT_UNKNOWN = 0,
    EVENT_WAKEUP_SUCCESS = 1,   // 唤醒成功
    EVENT_WAKEUP_FAILED = 2,    // 唤醒失败
    EVENT_ESR_SUCCESS = 3,      // 命令词识别成功
    EVENT_ESR_FAILED = 4,       // 命令词识别失败
    EVENT_ESR_TIMEOUT = 5       // 命令词识别超时
};

// 内部使用的函数和变量
namespace AIKITDLL {
	// 是否已经初始化
	extern bool isInitialized;

	// 最后的结果或错误信息
	extern std::string lastResult;
	
	// 最后的事件类型
	extern AIKIT_EVENT_TYPE lastEventType;

	// 回调函数
	void OnOutput(AIKIT_HANDLE* handle, const AIKIT_OutputData* output);
	void OnEvent(AIKIT_HANDLE* handle, AIKIT_EVENT eventType, const AIKIT_OutputEvent* eventValue);
	void OnError(AIKIT_HANDLE* handle, int32_t err, const char* desc);
	
	// 事件类型识别函数
	AIKIT_EVENT_TYPE ParseEventType(const char* key, const char* value);

	// 日志记录函数 - 同时写入控制台和日志文件
	void LogInfo(const char* format, ...);
	void LogError(const char* format, ...);
	void LogWarning(const char* format, ...);
	void LogDebug(const char* format, ...);

	// 引擎初始化支持函数
	bool EnsureEngineDllsLoaded(); // 确保引擎DLL被正确加载
	bool ValidateEngineResources(const char* resourcePath); // 验证引擎资源文件完整性
}