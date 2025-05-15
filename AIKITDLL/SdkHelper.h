#pragma once

namespace AIKITDLL {
    // 检查SDK是否在运行
    bool IsSdkRunning();
    
    // 安全地初始化SDK
    bool SafeInitSDK();
    
    // 安全地清理SDK
    void SafeCleanupSDK();
}
