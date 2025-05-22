#include "pch.h"
#include "Common.h"
#include "IvwWrapper.h"
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
#include "CnenEsrWrapper.h" // 假设包含 ESR_ABILITY 和 ESR 相关全局变量声明
#include "EsrHelper.h"      // 假设包含 ProcessRecognitionResult 和相关全局变量声明
#include <aikit_biz_type.h> // 确保 AIKIT_OutputData_Status_* 常量可用

// 添加宏定义
#define FRAME_LEN 640 // 16k采样率的16bit音频，一帧的大小为640B, 时长20ms

// 全局变量声明
int wakeupFlag = 0; // 唤醒状态标志，0表示未唤醒，1表示已唤醒
namespace AIKITDLL {
	bool isInitialized = true;
	std::string lastResult;
	std::atomic_bool wakeupDetected(false);  // 是否检测到唤醒词
	std::mutex logMutex; // 用于日志写入的互斥锁
	FILE* fin = nullptr;
	std::string wakeupInfoString; // 存储唤醒详细信息的字符串
	std::string GetCurrentTimeString() {
		time_t now = time(nullptr);
		struct tm tm_now;
		localtime_s(&tm_now, &now);
		char buffer[80];
		strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
		return std::string(buffer);
	}

	void AIKITDLL::OnOutput(AIKIT_HANDLE* handle, const AIKIT_OutputData* output) {
		if (!handle || !output || !output->node) {
			LogError("OnOutput received invalid parameters: handle=%p", static_cast<void*>(handle));
			if (output && !output->node) LogError("Output node is null.");
			return;
		}

		LogInfo("OnOutput abilityID: %s, status: %d", handle->abilityID, output->node->status);
		if (output->node->key) {
			LogInfo("OnOutput key: %s", output->node->key);
		}

		if (output->node->value) {
			std::string resultText(static_cast<char*>(output->node->value), output->node->len);
			LogInfo("OnOutput value (len %lu, status %d): %s", output->node->len, output->node->status, resultText.c_str());

			// --- IVW (唤醒) 处理 ---
			if (!strcmp(handle->abilityID, IVW_ABILITY) || !strcmp(handle->abilityID, CNENIVW_ABILITY)) {
				if (output->node->status == AIKIT_DataEnd || output->node->status == AIKIT_DataOnce) {
					if (output->node->len > 0) { // 确保有实际数据
						AIKITDLL::wakeupDetected = true;
						::wakeupFlag = 1;
						AIKITDLL::wakeupInfoString = resultText;
						AIKITDLL::lastResult = "唤醒结果: " + resultText;
						LogInfo("唤醒词检测到 (status %d): %s", output->node->status, resultText.c_str());
					} else {
						AIKITDLL::wakeupDetected = false;
						// ::wakeupFlag 保持或应为0
						AIKITDLL::wakeupInfoString = "";
						AIKITDLL::lastResult = "唤醒结果: 无效 (空值, status " + std::to_string(output->node->status) + ")";
						LogWarning("唤醒引擎: 最终/单次结果包 (status %d) 值为空或长度为0. 非成功唤醒.", output->node->status);
					}
				} else if (output->node->status == AIKIT_DataBegin || output->node->status == AIKIT_DataContinue) {
					LogInfo("唤醒引擎: 中间数据包 (status %d): %s", output->node->status, resultText.c_str());
					// 中间包不设置唤醒成功状态
				}
			}
			// --- ESR (命令词识别) 处理 ---
			else if (!strcmp(handle->abilityID, AIKITDLL::ESR_ABILITY_ID)) {
				LogInfo("命令词引擎原始输出 (status %d): %s", output->node->status, resultText.c_str());

				if (output->node->status == AIKIT_DataEnd || output->node->status == AIKIT_DataOnce) {
					LogInfo("命令词最终/单次结果包 (status %d)", output->node->status);
					// 假设 EsrHelper::ProcessRecognitionResult 会处理 resultText 并设置 g_hasNewReadableResult
					// EsrHelper::ProcessRecognitionResult(resultText.c_str()); // 如果需要调用

					if (g_hasNewReadableResult) {
						AIKITDLL::esrStatus = AIKITDLL::ESR_STATUS_SUCCESS_INTERNAL;
						AIKITDLL::lastEsrKeywordResult = std::string(g_readableResultBuffer);
						AIKITDLL::lastEsrErrorInfo = "";
						AIKITDLL::lastResult = "命令词识别: " + AIKITDLL::lastEsrKeywordResult;
						LogInfo("命令词识别成功 (EsrHelper): %s", AIKITDLL::lastEsrKeywordResult.c_str());
						g_hasNewReadableResult = false; // 读取后重置标志
					} else {
						AIKITDLL::esrStatus = AIKITDLL::ESR_STATUS_NO_MATCH_INTERNAL;
						AIKITDLL::lastEsrKeywordResult = "未匹配";
						AIKITDLL::lastEsrErrorInfo = ""; // 没有SDK错误，只是未匹配
						AIKITDLL::lastResult = "命令词结果: 未匹配";
						LogInfo("命令词最终/单次结果: 未匹配 (EsrHelper g_hasNewReadableResult is false, status %d)", output->node->status);
					}
				} else if (output->node->status == AIKIT_DataBegin || output->node->status == AIKIT_DataContinue) {
					// AIKITDLL::esrStatus 应该由调用方设置为 ESR_STATUS_PROCESSING_INTERNAL
					AIKITDLL::lastResult = "命令词中间结果: " + resultText;
					LogInfo("命令词中间结果 (status %d): %s", output->node->status, resultText.c_str());
				} else {
					LogWarning("命令词引擎: 未知或未处理的 output->node->status: %d", output->node->status);
				}
			}
			// --- 其他能力处理 ---
			else {
				AIKITDLL::lastResult = "未知能力 (" + std::string(handle->abilityID) + ") 结果 (status " + std::to_string(output->node->status) + "): " + resultText;
				LogWarning("OnOutput: 未处理的 abilityID: %s, status: %d, 结果: %s", handle->abilityID, output->node->status, resultText.c_str());
			}

			if (AIKITDLL::fin != nullptr) {
				fwrite(output->node->value, sizeof(char), output->node->len, AIKITDLL::fin);
			}

		} else { // output->node->value 为 NULL
			std::string statusStr = "未知";
			switch (output->node->status) {
				case AIKIT_DataBegin:    statusStr = "Begin"; break;
				case AIKIT_DataContinue: statusStr = "Continue"; break;
				case AIKIT_DataEnd:      statusStr = "End"; break;
				case AIKIT_DataOnce:     statusStr = "Once"; break;
			}
			LogWarning("OnOutput received output with NULL value. Key: %s, Status: %d (%s)",
				output->node->key ? output->node->key : "N/A",
				output->node->status, statusStr.c_str());

			if (output->node->status == AIKIT_DataEnd || output->node->status == AIKIT_DataOnce) {
				if (!strcmp(handle->abilityID, AIKITDLL::ESR_ABILITY_ID)) {
					if (AIKITDLL::esrStatus != AIKITDLL::ESR_STATUS_SUCCESS_INTERNAL) { // 避免覆盖可能的多包最终结果
						AIKITDLL::esrStatus = AIKITDLL::ESR_STATUS_NO_MATCH_INTERNAL;
						AIKITDLL::lastEsrKeywordResult = "未匹配 (空结果)";
						AIKITDLL::lastEsrErrorInfo = "最终结果包为空";
						AIKITDLL::lastResult = "命令词结果: 未匹配 (空结果)";
						LogWarning("命令词引擎: 最终/单次结果包 (status %d) 为空.", output->node->status);
					}
				} else if (!strcmp(handle->abilityID, IVW_ABILITY) || !strcmp(handle->abilityID, CNENIVW_ABILITY)) {
					AIKITDLL::wakeupDetected = false; // 确保唤醒失败
					// ::wakeupFlag 应该为0
					AIKITDLL::wakeupInfoString = "";
					AIKITDLL::lastResult = "唤醒结果: 失败 (空结果, status " + std::to_string(output->node->status) + ")";
					LogError("唤醒引擎: 最终/单次结果包 (status %d) 为空. 唤醒失败.", output->node->status);
				} else {
					AIKITDLL::lastResult = "未知能力 (" + std::string(handle->abilityID) + ") 结果: 空 (status " + std::to_string(output->node->status) + ")";
					LogWarning("OnOutput: 未知能力 %s, 最终/单次结果包 (status %d) 为空.", handle->abilityID, output->node->status);
				}
			}
		}
	}

