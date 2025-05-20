#ifndef VOICE_STATE_MANAGER_H
#define VOICE_STATE_MANAGER_H

#include <atomic>
#include <string>

namespace AIKITDLL {
    enum VoiceState {
        WAKEUP_LISTENING,  // 唤醒词监听状态
        COMMAND_RECOGNITION  // 命令词识别状态
    };

    class VoiceStateManager {
    public:
        VoiceStateManager();
        ~VoiceStateManager();

        // 获取当前状态
        VoiceState getCurrentState() const;

        // 切换到唤醒词监听状态
        void switchToWakeupListening();

        // 切换到命令词识别状态
        void switchToCommandRecognition();

        // 是否检测到唤醒词
        bool isWakeupDetected() const;

        // 设置唤醒词检测标志
        void setWakeupDetected(bool detected);

        // 是否完成命令词识别
        bool isCommandRecognitionCompleted() const;

        // 设置命令词识别完成标志
        void setCommandRecognitionCompleted(bool completed);

    private:
        std::atomic<VoiceState> currentState;
        std::atomic<bool> wakeupDetected;
        std::atomic<bool> commandRecognitionCompleted;
    };
}

#endif // VOICE_STATE_MANAGER_H