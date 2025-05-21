#include "pch.h"
#include "EsrHelper.h"
#include "AudioManager.h" // 主要依赖AudioManager，不再直接操作recorder
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string>
#include <vector> // 用于UTF8ToLocalString
#include <unordered_map> // 如果仍有其他辅助功能需要

using namespace AIKIT;

// 定义全局结果缓冲区和相关标志
char g_pgsResultBuffer[8192] = { 0 };
bool g_hasNewPgsResult = false;
char g_htkResultBuffer[8192] = { 0 };
bool g_hasNewHtkResult = false;
char g_plainResultBuffer[8192] = { 0 };
bool g_hasNewPlainResult = false;
char g_vadResultBuffer[8192] = { 0 };
bool g_hasNewVadResult = false;
char g_readableResultBuffer[8192] = { 0 };
bool g_hasNewReadableResult = false;

// 定义临界区对象和初始化标志
CRITICAL_SECTION g_resultLock;
bool g_resultLockInitialized = false;

// UTF8ToLocalString 函数保持不变，用于日志输出等
std::string UTF8ToLocalString(const char* utf8Str) {
	if (!utf8Str) return "";

	int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
	if (wlen <= 0) {
		AIKITDLL::LogError("UTF8转换失败，错误码: %d", GetLastError());
		return "";
	}

	std::vector<wchar_t> wstr(wlen);
	if (MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wstr.data(), wlen) <= 0) {
		AIKITDLL::LogError("UTF8转宽字符失败，错误码: %d", GetLastError());
		return "";
	}

	int len = WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, NULL, 0, NULL, NULL); // 添加了最后两个NULL参数
	if (len <= 0) {
		AIKITDLL::LogError("宽字符转本地编码失败（计算长度时），错误码: %d", GetLastError());
		return "";
	}

	std::vector<char> str(len);
	if (WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, str.data(), len, NULL, NULL) <= 0) { // 添加了最后两个NULL参数
		AIKITDLL::LogError("宽字符转本地编码失败（转换时），错误码: %d", GetLastError());
		return "";
	}

	return std::string(str.data());
}

// InitResultLock, AddResult, ProcessRecognitionResult 保持不变，由OnOutput回调使用
// 初始化临界区锁
void InitResultLock() {
	if (!g_resultLockInitialized) {
		__try {
			InitializeCriticalSection(&g_resultLock);
			g_resultLockInitialized = true;
			AIKITDLL::LogDebug("EsrHelper: 临界区初始化成功");
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			AIKITDLL::LogError("EsrHelper: 临界区初始化失败");
		}
	}
}

// 添加结果到对应缓冲区
void AddResult(const char* type, const char* result) {
	if (!type || !result || strlen(result) == 0) return;
	if (!g_resultLockInitialized) InitResultLock(); //确保初始化

	EnterCriticalSection(&g_resultLock);

	char* targetBuffer = nullptr;
	bool* newResultFlag = nullptr;
	size_t bufferSize = 8192;
	std::string prefix = std::string(type) + ": ";

	if (strcmp(type, "pgs") == 0) { targetBuffer = g_pgsResultBuffer; newResultFlag = &g_hasNewPgsResult; }
	else if (strcmp(type, "htk") == 0) { targetBuffer = g_htkResultBuffer; newResultFlag = &g_hasNewHtkResult; }
	else if (strcmp(type, "plain") == 0) { targetBuffer = g_plainResultBuffer; newResultFlag = &g_hasNewPlainResult; }
	else if (strcmp(type, "vad") == 0) { targetBuffer = g_vadResultBuffer; newResultFlag = &g_hasNewVadResult; }
	else if (strcmp(type, "readable") == 0) { targetBuffer = g_readableResultBuffer; newResultFlag = &g_hasNewReadableResult; }

	if (targetBuffer && newResultFlag) {
		// 清理旧结果或实现滚动逻辑 (简化为直接覆盖)
		memset(targetBuffer, 0, bufferSize);
		strcpy_s(targetBuffer, bufferSize, prefix.c_str());
		strcat_s(targetBuffer, bufferSize, result);
		*newResultFlag = true;
	}

	LeaveCriticalSection(&g_resultLock);
}

// 判断结果类型并存储
void ProcessRecognitionResult(const char* key, const char* value) {
	if (!key || !value) return;
	AddResult(key, value);
	// AIKITDLL::LogInfo("EsrHelper 识别结果: %s: %s", key, value); // 日志可在此处或OnOutput中记录
}

// 导出给CnenEsrWrapper.cpp使用的函数，用于从缓冲区获取结果
extern "C" __declspec(dllexport) int GetPlainResult(char* buffer, int bufferSize, bool* isNewResult) {
    if (!g_resultLockInitialized) InitResultLock();
    EnterCriticalSection(&g_resultLock);
    int len = 0;
    if (g_hasNewPlainResult && buffer && bufferSize > 0) {
        strncpy_s(buffer, bufferSize, g_plainResultBuffer, _TRUNCATE);
        buffer[bufferSize - 1] = '\0';
        len = strlen(buffer);
        g_hasNewPlainResult = false; // 标记为已读取
        if (isNewResult) *isNewResult = true;
    }
    else {
        if (buffer && bufferSize > 0) buffer[0] = '\0';
        if (isNewResult) *isNewResult = false;
    }
    LeaveCriticalSection(&g_resultLock);
    return len;
}

// 其他 Get<Type>Result 函数可以类似地实现，如果需要的话
// 例如 GetPgsResult, GetHtkResult 等