	void OnEvent(AIKIT_HANDLE* handle, AIKIT_EVENT eventType, const AIKIT_OutputEvent* eventValue) {
		std::string eventMsg = "事件类型: " + std::to_string(eventType);
		std::string abilityID_str = (handle && handle->abilityID) ? handle->abilityID : "N/A";
		eventMsg += ", AbilityID: " + abilityID_str;

		lastResult = eventMsg; // 更新 lastResult，以便外部获取最新事件信息
		LogInfo("AIKIT事件: %s", eventMsg.c_str()); // 记录详细事件信息
	}

	void OnError(AIKIT_HANDLE* handle, int32_t err, const char* desc) {
		std::string errorDesc = desc ? desc : "无描述";
		std::string abilityID_str = (handle && handle->abilityID) ? handle->abilityID : "N/A";
		std::string errorMsg = "错误: " + std::to_string(err) + " (" + errorDesc + "), AbilityID: " + abilityID_str;
		
		lastResult = errorMsg;
		LogError("AIKIT错误: %s", errorMsg.c_str());

		if (handle && handle->abilityID) {
			if (!strcmp(handle->abilityID, AIKITDLL::ESR_ABILITY_ID)) {
				AIKITDLL::esrStatus = AIKITDLL::ESR_STATUS_FAILED_INTERNAL;
				AIKITDLL::lastEsrErrorInfo = errorMsg;
				AIKITDLL::lastEsrKeywordResult = "";
			} else if (!strcmp(handle->abilityID, IVW_ABILITY) || !strcmp(handle->abilityID, CNENIVW_ABILITY)) {
				AIKITDLL::wakeupDetected = false;
				// ::wakeupFlag 应由C#侧逻辑结合 GetWakeupStatus 和 GetLastResult 来管理是否重置为0
				AIKITDLL::wakeupInfoString = ""; // 清除可能存在的旧信息
			}
		}
	}

