#include "EsrUtils.h"
#include <windows.h>
#include <vector>
#include "../Common.h"
#include "../ResultBufferManager.h"

std::string UTF8ToLocalString(const char* utf8Str) {
    if (!utf8Str) return "";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (wlen <= 0) return "";
    std::vector<wchar_t> wstr(wlen);
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wstr.data(), wlen) <= 0) return "";
    int len = WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::vector<char> str(len);
    if (WideCharToMultiByte(CP_ACP, 0, wstr.data(), -1, str.data(), len, NULL, NULL) <= 0) return "";
    return std::string(str.data());
}

void ProcessRecognitionResult(const char* key, const char* value) {
    if (!key || !value) return;
    // 这里只做简单日志，实际可调用ResultBufferManager等
    std::string localKey = UTF8ToLocalString(key);
    std::string localValue = UTF8ToLocalString(value);
    AIKITDLL::LogInfo("识别结果: %s: %s", key, value);
}
