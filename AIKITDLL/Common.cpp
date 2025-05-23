#include "pch.h"
#include "Common.h"
#include "IvwWrapper.h"
#include "VoiceStateManager.h"
#include <string.h>
#include <aikit_constant.h>
#include <Windows.h>
#include <fstream>
#include <atomic>
#include <assert.h>
#include <cstring>
#include <cstdarg>
#include <time.h>
#include <mutex>

// 添加宏定义
#define FRAME_LEN 640 // 16k采样率的16bit音频，一帧的大小为640B, 时长20ms

// 注意：wakeupFlag 已移到 AIKITDLL 命名空间内 (定义在 IvwWrapper.cpp)
namespace AIKITDLL {
	bool isInitialized = true;
	std::string lastResult;
	std::atomic_bool wakeupDetected{ false };  // 是否检测到唤醒词
	std::mutex logMutex; // 用于日志写入的互斥锁
	FILE* fin = nullptr;
	std::string wakeupInfoString; // 存储唤醒详细信息的字符串
	AIKIT_EVENT_TYPE lastEventType = EVENT_UNKNOWN; // 最近一次事件类型
	
	std::string GetCurrentTimeString() {
		time_t now = time(nullptr);
		struct tm tm_now;
		localtime_s(&tm_now, &now);
		char buffer[80];
		strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
		return std::string(buffer);
	}
	
	void OnOutput(AIKIT_HANDLE* handle, const AIKIT_OutputData* output) {
		if (!handle || !output || !output->node) {
			LogError("OnOutput received invalid parameters");
			return;
		}

		// Log the output
		LogInfo("OnOutput abilityID: %s", handle->abilityID);

		if (output->node->key) {
			LogInfo("OnOutput key: %s", output->node->key);
		}
		if (output->node->value) {
			// Store result for WPF display
			std::string resultText = std::string((char*)output->node->value);
			lastResult = "识别结果: " + resultText;			// Log the result
			LogInfo("OnOutput value: %s", resultText.c_str());

			// Handle wakeup detection
			if (!strcmp(handle->abilityID, IVW_ABILITY) || !strcmp(handle->abilityID, CNENIVW_ABILITY)) {
				// 添加线程安全保护
				std::lock_guard<std::mutex> lock(g_ivwMutex);
				// 确认是否包含关键词
				if (strstr(resultText.c_str(), "keyword")) {
					wakeupDetected.store(true);
					wakeupFlag.store(1);  // Setting the global flag
					wakeupInfoString = resultText; // 保存唤醒词信息到全局变量
					LogInfo("唤醒词检测到: %s", resultText.c_str());

					// 设置唤醒成功事件类型
					lastEventType = EVENT_WAKEUP_SUCCESS;

					// 发出唤醒通知
					NotifyWakeupDetected();

					// 通知状态管理器
					if (VoiceStateManager::GetInstance()) {
						VoiceStateManager::GetInstance()->HandleEvent(EVENT_WAKEUP_SUCCESS, resultText.c_str());
					}
				}
			}
			else if (strstr(handle->abilityID, "e75f07b62")) {  // ESR相关的abilityID
				// 设置命令词识别成功事件类型
				lastEventType = EVENT_ESR_SUCCESS;

				// 通知状态管理器
				if (VoiceStateManager::GetInstance()) {
					VoiceStateManager::GetInstance()->HandleEvent(EVENT_ESR_SUCCESS, resultText.c_str());
				}
			}

			// If we have an open file, write to it
			if (fin != nullptr) {
				fwrite(output->node->value, sizeof(char), output->node->len, fin);
			}

			// Add final result indicator if applicable
			if (output->node->status == 2) {
				lastResult += " (最终结果)";
				LogInfo("结果状态: 最终结果");
			}
		}
	}

