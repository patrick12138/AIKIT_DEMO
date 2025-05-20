#pragma once
#include <windows.h>
#include <string>
#include <unordered_map>

// 统一的识别结果管理类
class ResultBufferManager {
public:
    // 结果类型枚举
    enum ResultType {
        PGS,
        HTK,
        PLAIN,
        VAD,
        READABLE
    };

    ResultBufferManager();
    ~ResultBufferManager();

    // 添加结果
    void AddResult(ResultType type, const char* result);
    // 获取结果，返回实际长度，isNewResult为true表示有新结果
    int GetResult(ResultType type, char* buffer, int bufferSize, bool* isNewResult);
    // 清空所有结果
    void ClearAll();

    // 单例接口
    static ResultBufferManager& Instance();
    // ESR专用便捷方法
    void SetStatus(int status);
    int GetStatus() const;
    void SetKeywordResult(const std::string& result);
    const char* GetKeywordResult() const;
    void SetErrorInfo(const std::string& err);
    const char* GetErrorInfo() const;
    void Reset();

private:
    // 缓冲区和新结果标志
    std::unordered_map<ResultType, std::string> m_buffers;
    std::unordered_map<ResultType, bool> m_newFlags;
    // 线程安全
    CRITICAL_SECTION m_lock;
    bool m_lockInitialized;
    void InitLock();

    int m_esrStatus = 0;
    std::string m_esrKeywordResult;
    std::string m_esrErrorInfo;
};
