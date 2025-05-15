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
    
    // 重置所有唤醒状态标志位
    void ResetWakeupStatus() {
        // 记录日志
        LogInfo("ResetWakeupStatus: 已重置所有唤醒状态标志");
        
        // 重置所有与唤醒相关的标志位
        wakeupFlag = 0;
        wakeupDetected = false;
        lastEventType = EVENT_NONE;
        g_ivwSessionActive.store(false);
        
        // 重置外部数据
        lastResult = "";
        
        // 通知所有等待者
        std::unique_lock<std::mutex> lock(g_wakeupMutex);
        g_wakeupCond.notify_all();
    }
    
    // 等待唤醒事件，带超时
    bool WaitForWakeup(int timeoutMs) {
        std::unique_lock<std::mutex> lock(g_wakeupMutex);
        LogInfo("WaitForWakeup: 开始等待唤醒事件，超时时间: %d ms", timeoutMs);
        
        // 检查唤醒状态 - 方法1：直接检查wakeupFlag
        if (wakeupFlag.load() == 1) {
            LogInfo("WaitForWakeup: wakeupFlag已为1，无需等待");
            return true;
        }
        
        // 检查唤醒状态 - 方法2：通过GetWakeupStatus检查
        if (GetWakeupStatus() == 1) {
            LogInfo("WaitForWakeup: GetWakeupStatus返回1，主动设置wakeupFlag");
            wakeupFlag.store(1);
            return true;
        }
        
        // 检查唤醒状态 - 方法3：检查最后事件类型
        if (lastEventType == EVENT_WAKEUP_SUCCESS) {
            LogInfo("WaitForWakeup: 检测到唤醒成功事件，主动设置wakeupFlag");
            wakeupFlag.store(1);
            return true;
        }
        
        // 等待唤醒事件，带超时
        bool result = false;
        if (timeoutMs > 0) {
            result = g_wakeupCond.wait_for(lock, std::chrono::milliseconds(timeoutMs), 
                []{ 
                    // 条件变量等待期间再次检查状态
                    if (wakeupFlag.load() == 1 || GetWakeupStatus() == 1 || lastEventType == EVENT_WAKEUP_SUCCESS) {
                        wakeupFlag.store(1);
                        return true;
                    }
                    return false;
                });
        } else {
            g_wakeupCond.wait(lock, []{ 
                // 条件变量等待期间再次检查状态
                if (wakeupFlag == 1 || GetWakeupStatus() == 1 || lastEventType == EVENT_WAKEUP_SUCCESS) {
                    wakeupFlag = 1;
                    return true;
                }
                return false;
            });
            result = (wakeupFlag == 1);
        }
        
        LogInfo("WaitForWakeup: 等待结束，唤醒状态: %d", result);
        return result;
    }
}