	void OnEvent(AIKIT_HANDLE* handle, AIKIT_EVENT eventType, const AIKIT_OutputEvent* eventValue) {
		// 记录事件信息
		LogInfo("OnEvent abilityID: %s, eventType: %d", handle ? handle->abilityID : "NULL", eventType);

		// 尝试从eventValue中获取信息
		const char* eventMsg = "";
		if (eventValue && eventValue->node) {
			eventMsg = (const char*)(eventValue->node->value);
			LogInfo("OnEvent details: %s", eventMsg);
		}

		// 检查是否是唤醒相关事件
		if (handle && (!strcmp(handle->abilityID, IVW_ABILITY) || !strcmp(handle->abilityID, CNENIVW_ABILITY))) {
			// 添加线程安全保护
			std::lock_guard<std::mutex> lock(g_ivwMutex);

			// 处理具体事件类型
			if (eventType == AIKIT_Event_Error) {
				// 唤醒出错，设置失败事件
				lastEventType = EVENT_WAKEUP_FAILED;

				// 通知状态管理器
				if (VoiceStateManager::GetInstance()) {
					VoiceStateManager::GetInstance()->HandleEvent(EVENT_WAKEUP_FAILED, eventMsg);
				}
			}
		}

		// 根据不同事件类型处理
		AIKIT_EVENT_TYPE parsedEventType = EVENT_UNKNOWN;
		// 解析事件类型
		if (handle && handle->abilityID) {
			if (!strcmp(handle->abilityID, IVW_ABILITY) || !strcmp(handle->abilityID, CNENIVW_ABILITY)) {
				// 唤醒相关事件
				if (eventType == AIKIT_Event_End && wakeupDetected.load()) {
					parsedEventType = EVENT_WAKEUP_SUCCESS;
					LogInfo("唤醒成功事件");
				}
				else if (eventType == AIKIT_Event_Error) {
					parsedEventType = EVENT_WAKEUP_FAILED;
					LogError("唤醒失败事件");
				}
			}
			else if (strstr(handle->abilityID, "e75f07b62")) {  // ESR相关事件
				if (eventType == AIKIT_Event_End) {
					parsedEventType = EVENT_ESR_SUCCESS;
					LogInfo("命令词识别成功事件");
				}
				else if (eventType == AIKIT_Event_Error) {
					parsedEventType = EVENT_ESR_FAILED;
					LogError("命令词识别失败事件");
				}
				else if (eventType == AIKIT_Event_VadEnd) {
					// VAD检测到语音结束
					LogInfo("VAD检测到语音结束");
				}
			}
		}

		// 更新最后的事件类型
		lastEventType = parsedEventType;
		// 如果集成了VoiceStateManager，通知状态变化
		if (parsedEventType != EVENT_UNKNOWN) {
			// 将事件通知给VoiceStateManager
			if (VoiceStateManager::GetInstance()) {
				const char* resultText = eventMsg;
				VoiceStateManager::GetInstance()->HandleEvent(parsedEventType, resultText);
			}

			lastResult = "事件: " + std::to_string((int)parsedEventType) + " - " + eventMsg;
		}
		else {
			lastResult = "事件: " + std::to_string(eventType);
		}
	}

	// 事件类型解析函数
	AIKIT_EVENT_TYPE ParseEventType(const char* key, const char* value) {
		if (!key || !value) {
			return EVENT_UNKNOWN;
		}

		// 根据关键字和值来判断事件类型
		if (strstr(key, "wake") || strstr(key, "ivw")) {
			return EVENT_WAKEUP_SUCCESS;
		}
		else if (strstr(key, "error") || strstr(key, "fail")) {
			// 根据内容进一步判断是唤醒失败还是识别失败
			if (strstr(value, "wake") || strstr(value, "ivw")) {
				return EVENT_WAKEUP_FAILED;
			}
			else {
				return EVENT_ESR_FAILED;
			}
		}
		else if (strstr(key, "esr") || strstr(key, "recognize") || strstr(key, "result")) {
			return EVENT_ESR_SUCCESS;
		}

		return EVENT_UNKNOWN;
	}

	void OnError(AIKIT_HANDLE* handle, int32_t err, const char* desc) {
		std::string errorMsg = "错误: " + std::to_string(err) + " - " + std::string(desc ? desc : "无描述");
		lastResult = errorMsg;

		// 同时记录到日志文件
		LogError("AIKIT错误: %d - %s", err, desc ? desc : "无描述");
	}

	// 将日志写入文件
	void WriteToLogFile(const std::string& level, const std::string& message) {
		std::lock_guard<std::mutex> lock(logMutex);
		std::ofstream logFile("D:\\AIKITDLL\\aikit_wpf_demo.log", std::ios::app);
		if (logFile.is_open()) {
			logFile << GetCurrentTimeString() << " [" << level << "] " << message << std::endl;
			logFile.close();
		}
	}

	// 通用日志函数的实现
	void LogCommon(const char* level, const char* format, va_list args) {
		char buffer[4096] = { 0 };
		vsnprintf(buffer, sizeof(buffer) - 1, format, args);

		// 在控制台输出
		printf("[%s] %s\n", level, buffer);

		// 同时写入日志文件
		WriteToLogFile(level, buffer);

		// 更新最后结果
		lastResult = std::string(level) + ": " + buffer;
	}

	// 信息级别日志
	void LogInfo(const char* format, ...) {
		va_list args;
		va_start(args, format);
		LogCommon("INFO", format, args);
		va_end(args);
	}

	// 错误级别日志
	void LogError(const char* format, ...) {
		va_list args;
		va_start(args, format);
		LogCommon("ERROR", format, args);
		va_end(args);
	}

	// 警告级别日志
	void LogWarning(const char* format, ...) {
		va_list args;
		va_start(args, format);
		LogCommon("WARNING", format, args);
		va_end(args);
	}

	// 调试级别日志
	void LogDebug(const char* format, ...) {
		va_list args;
		va_start(args, format);
		LogCommon("DEBUG", format, args);
		va_end(args);
	}

