#include "pch.h"
#include "VoiceStateManager.h"
#include "Common.h"
#include <mutex>

namespace AIKITDLL {
    // 静态成员定义
    VoiceStateManager* VoiceStateManager::instance = nullptr;
    std::mutex VoiceStateManager::instance_mutex;

    VoiceStateManager* VoiceStateManager::GetInstance() {
        std::lock_guard<std::mutex> lock(instance_mutex);
        if (instance == nullptr) {
            instance = new VoiceStateManager();
        }
        return instance;
    }

    VoiceStateManager::VoiceStateManager() 
        : currentState(WAKEUP_LISTENING), 
          wakeupDetected(false), 
          commandRecognitionCompleted(false) {
    }

    VoiceStateManager::~VoiceStateManager() {
    }

    VoiceState VoiceStateManager::getCurrentState() const {
        return currentState.load();
    }

    void VoiceStateManager::switchToWakeupListening() {
        currentState = WAKEUP_LISTENING;
        wakeupDetected = false;
        commandRecognitionCompleted = false;
        LogInfo("切换到唤醒词监听状态");
    }

    void VoiceStateManager::switchToCommandRecognition() {
        currentState = COMMAND_RECOGNITION;
        wakeupDetected = true;
        commandRecognitionCompleted = false;
        LogInfo("切换到命令词识别状态");
    }

    bool VoiceStateManager::isWakeupDetected() const {
        return wakeupDetected.load();
    }

    void VoiceStateManager::setWakeupDetected(bool detected) {
        wakeupDetected = detected;
    }

    bool VoiceStateManager::isCommandRecognitionCompleted() const {
        return commandRecognitionCompleted.load();
    }

    void VoiceStateManager::setCommandRecognitionCompleted(bool completed) {
        commandRecognitionCompleted = completed;
        if (completed) {
            LogInfo("命令词识别完成，准备返回唤醒词监听状态");
        }
    }
}