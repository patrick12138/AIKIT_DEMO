#include "ResultBufferManager.h"

ResultBufferManager::ResultBufferManager() : m_lockInitialized(false), m_esrStatus(0) {
    InitLock();
    m_buffers[PGS] = "";
    m_buffers[HTK] = "";
    m_buffers[PLAIN] = "";
    m_buffers[VAD] = "";
    m_buffers[READABLE] = "";
    m_newFlags[PGS] = false;
    m_newFlags[HTK] = false;
    m_newFlags[PLAIN] = false;
    m_newFlags[VAD] = false;
    m_newFlags[READABLE] = false;
}

ResultBufferManager::~ResultBufferManager() {
    if (m_lockInitialized) {
        DeleteCriticalSection(&m_lock);
        m_lockInitialized = false;
    }
}

void ResultBufferManager::InitLock() {
    if (!m_lockInitialized) {
        __try {
            InitializeCriticalSection(&m_lock);
            m_lockInitialized = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            m_lockInitialized = false;
        }
    }
}

void ResultBufferManager::AddResult(ResultType type, const char* result) {
    if (!result || !m_lockInitialized) return;
    EnterCriticalSection(&m_lock);
    m_buffers[type] = result;
    m_newFlags[type] = true;
    LeaveCriticalSection(&m_lock);
}

int ResultBufferManager::GetResult(ResultType type, char* buffer, int bufferSize, bool* isNewResult) {
    if (!buffer || bufferSize <= 0 || !isNewResult || !m_lockInitialized) return 0;
    EnterCriticalSection(&m_lock);
    *isNewResult = m_newFlags[type];
    int len = (int)m_buffers[type].size();
    if (len > 0 && len < bufferSize) {
        memcpy(buffer, m_buffers[type].c_str(), len);
        buffer[len] = '\0';
    }
    m_newFlags[type] = false;
    LeaveCriticalSection(&m_lock);
    return len;
}

void ResultBufferManager::ClearAll() {
    if (!m_lockInitialized) return;
    EnterCriticalSection(&m_lock);
    for (auto& kv : m_buffers) kv.second.clear();
    for (auto& kv : m_newFlags) kv.second = false;
    LeaveCriticalSection(&m_lock);
}

ResultBufferManager& ResultBufferManager::Instance() {
    static ResultBufferManager instance;
    return instance;
}

void ResultBufferManager::SetStatus(int status) {
    m_esrStatus = status;
}

int ResultBufferManager::GetStatus() const {
    return m_esrStatus;
}

void ResultBufferManager::SetKeywordResult(const std::string& result) {
    m_esrKeywordResult = result;
}

const char* ResultBufferManager::GetKeywordResult() const {
    return m_esrKeywordResult.c_str();
}

void ResultBufferManager::SetErrorInfo(const std::string& err) {
    m_esrErrorInfo = err;
}

const char* ResultBufferManager::GetErrorInfo() const {
    return m_esrErrorInfo.c_str();
}

void ResultBufferManager::Reset() {
    m_esrStatus = 0;
    m_esrKeywordResult.clear();
    m_esrErrorInfo.clear();
}
