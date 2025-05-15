#pragma once
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace AIKITDLL {
    // 全局互斥量，用于保护IVW会话创建和资源管理
    extern std::mutex g_ivwMutex;
    
    // 当前唤醒会话状态标志
    extern std::atomic<bool> g_ivwSessionActive;
    
    // 用于同步唤醒事件的条件变量
    extern std::condition_variable g_wakeupCond;
    extern std::mutex g_wakeupMutex;
    
    // 初始化IVW互斥资源
    void InitIvwResources();
    
    // 释放IVW互斥资源
    void CleanupIvwResources();
    
    // 通知唤醒事件发生
    void NotifyWakeupDetected();
    
    // 等待唤醒事件，带超时
    bool WaitForWakeup(int timeoutMs);
}
