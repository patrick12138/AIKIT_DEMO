#pragma once

#include "Common.h"
#include "AudioManager.h"
#include "VoiceStateManager.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace AIKITDLL {

    // 语音交互协调器 - 统一管理语音唤醒和命令词识别的完整流程
    class VoiceCoordinator {
    public:
        // 单例模式
        static VoiceCoordinator& GetInstance();
        
        // 启动完整的语音交互循环
        int StartVoiceInteraction(int wakeupThreshold = 50, int esrTimeout = 10);
        
        // 停止语音交互循环
        int StopVoiceInteraction();
        
        // 获取当前状态
        VoiceState GetCurrentState() const;
        
        // 检查是否正在运行
        bool IsRunning() const;
        
        // 手动触发状态转换（用于测试）
        void TriggerWakeupDetected();
        void TriggerCommandCompleted(const std::string& command);
        void TriggerTimeout();
          private:
        VoiceCoordinator();
        ~VoiceCoordinator();
        
        // 禁止拷贝
        VoiceCoordinator(const VoiceCoordinator&) = delete;
        VoiceCoordinator& operator=(const VoiceCoordinator&) = delete;
        
        // 自定义删除器，用于单例模式中的私有析构函数
        struct Deleter {
            void operator()(VoiceCoordinator* ptr) {
                delete ptr;
            }
        };
        
        // 友元声明，允许Deleter访问私有析构函数
        friend struct Deleter;
        
        // 核心循环线程
        void VoiceInteractionLoop();
        
        // 状态处理函数
        void HandleWakeupListening();
        void HandleWakeupDetected();
        void HandleCommandRecognition();
        void HandleCommandCompleted();
        void HandleTimeout();
        void HandleError();
        
        // 唤醒相关
        int StartWakeupDetection();
        int StopWakeupDetection();
        bool CheckWakeupStatus();
        
        // 命令词相关
        int StartCommandRecognition();
        int StopCommandRecognition();
        bool CheckCommandStatus();
          // 资源清理
        void CleanupResources();        // 全局清理
        void CleanupCurrentSession();   // 只清理当前会话
        
        // 状态同步
        void TransitionToState(VoiceState newState);
          private:
        // 单例实例，使用自定义删除器
        static std::unique_ptr<VoiceCoordinator, Deleter> instance_;
        static std::mutex instance_mutex_;
        
        // 运行状态
        std::atomic<bool> is_running_;
        std::atomic<bool> should_stop_;
        std::thread loop_thread_;
        
        // 状态管理
        VoiceStateManager* state_manager_;
        std::atomic<VoiceState> current_state_;
        
        // 同步对象
        std::mutex state_mutex_;
        std::condition_variable state_cv_;
        
        // 参数配置
        int wakeup_threshold_;
        int esr_timeout_;
        
        // 句柄管理 - 统一使用一套句柄
        AIKIT_HANDLE* unified_handle_;
        AIKIT::AIKIT_DataBuilder* unified_data_builder_;
        
        // 超时管理
        std::chrono::steady_clock::time_point last_state_change_;
          // 调试和日志
        std::string last_error_;
        std::atomic<int> loop_iteration_;
    };

} // namespace AIKITDLL
