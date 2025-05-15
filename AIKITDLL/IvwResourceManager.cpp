#include "pch.h"
#include "IvwResourceManager.h"
#include "Common.h"
#include "IvwWrapper.h"

namespace AIKITDLL {
    // 全局互斥量实现
    std::mutex g_ivwMutex;
    
    // 当前唤醒会话状态标志实现
    std::atomic<bool> g_ivwSessionActive(false);
    
    // 唤醒事件同步变量
    std::condition_variable g_wakeupCond;
    std::mutex g_wakeupMutex;
    
    // 初始化IVW互斥资源
    void InitIvwResources() {
        // 标记会话为非活动状态
        g_ivwSessionActive.store(false);
    }
    
    // 释放IVW互斥资源
    void CleanupIvwResources() {
        // 标记会话为非活动状态
        g_ivwSessionActive.store(false);
    }
    
    // 通知唤醒事件发生
    void NotifyWakeupDetected() {
        std::lock_guard<std::mutex> lock(g_wakeupMutex);
        // 记录通知事件
        LogInfo("NotifyWakeupDetected: 通知唤醒事件发生");
        g_wakeupCond.notify_all();
    }
    
    // 等待唤醒事件，带超时
    bool WaitForWakeup(int timeoutMs) {
        std::unique_lock<std::mutex> lock(g_wakeupMutex);
        LogInfo("WaitForWakeup: 开始等待唤醒事件，超时时间: %d ms", timeoutMs);
        
        // 如果wakeupFlag已经为1，则不需要等待
        if (wakeupFlag == 1) {
            LogInfo("WaitForWakeup: wakeupFlag已为1，无需等待");
            return true;
        }
        
        // 等待唤醒事件，带超时
        bool result = false;
        if (timeoutMs > 0) {
            result = g_wakeupCond.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
                []{ return wakeupFlag == 1; });
        } else {
            g_wakeupCond.wait(lock, []{ return wakeupFlag == 1; });
            result = true;
        }
        
        LogInfo("WaitForWakeup: 等待结束，唤醒状态: %d", result);
        return result;
    }
}
