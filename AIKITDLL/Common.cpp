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
    // 获取当前时间的字符串
    std::string GetCurrentTimeString() {
        time_t now = time(nullptr);
        struct tm tm_now;
        localtime_s(&tm_now, &now);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
        return std::string(buffer);
    }

    // 将日志写入文件
    void WriteToLogFile(const std::string& level, const std::string& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        std::ofstream logFile("D:\\AIKITDLL\\aikit.log", std::ios::app);
        if (logFile.is_open()) {
            logFile << GetCurrentTimeString() << " [" << level << "] " << message << std::endl;
            logFile.close();
        }
    }

    // 通用日志函数的实现
    void LogCommon(const char* level, const char* format, va_list args) {
        char buffer[4096] = {0};
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
            lastResult = "识别结果: " + resultText;
            
            // Log the result
            LogInfo("OnOutput value: %s", resultText.c_str());
            
            // Handle wakeup detection
            if (!strcmp(handle->abilityID, IVW_ABILITY) || !strcmp(handle->abilityID, CNENIVW_ABILITY)) {
                wakeupDetected = true;
                wakeupFlag = 1;  // Setting the global flag
                wakeupInfoString = resultText; // 保存唤醒词信息到全局变量
                LogInfo("唤醒词检测到: %s", resultText.c_str());
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
        lastResult = "事件: " + std::to_string(eventType);
    }

    void OnError(AIKIT_HANDLE* handle, int32_t err, const char* desc) {
        std::string errorMsg = "错误: " + std::to_string(err) + " - " + std::string(desc ? desc : "无描述");
        lastResult = errorMsg;
        
        // 同时记录到日志文件
        LogError("AIKIT错误: %d - %s", err, desc ? desc : "无描述");
    }

    // 根据错误码获取错误描述的实现
    const char* GetErrorDescription(int errorCode) {
        // 根据错误码返回对应的描述文本
        switch (errorCode) {
            case 0:
                return "成功";
            case -1:
                return "一般性错误";
            case -2:
                return "参数错误";
            case -3:
                return "授权错误";
            case -4:
                return "资源文件不存在或损坏";
            case -5:
                return "系统配置错误";
            case -6:
                return "内存分配失败";
            case -7:
                return "网络连接错误";
            case -8:
                return "数据处理错误";
            case -9:
                return "超时错误";
            case -10:
                return "引擎未初始化";
            case -11:
                return "资源加载失败";
            case -12:
                return "服务器返回错误";
            case -13:
                return "引擎初始化失败";
            case -14:
                return "引擎已初始化";
            case -15:
                return "句柄无效";
            case -16:
                return "音频格式错误";
            case -17:
                return "文件操作错误";
            case -18:
                return "数据集不存在";
            case -19:
                return "关键词未识别";
            case -20:
                return "配置文件格式错误";
            default:
                return "未知错误";
        }
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
        char pathBuffer[32768] = {0}; // 大小足够存储PATH
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
            } else {
                LogInfo("成功加载%s", dllName);
            }
        }
        
        return true;
    }
}

int RunFullTest()
{
    /**
     * 配置授权离线能力，如果需配置多个能力，可以用";"分隔开, 如"e867a88f2;ece9d3c90"
     * 语音唤醒 id = e867a88f2
     * 离线命令词识别 id = e75f07b62
     * 离线语音合成 id = e2e44feff
    */
    // 添加所有需要使用的能力ID
    const char* ability_id = "e867a88f2;e75f07b62";  // 添加语音合成能力ID
    if (strlen(ability_id) == 0)
    {
        printf("ability_id is empty!!\n");
        AIKITDLL::lastResult = "ERROR: Ability ID is empty. Please provide valid ability IDs.";
        return -1;
    }
    // 设置详细的日志输出
    AIKITDLL::LogDebug("开始初始化AIKIT SDK和语音唤醒引擎...\n");

    // 定义关键参数
    const char* appID = "80857b22";
    const char* apiSecret = "ZDkyM2RkZjcyYTZmM2VjMDM0MzVmZDJl";
    const char* apiKey = "f27d0261fb5fa1733b7e9a2190006aec";
    const char* workDir = ".";
    const char* abilityID = IVW_ABILITY; // 使用定义好的常量

    // 构建配置并完成SDK初始化
    auto config = AIKIT::AIKIT_Configurator::builder()
        .app()
        .appID(appID)
        .apiSecret(apiSecret)
        .apiKey(apiKey)
        .workDir(workDir)
        .auth()
        .authType(0)
        .ability(ability_id)
        .log()
        .logLevel(LOG_LVL_INFO)
        .logMode(2)
        .logPath("D:\\AIKITDLL\\aikit.log");

    int ret = AIKIT::AIKIT_Init();
    if (ret != 0) {
        printf("AIKIT_Init failed, error code: %d\n", ret);
        AIKITDLL::lastResult = "ERROR: AIKIT initialization failed with code: " + std::to_string(ret);
        return -1;
    }

    AIKITDLL::LogDebug("AIKIT SDK初始化成功，开始初始化语音唤醒引擎...\n");
    AIKITDLL::lastResult = "INFO: AIKIT初始化成功。";

    // 设置回调函数
    AIKIT_Callbacks cbs = { AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError };

    AIKITDLL::LogDebug("进入Ivw70Init");
    ret = Ivw70Init();
    if (ret != 0) {
        AIKIT::AIKIT_UnInit();
        return -1;
    }
    AIKITDLL::LogDebug("INFO: Voice wake-up 初始化成功");
    AIKITDLL::lastResult = "INFO: Voice wake-up module initialized successfully.";
    /*AIKITDLL::LogDebug("进入TestIvw70");
    TestIvw70(cbs);*/

    /*AIKITDLL::LogDebug("进入TestIvw70Microphone");
    TestIvw70Microphone(cbs);*/

    AIKITDLL::LogDebug("进入TestEsr");
    TestEsr(cbs);
    // 确保关闭文件资源
    if (AIKITDLL::fin != nullptr) {
        fclose(AIKITDLL::fin);
        AIKITDLL::fin = nullptr;
    }

    // 正常退出流程
    Ivw70Uninit();      //需要和Ivw70Init成对出现
    AIKITDLL::lastResult = "INFO: Voice wake-up module uninitialized.";
    AIKIT::AIKIT_UnInit();
    AIKITDLL::lastResult = "SUCCESS: Test completed successfully. All resources have been released.";
    return 0;
}

// 获取最后的结果或错误信息
#ifdef __cplusplus
extern "C" {
#endif
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
    return AIKITDLL::wakeupFlag;
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
    AIKITDLL::wakeupFlag = 0;
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