	// 将日志写入文件
	void WriteToLogFile(const std::string& level, const std::string& message) {
		std::lock_guard<std::mutex> lock(logMutex);
		std::ofstream logFile("C:\\AIKITDLL\\aikit_wpf.log", std::ios::app);
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
		const char* libsDir = "C:\\AIKITDLL\\libs\\64";
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
	// AIKITDLL_API 在DLL内部编译时应为 dllexport
	// 如果 AIKITDLL_EXPORTS 定义正确，则 AIKITDLL_API 会展开为 __declspec(dllexport)
	// 如果仍然报错，请检查项目预处理器定义中是否包含 AIKITDLL_EXPORTS
	AIKITDLL_API const char* GetLastResult()
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
	AIKITDLL_API int GetWakeupStatus()
	{
		return ::wakeupFlag; // wakeupFlag 是全局的
	}
#ifdef __cplusplus
}
#endif

// 重置唤醒词状态
#ifdef __cplusplus
extern "C" {
#endif
	AIKITDLL_API void ResetWakeupStatus()
	{
		::wakeupFlag = 0; // wakeupFlag 是全局的
		AIKITDLL::wakeupDetected = false;
	}
#ifdef __cplusplus
}
#endif

// 获取唤醒词详细信息
#ifdef __cplusplus
extern "C" {
#endif
	AIKITDLL_API const char* GetWakeupInfoString()
	{
		return AIKITDLL::wakeupInfoString.c_str();
	}
#ifdef __cplusplus
}
#endif