	// 加载引擎所需的动态库
	bool EnsureEngineDllsLoaded() {
		// 获取当前程序架构
#ifdef _WIN64
		const char* libsDir = "D:\\AIKITDLL\\libs\\64";
#else
		const char* libsDir = ".\\libs\\32";
#endif

		LogInfo("正在加载引擎动态库，目录: %s", libsDir);

		// 添加DLL搜索路径
		if (!SetDllDirectoryA(libsDir)) {
			LogError("设置DLL搜索路径失败，错误码: %d", GetLastError());
			return false;
		}

		// 获取当前环境变量PATH
		char pathBuffer[32768] = { 0 }; // 大小足够存储PATH
		if (GetEnvironmentVariableA("PATH", pathBuffer, sizeof(pathBuffer)) > 0) {
			// 添加库路径到PATH
			std::string newPath = std::string(libsDir) + ";" + pathBuffer;
			if (!SetEnvironmentVariableA("PATH", newPath.c_str())) {
				LogWarning("更新PATH环境变量失败，错误码: %d", GetLastError());
				// 继续执行，因为SetDllDirectory可能已经足够
			}
		}

		// 预加载关键DLL文件
		std::string aeeLibPath = std::string(libsDir) + "\\AEE_lib.dll";
		HMODULE hAeeLib = LoadLibraryExA(aeeLibPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (!hAeeLib) {
			LogError("加载AEE_lib.dll失败，错误码: %d", GetLastError());
			return false;
		}
		LogInfo("成功加载AEE_lib.dll");

		// 尝试加载语音唤醒相关的DLL
		const char* ivwDllNames[] = {
			"eabb2f029_v10092_aee.dll",
			"ef7d69542_v1014_aee.dll"
		};

		for (const char* dllName : ivwDllNames) {
			std::string dllPath = std::string(libsDir) + "\\" + dllName;
			HMODULE hDll = LoadLibraryExA(dllPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
			if (!hDll) {
				LogWarning("加载%s失败，错误码: %d", dllName, GetLastError());
				// 继续尝试加载其他DLL
			}
			else {
				LogInfo("成功加载%s", dllName);
			}
		}

		return true;
	}
}


// 获取最后的结果或错误信息
#ifdef __cplusplus
extern "C" {
#endif
	const char* GetLastResult()
	{
		return AIKITDLL::lastResult.c_str();
	}
#ifdef __cplusplus
}
#endif

// 获取唤醒词状态
#ifdef __cplusplus
extern "C" {
#endif
	int GetWakeupStatus()
	{
		// 添加锁保护，确保线程安全
		std::lock_guard<std::mutex> lock(AIKITDLL::g_ivwMutex);

		// 首先检查全局标志
		if (AIKITDLL::wakeupFlag.load() == 1) {
			return 1;
		}
		// 检查最后一次事件类型
		if (AIKITDLL::lastEventType == EVENT_WAKEUP_SUCCESS) {
			// 如果检测到了唤醒事件，主动设置标志
			AIKITDLL::wakeupFlag.store(1);
			AIKITDLL::LogInfo("GetWakeupStatus: 检测到唤醒事件，主动设置唤醒标志");
			return 1;
		}

		// 检查唤醒信息字符串
		if (!AIKITDLL::wakeupInfoString.empty() && strstr(AIKITDLL::wakeupInfoString.c_str(), "keyword")) {
			// 如果唤醒信息中包含关键词，主动设置标志
			AIKITDLL::wakeupFlag.store(1);
			AIKITDLL::LogInfo("GetWakeupStatus: 检测到唤醒词信息，主动设置唤醒标志");
			return 1;
		}

		return 0;
	}
#ifdef __cplusplus
}
#endif

// 重置唤醒词状态
#ifdef __cplusplus
extern "C" {
#endif
	void ResetWakeupStatus()
	{
		// 使用线程锁保护状态重置
		std::lock_guard<std::mutex> lock(AIKITDLL::g_ivwMutex);

		// 重置所有与唤醒相关的状态
		AIKITDLL::wakeupFlag.store(0);
		AIKITDLL::wakeupDetected.store(false);
		AIKITDLL::wakeupInfoString.clear();  // 清空唤醒信息字符串

		// 重置最后事件类型（如果当前是唤醒成功状态）
		if (AIKITDLL::lastEventType == EVENT_WAKEUP_SUCCESS) {
			AIKITDLL::lastEventType = EVENT_NONE;
		}

		AIKITDLL::LogInfo("ResetWakeupStatus: 已重置所有唤醒状态标志");
	}
#ifdef __cplusplus
}
#endif

// 获取唤醒词详细信息
#ifdef __cplusplus
extern "C" {
#endif
	const char* GetWakeupInfoString()
	{
		return AIKITDLL::wakeupInfoString.c_str();
	}
#ifdef __cplusplus
}
#endif
