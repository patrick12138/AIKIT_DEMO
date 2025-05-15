#pragma once

#include "pch.h"
#include "Common.h"
#include <Windows.h>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

// 语音助手状态枚举
enum VOICE_ASSISTANT_STATE {
    STATE_IDLE = 0,             // 空闲状态
    STATE_WAKEUP_LISTENING = 1, // 唤醒词监听中
    STATE_COMMAND_LISTENING = 2,// 命令词监听中
    STATE_PROCESSING = 3        // 命令处理中
};

// 语音助手状态管理类
class VoiceStateManager {
private:
    // 当前状态
    std::atomic<VOICE_ASSISTANT_STATE> m_currentState;
    
    // 控制线程
    std::thread m_controlThread;
    
    // 用于同步的互斥锁
    std::mutex m_stateMutex;
    
    // 控制运行标志
    std::atomic<bool> m_isRunning;
    
    // 状态转换事件句柄
    HANDLE m_stateChangeEvent;
    
    // 内部状态转换函数
    void TransitionToState(VOICE_ASSISTANT_STATE newState);
    
    // 控制线程主函数
    void ControlThreadProc();
    
    // 确保单例模式
    static VoiceStateManager* s_instance;
    
    // 私有构造函数，确保单例
    VoiceStateManager();
    
public:
    // 析构函数
    ~VoiceStateManager();
    
    // 获取单例实例
    static VoiceStateManager* GetInstance();
    
    // 开始语音助手循环
    bool StartVoiceAssistant();
    
    // 停止语音助手循环
    void StopVoiceAssistant();
    
    // 获取当前状态
    VOICE_ASSISTANT_STATE GetCurrentState();
    
    // 处理事件回调
    void HandleEvent(AIKIT_EVENT_TYPE eventType, const char* result);
    
    // 重置状态（用于错误恢复）
    void ResetState();
};

// 导出函数声明
extern "C" {
    AIKITDLL_API int StartVoiceAssistantLoop();
    AIKITDLL_API int StopVoiceAssistantLoop();
    AIKITDLL_API int GetVoiceAssistantState();
    AIKITDLL_API const char* GetLastCommandResult();
}
