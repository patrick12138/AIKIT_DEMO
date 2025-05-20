#pragma once
#include <aikit_biz_type.h>

#include <atomic>
#include <mutex>
#include <thread>
#include "winrec.h"
#include "aikit_constant.h"
#include <aikit_biz_api.h>

// 持续唤醒功能的状态枚举
enum ContinuousWakeupState {
    CWS_IDLE,        // 空闲状态，未启动
    CWS_STARTING,    // 正在启动
    CWS_LISTENING,   // 正在监听
    CWS_STOPPING,    // 正在停止
    CWS_ERROR        // 出错状态
};

// 唤醒事件回调函数类型
typedef void (*WakeupEventCallback)(const char* keyword, int confidence);

namespace AIKITDLL {

    // 持续唤醒功能管理类
    class ContinuousWakeupManager {
    private:
        // 单例实例
        static ContinuousWakeupManager* s_instance;
        static std::mutex s_mutex;

        // 状态相关
        std::atomic<ContinuousWakeupState> m_state;
        std::atomic<bool> m_keepRunning;
        std::thread m_workerThread;
        
        // 资源相关
        AIKIT_HANDLE* m_handle;
        struct recorder* m_recorder;
        AIKIT::AIKIT_DataBuilder* m_dataBuilder;
        
        // 参数相关
        int m_threshold;
        WakeupEventCallback m_callback;

        // 私有构造函数和析构函数（单例模式）
        ContinuousWakeupManager();
        ~ContinuousWakeupManager();

        // 禁止拷贝和赋值
        ContinuousWakeupManager(const ContinuousWakeupManager&) = delete;
        ContinuousWakeupManager& operator=(const ContinuousWakeupManager&) = delete;

        // 工作线程函数
        void WorkerThread();
        // 音频回调函数
        static void AudioCallback(char* data, unsigned long len, void* user_para);
        // 资源清理
        void Cleanup();

    public:
        // 获取单例实例
        static ContinuousWakeupManager* GetInstance();
        // 释放单例实例
        static void ReleaseInstance();

        // 开始持续唤醒检测
        int Start(int threshold, WakeupEventCallback callback);
        // 停止持续唤醒检测
        int Stop();
        // 获取当前状态
        ContinuousWakeupState GetState() const;
    };

} // namespace AIKITDLL
