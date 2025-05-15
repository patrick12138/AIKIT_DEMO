#include "pch.h"
#include "Common.h"
#include <mutex>

namespace AIKITDLL {
    // 标志SDK是否已成功初始化
    std::atomic<bool> sdkInitialized(false);
    std::mutex sdkMutex;

    // 检查SDK是否在运行
    bool IsSdkRunning() {
        return sdkInitialized.load();
    }

    // 安全地初始化SDK
    bool SafeInitSDK() {
        std::lock_guard<std::mutex> lock(sdkMutex);
        
        // 如果SDK已初始化，直接返回成功
        if (sdkInitialized.load()) {
            return true;
        }
        
        // 尝试初始化SDK
        if (InitializeAIKitSDK() == 0) {
            sdkInitialized.store(true);
            return true;
        }
        
        return false;
    }

    // 安全地清理SDK
    void SafeCleanupSDK() {
        std::lock_guard<std::mutex> lock(sdkMutex);
        
        if (sdkInitialized.load()) {
            AIKIT::AIKIT_UnInit();
            sdkInitialized.store(false);
        }
    }
}
