#include "pch.h"
#include "Common.h"
#include "IvwWrapper.h"
#include "VoiceStateManager.h"

// 专用于调试的函数，获取唤醒状态的详细信息
#ifdef __cplusplus
extern "C" {
#endif

AIKITDLL_API const char* GetWakeupStatusDetails()
{
    // 使用线程锁保护状态读取
    std::lock_guard<std::mutex> lock(AIKITDLL::g_ivwMutex);
      // 构建包含所有状态的详细信息字符串
    static char statusDetails[2048];
    snprintf(statusDetails, sizeof(statusDetails),
        "WakeupFlag: %d\n"
        "WakeupDetected: %s\n"
        "LastEventType: %d\n"
        "WakeupInfoString: %s\n"
        "VoiceStateManager_State: %d",
        AIKITDLL::wakeupFlag.load(),
        AIKITDLL::wakeupDetected.load() ? "true" : "false",
        AIKITDLL::lastEventType,
        AIKITDLL::wakeupInfoString.empty() ? "(empty)" : AIKITDLL::wakeupInfoString.c_str(),
        VoiceStateManager::GetInstance() ? VoiceStateManager::GetInstance()->GetCurrentState() : -1
    );
    
    AIKITDLL::LogInfo("GetWakeupStatusDetails 被调用，返回状态: %s", statusDetails);
    return statusDetails;
}

#ifdef __cplusplus
}
#endif
