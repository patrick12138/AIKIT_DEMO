#include "pch.h"
#include "VoiceCoordinator.h"
#include "IvwWrapper.h"
#include "CnenEsrWrapper.h"
#include "AudioManager.h"
#include "aikit_biz_api.h"
#include "aikit_biz_builder.h"
#include "aikit_constant.h"
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

// 引用外部声明的全局变量
extern std::atomic<int> wakeupFlag;  // 定义在IvwWrapper.cpp中
extern void ResetWakeupStatus();     // 定义在IvwWrapper.cpp中

// AIKITDLL命名空间中的ESR变量不需要extern声明，直接使用命名空间访问

namespace AIKITDLL {    // 静态成员初始化
    std::unique_ptr<VoiceCoordinator, VoiceCoordinator::Deleter> VoiceCoordinator::instance_ = nullptr;
    std::mutex VoiceCoordinator::instance_mutex_;

    VoiceCoordinator& VoiceCoordinator::GetInstance() {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_) {
            instance_ = std::unique_ptr<VoiceCoordinator, VoiceCoordinator::Deleter>(new VoiceCoordinator());
        }
        return *instance_;
    }    VoiceCoordinator::VoiceCoordinator()
        : is_running_(false)
        , should_stop_(false)
        , state_manager_(nullptr)
        , current_state_(VoiceState::Idle)
        , wakeup_threshold_(50)
        , esr_timeout_(10)
        , unified_handle_(nullptr)
        , unified_data_builder_(nullptr)
        , loop_iteration_(0) {
        
        // 获取状态管理器实例
        state_manager_ = VoiceStateManager::GetInstance();
        if (state_manager_) {
            state_manager_->switchToWakeupListening();
        }
        
        LogInfo("VoiceCoordinator 已初始化");
    }

    VoiceCoordinator::~VoiceCoordinator() {
        if (is_running_.load()) {
            StopVoiceInteraction();
        }
        CleanupResources();
        LogInfo("VoiceCoordinator 已销毁");
    }

    int VoiceCoordinator::StartVoiceInteraction(int wakeupThreshold, int esrTimeout) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (is_running_.load()) {
            LogWarning("语音交互循环已在运行");
            return -1;
        }
        
        LogInfo("启动统一语音交互循环，唤醒阈值: %d, ESR超时: %d秒", wakeupThreshold, esrTimeout);
        
        // 保存参数
        wakeup_threshold_ = wakeupThreshold;
        esr_timeout_ = esrTimeout;
        
        // 重置状态
        should_stop_ = false;
        current_state_ = VoiceState::Idle;
        last_error_.clear();
        loop_iteration_ = 0;
        
        // 清理之前的资源
        CleanupResources();
        
        // 初始化SDK
        int ret = InitializeAIKitSDK();
        if (ret != 0) {
            LogError("初始化AIKit SDK失败: %d", ret);
            return ret;
        }
        
        // 启动循环线程
        try {
            is_running_ = true;
            loop_thread_ = std::thread(&VoiceCoordinator::VoiceInteractionLoop, this);
            LogInfo("语音交互循环线程已启动");
            return 0;
        } catch (const std::exception& e) {
            LogError("启动语音交互循环线程失败: %s", e.what());
            is_running_ = false;
            return -1;
        }
    }

    int VoiceCoordinator::StopVoiceInteraction() {
        LogInfo("停止语音交互循环");
        
        should_stop_ = true;
        
        if (loop_thread_.joinable()) {
            state_cv_.notify_all(); // 唤醒等待的线程
            try {
                loop_thread_.join();
                LogInfo("语音交互循环线程已结束");
            } catch (const std::exception& e) {
                LogError("等待语音交互循环线程结束失败: %s", e.what());
                return -1;
            }
        }
        
        is_running_ = false;
        CleanupResources();
        
        return 0;
    }

    VoiceState VoiceCoordinator::GetCurrentState() const {
        return current_state_.load();
    }

    bool VoiceCoordinator::IsRunning() const {
        return is_running_.load();
    }

    void VoiceCoordinator::VoiceInteractionLoop() {
        LogInfo("语音交互循环开始");
        last_state_change_ = std::chrono::steady_clock::now();
        
        TransitionToState(VoiceState::Idle);
        
        while (!should_stop_.load()) {
            loop_iteration_++;
            
            try {
                VoiceState state = current_state_.load();
                
                switch (state) {
                    case VoiceState::Idle:
                        HandleWakeupListening();
                        break;
                        
                    case VoiceState::WakeupDetected:
                        HandleWakeupDetected();
                        break;
                        
                    case VoiceState::ListeningCommand:
                        HandleCommandRecognition();
                        break;
                        
                    case VoiceState::CommandCompleted:
                        HandleCommandCompleted();
                        break;
                        
                    case VoiceState::Timeout:
                        HandleTimeout();
                        break;
                        
                    case VoiceState::Error:
                        HandleError();
                        break;
                        
                    default:
                        LogWarning("未知状态: %d", static_cast<int>(state));
                        TransitionToState(VoiceState::Error);
                        break;
                }
                
                // 检查超时
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_state_change_).count();
                
                if (state == VoiceState::ListeningCommand && elapsed >= esr_timeout_) {
                    LogInfo("命令词识别超时 (%d秒)", esr_timeout_);
                    TransitionToState(VoiceState::Timeout);
                }
                
                // 短暂休眠避免CPU占用过高
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
            } catch (const std::exception& e) {
                LogError("语音交互循环异常: %s", e.what());
                last_error_ = e.what();
                TransitionToState(VoiceState::Error);
            }
        }
        
        LogInfo("语音交互循环结束，总迭代次数: %d", loop_iteration_.load());
    }

    void VoiceCoordinator::HandleWakeupListening() {
        LogDebug("处理唤醒监听状态 (迭代 %d)", loop_iteration_.load());
        
        // 启动唤醒检测
        if (unified_handle_ == nullptr) {
            int ret = StartWakeupDetection();
            if (ret != 0) {
                LogError("启动唤醒检测失败: %d", ret);
                TransitionToState(VoiceState::Error);
                return;
            }
        }
        
        // 检查唤醒状态
        if (CheckWakeupStatus()) {
            LogInfo("检测到唤醒词！");
            TransitionToState(VoiceState::WakeupDetected);
        }
    }

    void VoiceCoordinator::HandleWakeupDetected() {
        LogInfo("处理唤醒检测状态");
        
        // 停止唤醒检测
        StopWakeupDetection();
        
        // 短暂延迟给用户反应时间
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 转换到命令词监听
        TransitionToState(VoiceState::ListeningCommand);
    }

    void VoiceCoordinator::HandleCommandRecognition() {
        LogDebug("处理命令词识别状态 (迭代 %d)", loop_iteration_.load());
        
        // 启动命令词识别
        if (unified_handle_ == nullptr) {
            int ret = StartCommandRecognition();
            if (ret != 0) {
                LogError("启动命令词识别失败: %d", ret);
                TransitionToState(VoiceState::Error);
                return;
            }
        }
        
        // 检查命令词状态
        if (CheckCommandStatus()) {
            LogInfo("命令词识别完成！");
            TransitionToState(VoiceState::CommandCompleted);
        }
    }

    void VoiceCoordinator::HandleCommandCompleted() {
        LogInfo("处理命令词完成状态");
        
        // 停止命令词识别
        StopCommandRecognition();
        
        // 短暂延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // 返回唤醒监听
        TransitionToState(VoiceState::Idle);
    }

    void VoiceCoordinator::HandleTimeout() {
        LogInfo("处理超时状态");
        
        // 清理当前会话
        StopCommandRecognition();
        
        // 短暂延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 返回唤醒监听
        TransitionToState(VoiceState::Idle);
    }

    void VoiceCoordinator::HandleError() {
        LogError("处理错误状态: %s", last_error_.c_str());
        
        // 清理所有资源
        CleanupResources();
        
        // 延迟后重试
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // 重新初始化
        int ret = InitializeAIKitSDK();
        if (ret == 0) {
            LogInfo("错误恢复成功，返回唤醒监听");
            TransitionToState(VoiceState::Idle);
        } else {
            LogError("错误恢复失败，将在下次循环重试");
            last_error_ = "SDK重新初始化失败: " + std::to_string(ret);
        }
    }

    int VoiceCoordinator::StartWakeupDetection() {
        LogInfo("协调器启动唤醒检测");
        
        // 使用统一的句柄和数据构建器
        int ret = ivw_start_session(IVW_ABILITY, &unified_handle_, wakeup_threshold_);
        if (ret != 0 || unified_handle_ == nullptr) {
            LogError("启动唤醒会话失败: %d", ret);
            return ret;
        }
        
        if (unified_data_builder_ == nullptr) {
            unified_data_builder_ = AIKIT::AIKIT_DataBuilder::create();
            if (!unified_data_builder_) {
                LogError("创建统一数据构建器失败");
                ivw_stop_session(unified_handle_);
                unified_handle_ = nullptr;
                return -1;
            }
        }
        unified_data_builder_->clear();
        
        // 激活AudioManager的IVW消费者，统一使用"audio"作为audioKey
        bool activated = AudioManager::GetInstance().ActivateConsumer(
            AudioConsumer::IVW,
            unified_handle_,
            unified_data_builder_,
            "audio" // 统一使用"audio"
        );
        
        if (!activated) {
            LogError("激活AudioManager IVW消费者失败");
            ivw_stop_session(unified_handle_);
            unified_handle_ = nullptr;
            return -1;
        }
        
        // 重置唤醒标志
        wakeupFlag = 0;
        ResetWakeupStatus();
        
        LogInfo("唤醒检测已启动");
        return 0;
    }

    int VoiceCoordinator::StopWakeupDetection() {
        LogInfo("协调器停止唤醒检测");
        
        // 停用AudioManager的IVW消费者
        AudioManager::GetInstance().DeactivateConsumer(AudioConsumer::IVW);
        
        // 停止会话
        if (unified_handle_ != nullptr) {
            ivw_stop_session(unified_handle_);
            unified_handle_ = nullptr;
        }
        
        return 0;
    }

    bool VoiceCoordinator::CheckWakeupStatus() {
        // 检查全局唤醒标志
        if (wakeupFlag.load() == 1) {
            return true;
        }
        
        // 检查状态管理器
        if (state_manager_ && state_manager_->isWakeupDetected()) {
            return true;
        }
        
        return false;
    }

    int VoiceCoordinator::StartCommandRecognition() {
        LogInfo("协调器启动命令词识别");
        
        // 初始化ESR能力
        int ret = CnenEsrInit();
        if (ret != 0) {
            LogError("ESR初始化失败: %d", ret);
            return ret;
        }
        
        // 创建参数构建器
        AIKIT::AIKIT_ParamBuilder* paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
        if (!paramBuilder) {
            LogError("创建ESR参数构建器失败");
            return -1;
        }
        
        paramBuilder->clear();
        paramBuilder->param("languageType", 0);
        paramBuilder->param("vadEndGap", 75);
        paramBuilder->param("vadOn", true);
        
        // 指定数据集
        int index[] = { 0 };
        ret = AIKIT::AIKIT_SpecifyDataSet(ESR_ABILITY, "FSA", index, 1);
        if (ret != 0) {
            LogError("指定ESR数据集失败: %d", ret);
            delete paramBuilder;
            return ret;
        }
        
        // 启动ESR会话 - 重用统一句柄
        ret = AIKIT::AIKIT_Start(ESR_ABILITY, AIKIT::AIKIT_Builder::build(paramBuilder), nullptr, &unified_handle_);
        delete paramBuilder;
        
        if (ret != 0 || unified_handle_ == nullptr) {
            LogError("启动ESR会话失败: %d", ret);
            return ret;
        }
        
        if (unified_data_builder_ == nullptr) {
            unified_data_builder_ = AIKIT::AIKIT_DataBuilder::create();
        }
        unified_data_builder_->clear();
        
        // 激活AudioManager的ESR消费者，统一使用"audio"作为audioKey
        bool activated = AudioManager::GetInstance().ActivateConsumer(
            AudioConsumer::ESR,
            unified_handle_,
            unified_data_builder_,
            "audio" // 统一使用"audio"
        );
        
        if (!activated) {
            LogError("激活AudioManager ESR消费者失败");
            AIKIT::AIKIT_End(unified_handle_);
            unified_handle_ = nullptr;
            return -1;
        }        // 重置ESR状态
        AIKITDLL::esrResultFlag = 0;
        std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
        AIKITDLL::lastEsrKeywordResult.clear();
        AIKITDLL::lastEsrErrorInfo.clear();
        AIKITDLL::esrStatus = AIKITDLL::ESR_STATUS_PROCESSING_INTERNAL;
        
        LogInfo("命令词识别已启动");
        return 0;
    }

    int VoiceCoordinator::StopCommandRecognition() {
        LogInfo("协调器停止命令词识别");
        
        // 停用AudioManager的ESR消费者
        AudioManager::GetInstance().DeactivateConsumer(AudioConsumer::ESR);        // 发送结束信号
        if (unified_handle_ != nullptr && unified_data_builder_ != nullptr) {
            unified_data_builder_->clear();
            AIKIT::AiAudio* audioEndObj = AIKIT::AiAudio::get("audio")->status(AIKIT_DataEnd)->data(nullptr, 0)->valid();
            if (audioEndObj) {
                unified_data_builder_->payload(audioEndObj);
                int ret = AIKIT::AIKIT_Write(unified_handle_, AIKIT::AIKIT_Builder::build(unified_data_builder_));
                if (ret != 0) {  // AIKIT_ERR_SUCCESS = 0
                    LogError("发送ESR结束信号失败: %d", ret);
                }
            }
        }
        
        // 结束会话
        if (unified_handle_ != nullptr) {
            AIKIT::AIKIT_End(unified_handle_);
            unified_handle_ = nullptr;
        }
        
        // 清理ESR资源
        CnenEsrUninit();
        
        return 0;
    }    bool VoiceCoordinator::CheckCommandStatus() {
        std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
        return (AIKITDLL::esrStatus.load() == AIKITDLL::ESR_STATUS_SUCCESS_INTERNAL && !AIKITDLL::lastEsrKeywordResult.empty());
    }

    void VoiceCoordinator::CleanupResources() {
        LogInfo("清理协调器资源");
        
        // 停用所有AudioManager消费者
        AudioManager::GetInstance().DeactivateConsumer(AudioConsumer::IVW);
        AudioManager::GetInstance().DeactivateConsumer(AudioConsumer::ESR);
        
        // 清理统一句柄
        if (unified_handle_ != nullptr) {
            AIKIT::AIKIT_End(unified_handle_);
            unified_handle_ = nullptr;
        }
        
        // 清理数据构建器
        if (unified_data_builder_ != nullptr) {
            delete unified_data_builder_;
            unified_data_builder_ = nullptr;
        }        // 重置状态
        wakeupFlag = 0;
        AIKITDLL::esrResultFlag = 0;
        ResetWakeupStatus();
    }

    void VoiceCoordinator::TransitionToState(VoiceState newState) {
        VoiceState oldState = current_state_.load();
        if (oldState != newState) {
            current_state_ = newState;
            last_state_change_ = std::chrono::steady_clock::now();
            
            if (state_manager_) {
                switch (newState) {
                    case VoiceState::Idle:
                        state_manager_->switchToWakeupListening();
                        break;
                    case VoiceState::ListeningCommand:
                        state_manager_->switchToCommandRecognition();
                        break;
                    default:
                        break;
                }
            }
            
            LogInfo("状态转换: %d -> %d", static_cast<int>(oldState), static_cast<int>(newState));
            state_cv_.notify_all();
        }
    }

    // 手动触发函数（用于测试）
    void VoiceCoordinator::TriggerWakeupDetected() {
        LogInfo("手动触发唤醒检测");
        wakeupFlag = 1;
        if (state_manager_) {
            state_manager_->setWakeupDetected(true);
        }
    }    void VoiceCoordinator::TriggerCommandCompleted(const std::string& command) {
        LogInfo("手动触发命令词完成: %s", command.c_str());
        std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
        AIKITDLL::lastEsrKeywordResult = command;
        AIKITDLL::esrStatus = AIKITDLL::ESR_STATUS_SUCCESS_INTERNAL;
    }

    void VoiceCoordinator::TriggerTimeout() {
        LogInfo("手动触发超时");
        TransitionToState(VoiceState::Timeout);
    }

} // namespace AIKITDLL
