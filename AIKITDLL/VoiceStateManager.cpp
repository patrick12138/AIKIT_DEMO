#include "pch.h"
#include "VoiceStateManager.h"
#include "IvwWrapper.h"
#include "CnenEsrWrapper.h"
#include "SdkHelper.h"
#include <chrono>

// 静态实例初始化
VoiceStateManager* VoiceStateManager::s_instance = nullptr;

// 设置最大等待时间（毫秒）
const int MAX_COMMAND_WAIT_TIME = 10000; // 10秒

// 构造函数
VoiceStateManager::VoiceStateManager()
    : m_currentState(STATE_IDLE),
    m_isRunning(false),
    m_sdkInitialized(false),
    m_wakeupInitialized(false),
    m_consecutiveFailures(0) {
    // 创建状态变化事件对象
    m_stateChangeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

// 析构函数
VoiceStateManager::~VoiceStateManager() {
	// 确保线程已停止
	StopVoiceAssistant();

	// 清理事件对象
	if (m_stateChangeEvent) {
		CloseHandle(m_stateChangeEvent);
		m_stateChangeEvent = NULL;
	}
}

// 静态互斥锁的定义
std::mutex VoiceStateManager::s_instanceMutex;

// 获取单例实例（使用双重检查锁定模式）
VoiceStateManager* VoiceStateManager::GetInstance() {
    // 第一次检查，避免每次都加锁
    if (s_instance == nullptr) {
        std::lock_guard<std::mutex> lock(s_instanceMutex);
        // 第二次检查，确保只创建一个实例
        if (s_instance == nullptr) {
            try {
                VoiceStateManager* temp = new VoiceStateManager();
                // 使用内存栅栏确保实例完全初始化后再赋值
                std::atomic_thread_fence(std::memory_order_release);
                s_instance = temp;
                AIKITDLL::LogDebug("VoiceStateManager单例实例已创建\n");
            }
            catch (const std::exception& e) {
                AIKITDLL::LogError("创建VoiceStateManager实例失败: %s\n", e.what());
                throw;
            }
        }
    }
    return s_instance;
}

// 开始语音助手循环
bool VoiceStateManager::StartVoiceAssistant() {
    // 检查是否已在运行
    if (m_isRunning.load(std::memory_order_acquire)) {
        AIKITDLL::LogWarning("语音助手已经在运行中\n");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        
        // 确保资源初始状态
        m_sdkInitialized.store(false, std::memory_order_release);
        m_wakeupInitialized.store(false, std::memory_order_release);
        m_currentState.store(STATE_IDLE, std::memory_order_release);
        m_consecutiveFailures.store(0, std::memory_order_release);

        // 重置事件
        if (m_stateChangeEvent) {
            ResetEvent(m_stateChangeEvent);
        } else {
            m_stateChangeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (!m_stateChangeEvent) {
                AIKITDLL::LogError("创建事件对象失败\n");
                return false;
            }
        }

        // 设置运行标志
        m_isRunning.store(true, std::memory_order_release);

        // 启动控制线程
        try {
            m_controlThread = std::thread(&VoiceStateManager::ControlThreadProc, this);
        } catch (const std::exception& e) {
            AIKITDLL::LogError("启动控制线程失败: %s\n", e.what());
            m_isRunning.store(false, std::memory_order_release);
            return false;
        }
    }

    // 触发初始状态变化
    TransitionToState(STATE_WAKEUP_LISTENING);

    AIKITDLL::LogDebug("语音助手启动成功\n");
    return true;
}

// 停止语音助手循环
void VoiceStateManager::StopVoiceAssistant() {
	// 设置停止标志
	m_isRunning.store(false);

	// 触发事件，让控制线程可以检查停止标志
	SetEvent(m_stateChangeEvent);
	// 等待线程完成
	if (m_controlThread.joinable()) {
		m_controlThread.join();
	}

	// 重置状态
	m_currentState.store(STATE_IDLE);

	AIKITDLL::LogDebug("语音助手已停止\n");
}

// 获取当前状态
VOICE_ASSISTANT_STATE VoiceStateManager::GetCurrentState() {
	return m_currentState.load();
}

// 内部状态转换函数
void VoiceStateManager::TransitionToState(VOICE_ASSISTANT_STATE newState) {
    // 先获取所有状态值,避免在锁内频繁访问atomic变量
    VOICE_ASSISTANT_STATE oldState = m_currentState.load(std::memory_order_acquire);
    bool sdkInit = m_sdkInitialized.load(std::memory_order_acquire);
    bool wakeupInit = m_wakeupInitialized.load(std::memory_order_acquire);
    int failures = m_consecutiveFailures.load(std::memory_order_acquire);

    // 如果状态相同，不需要转换
    if (oldState == newState) {
        AIKITDLL::LogDebug("状态未变化，保持在: %s (SDK初始化=%d, 唤醒初始化=%d)\n", 
            GetStateName(oldState), sdkInit, wakeupInit);
        return;
    }

    std::lock_guard<std::mutex> lock(m_stateMutex);

    // 如果状态相同，不需要转换
    if (oldState == newState) {
        AIKITDLL::LogDebug("状态未变化，保持在: %s (SDK初始化=%d, 唤醒初始化=%d)\n", 
            GetStateName(oldState), sdkInit, wakeupInit);
        return;
    }

    // 离开当前状态时的清理工作
    switch (oldState) {
    case STATE_WAKEUP_LISTENING:
        if (m_wakeupInitialized) {
            AIKITDLL::LogDebug("离开唤醒监听状态，清理唤醒资源\n");
            Ivw70Uninit();
            m_wakeupInitialized = false;
        }
        break;
    case STATE_COMMAND_LISTENING:
        AIKITDLL::LogDebug("离开命令词识别状态，重置ESR状态\n");
        ResetEsrStatus();
        break;
    }

    // 记录详细的状态转换信息
    AIKITDLL::LogDebug("语音助手状态转换: %s -> %s (SDK初始化=%d, 唤醒初始化=%d, 失败次数=%d)\n",
        GetStateName(oldState), GetStateName(newState),
        sdkInit, wakeupInit, failures);
        
    // 进入新状态前的准备工作
    switch (newState) {
    case STATE_IDLE:
        // 进入空闲状态时，确保所有资源都被清理
        ResetSDKState();
        break;
    case STATE_WAKEUP_LISTENING:
        // 进入唤醒监听状态时，重置所有唤醒相关标志
        ResetWakeupStatus();
        break;
    }

    // 设置新状态
    m_currentState.store(newState);

    // 触发状态变化事件
    SetEvent(m_stateChangeEvent);
}

// 获取状态名称的辅助函数
const char* VoiceStateManager::GetStateName(VOICE_ASSISTANT_STATE state) {
    switch (state) {
    case STATE_IDLE:
        return "空闲";
    case STATE_WAKEUP_LISTENING:
        return "唤醒监听";
    case STATE_COMMAND_LISTENING:
        return "命令词监听";
    case STATE_PROCESSING:
        return "处理中";
    default:
        return "未知状态";
    }
}

// 处理事件回调 
void VoiceStateManager::HandleEvent(AIKIT_EVENT_TYPE eventType, const char* result) {
    // 先获取状态,减少锁内操作
    bool sdkInit = m_sdkInitialized.load(std::memory_order_acquire);
    bool wakeupInit = m_wakeupInitialized.load(std::memory_order_acquire);
    VOICE_ASSISTANT_STATE currentState = m_currentState.load(std::memory_order_acquire);

    AIKITDLL::LogDebug("[事件处理开始] 类型=%d, 结果=%s, 当前状态=%s, SDK=%d, 唤醒=%d\n",
        eventType, result ? result : "无",
        GetStateName(currentState),
        sdkInit, wakeupInit);

    std::unique_lock<std::mutex> lock(m_stateMutex);

    AIKITDLL::LogDebug("[事件处理] 类型=%d, 结果=%s, 当前状态=%s, SDK=%d, 唤醒=%d\n",
        eventType, result ? result : "无",
        GetStateName(currentState),
        sdkInit,
        wakeupInit);

    // 验证当前状态是否适合处理该事件
    bool isValidEvent = true;
    switch (eventType) {
    case EVENT_WAKEUP_SUCCESS:
        if (currentState != STATE_WAKEUP_LISTENING) {
            AIKITDLL::LogWarning("收到唤醒成功事件，但当前不在唤醒监听状态\n");
            isValidEvent = false;
        }
        break;
    case EVENT_WAKEUP_FAILED:
        if (currentState != STATE_WAKEUP_LISTENING) {
            AIKITDLL::LogWarning("收到唤醒失败事件，但当前不在唤醒监听状态\n");
            isValidEvent = false;
        }
        break;
    case EVENT_ESR_SUCCESS:
    case EVENT_ESR_FAILED:
    case EVENT_ESR_TIMEOUT:
        if (currentState != STATE_COMMAND_LISTENING) {
            AIKITDLL::LogWarning("收到命令词识别事件，但当前不在命令词识别状态\n");
            isValidEvent = false;
        }
        break;
    }

    if (!isValidEvent) {
        AIKITDLL::LogWarning("忽略不合适的事件\n");
        return;
    }

	switch (eventType) {
	case EVENT_WAKEUP_SUCCESS:
		// 唤醒成功，转到命令词识别状态前先确保清理当前资源
		if (m_wakeupInitialized) {
            AIKITDLL::LogDebug("准备进入命令词识别状态，清理唤醒资源\n");
            Ivw70Uninit();
            m_wakeupInitialized = false;
        }
		
		// 主动设置唤醒标志位，确保状态一致性
		AIKITDLL::wakeupFlag = 1;
		AIKITDLL::wakeupDetected = true;
		AIKITDLL::lastEventType = EVENT_WAKEUP_SUCCESS;
		
		// 状态转换
		TransitionToState(STATE_COMMAND_LISTENING);
		break;

	case EVENT_WAKEUP_FAILED:
		// 唤醒失败，重新回到唤醒监听状态
		
		// 重置唤醒标志位
		AIKITDLL::wakeupFlag = 0;
		AIKITDLL::wakeupDetected = false;
		
		// 状态转换
		TransitionToState(STATE_WAKEUP_LISTENING);
		break;

	case EVENT_ESR_SUCCESS:
	case EVENT_ESR_FAILED:
	case EVENT_ESR_TIMEOUT:
		// 命令词识别完成（无论成功失败），回到唤醒监听状态
		TransitionToState(STATE_WAKEUP_LISTENING);
		break;

	default:
		// 未知事件，忽略
		break;
	}
}

// 重置状态（用于错误恢复）
void VoiceStateManager::ResetState() {
    // 先获取当前状态,避免在锁内频繁访问
    VOICE_ASSISTANT_STATE currentState = m_currentState.load(std::memory_order_acquire);
    
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        // 清理当前状态的资源
        switch (currentState) {
        case STATE_WAKEUP_LISTENING:
            if (m_wakeupInitialized.load(std::memory_order_acquire)) {
                AIKITDLL::LogDebug("清理唤醒资源...\n");
                Ivw70Uninit();
                m_wakeupInitialized.store(false, std::memory_order_release);
            }
            break;

        case STATE_COMMAND_LISTENING:
            AIKITDLL::LogDebug("重置ESR状态...\n");
            ResetEsrStatus();
            break;

        default:
            break;
        }
    }

    // 重置计数并延迟一段时间确保资源释放
    m_consecutiveFailures.store(0, std::memory_order_release);
    Sleep(100);

    // 重新设置状态
    TransitionToState(STATE_WAKEUP_LISTENING);
}

// 控制线程主函数
void VoiceStateManager::ControlThreadProc() {
    AIKITDLL::LogDebug("语音助手控制线程启动\n");
    
    // 设置回调函数
    AIKIT_Callbacks cbs = { AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError };    // 确保清理之前的状态
    ResetSDKState();
    
    // 等待一小段时间确保之前的资源完全释放
    Sleep(100);
    
    // 尝试初始化SDK
    m_sdkInitialized = AIKITDLL::SafeInitSDK();
    if (!m_sdkInitialized) {
        AIKITDLL::LogError("SDK初始化失败，语音助手无法启动\n");
        m_isRunning.store(false);
        return;
    }
    
    // 初始化成功后重置失败计数
    m_consecutiveFailures.store(0);

    while (m_isRunning.load()) {
        try {
            // 获取当前状态
            VOICE_ASSISTANT_STATE currentState = m_currentState.load();

            // 根据当前状态执行操作
            switch (currentState) {
            case STATE_IDLE:
                // 空闲状态，等待事件
                WaitForSingleObject(m_stateChangeEvent, INFINITE);
                ResetEvent(m_stateChangeEvent);
                break;

            case STATE_WAKEUP_LISTENING:
            {                // 检查SDK是否需要重新初始化
                if (!m_sdkInitialized) {                    bool wakeupInit = m_wakeupInitialized.load();
                    int failures = m_consecutiveFailures.load();
                    AIKITDLL::LogDebug("[状态检查] 当前状态=%s, SDK未初始化, 唤醒状态=%d, 失败次数=%d\n",
                        GetStateName(currentState), wakeupInit, failures);
                    
                    AIKITDLL::LogDebug("尝试重新初始化SDK...\n");
                    // 确保完全清理之前的状态
                    ResetSDKState();
                    
                    // 尝试重新初始化
                    m_sdkInitialized = AIKITDLL::SafeInitSDK();
                    if (!m_sdkInitialized) {
                        AIKITDLL::LogError("SDK重新初始化失败，等待恢复...\n");
                        m_consecutiveFailures++;
                        if (m_consecutiveFailures.load() >= 3) {
                            AIKITDLL::LogError("连续多次初始化失败，需要重置系统\n");
                            TransitionToState(STATE_IDLE);
                            Sleep(5000); // 延长等待时间，避免频繁重试
                        } else {
                            Sleep(2000);
                        }
                        continue;
                    }
                    AIKITDLL::LogDebug("SDK重新初始化成功\n");
                }

                // 初始化唤醒功能（如果需要）
                if (!m_wakeupInitialized) {
                    AIKITDLL::LogDebug("初始化唤醒功能...\n");
                    int ret = Ivw70Init();
                    if (ret != 0) {
                        AIKITDLL::LogError("唤醒功能初始化失败，错误码：%d\n", ret);
                        m_consecutiveFailures++;
                        if (m_consecutiveFailures.load() >= 3) {
                            AIKITDLL::LogError("连续多次初始化失败，重置系统...\n");
                            ResetSDKState();
                            TransitionToState(STATE_IDLE);
                            Sleep(5000);
                        } else {
                            Sleep(2000);
                        }
                        continue;
                    }
                    m_wakeupInitialized = true;
                    m_consecutiveFailures.store(0); // 成功后重置失败计数
                    AIKITDLL::LogDebug("唤醒功能初始化成功\n");
                }                // 启动麦克风唤醒前先检查状态
                if (!m_wakeupInitialized || !m_sdkInitialized) {
                    AIKITDLL::LogError("唤醒或SDK未正确初始化，无法启动麦克风唤醒\n");
                    ResetSDKState();
                    TransitionToState(STATE_IDLE);
                    Sleep(2000);
                    continue;
                }

                AIKITDLL::LogDebug("启动麦克风唤醒监听...\n");
                int ret = Ivw70Microphone(cbs);
                if (ret != 0) {
                    AIKITDLL::LogError("启动麦克风唤醒失败，错误码：%d\n", ret);
                    
                    // 尝试恢复状态
                    ResetSDKState();
                    m_consecutiveFailures++;
                    
                    if (m_consecutiveFailures.load() >= 3) {
                        AIKITDLL::LogError("连续多次启动失败，重置系统...\n");
                        TransitionToState(STATE_IDLE);
                        Sleep(5000);
                    } else {
                        // 短暂等待后重试
                        Sleep(2000);
                    }
                    continue;
                }

                AIKITDLL::LogDebug("唤醒监听已启动，等待唤醒事件...\n");
                
                // 等待状态变化或停止信号
                WaitForSingleObject(m_stateChangeEvent, INFINITE);
                ResetEvent(m_stateChangeEvent);

                // 清理唤醒资源
                if (m_wakeupInitialized) {
                    Ivw70Uninit();
                    m_wakeupInitialized = false;
                }

                // 重置连续失败计数（正常流程）
                m_consecutiveFailures.store(0);
            }
            break;            case STATE_COMMAND_LISTENING:
            {
                if (!m_sdkInitialized) {
                    m_sdkInitialized = AIKITDLL::SafeInitSDK();
                    if (!m_sdkInitialized) {
                        AIKITDLL::LogError("SDK重新初始化失败，尝试恢复...\n");
                        // 回到空闲状态，避免循环
                        TransitionToState(STATE_IDLE);
                        Sleep(2000);
                        continue;
                    }
                }

                AIKITDLL::LogDebug("开始命令词识别...\n");

                // 重置ESR状态
                ResetEsrStatus();

                // 启动命令词识别 - 函数没有返回值，无法直接检查成功或失败
                EsrMicrophone(cbs);

                // 设置命令词识别超时事件
                HANDLE timeoutEvent = CreateWaitableTimer(NULL, TRUE, NULL);
                if (timeoutEvent == NULL) {
                    AIKITDLL::LogError("创建定时器失败，错误码：%d\n", GetLastError());
                    TransitionToState(STATE_WAKEUP_LISTENING);
                    continue;
                }

                LARGE_INTEGER dueTime;
                dueTime.QuadPart = -10000LL * MAX_COMMAND_WAIT_TIME; // 100纳秒为单位
                SetWaitableTimer(timeoutEvent, &dueTime, 0, NULL, NULL, FALSE);

                // 等待状态变化、超时或停止信号
                HANDLE events[2] = { m_stateChangeEvent, timeoutEvent };
                DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);

                // 清理定时器
                CloseHandle(timeoutEvent);

                // 处理超时情况
                if (waitResult == WAIT_OBJECT_0 + 1) {
                    AIKITDLL::LogWarning("命令词识别超时\n");
                    // 触发超时事件
                    HandleEvent(EVENT_ESR_TIMEOUT, "超时");
                }
                else if (waitResult == WAIT_FAILED) {
                    AIKITDLL::LogError("等待事件失败，错误码：%d\n", GetLastError());
                    // 发生错误时，回到唤醒状态
                    TransitionToState(STATE_WAKEUP_LISTENING);
                }

                ResetEvent(m_stateChangeEvent);
            }
            break;

            case STATE_PROCESSING:
                // 处理命令词，此处暂不实现具体逻辑
                AIKITDLL::LogDebug("处理识别到的命令词...\n");

                // 模拟处理命令的过程
                Sleep(500);

                // 处理完命令后回到唤醒监听状态
                TransitionToState(STATE_WAKEUP_LISTENING);
                break;
            } // switch 结束
        }
        catch (const std::exception& e) {
            AIKITDLL::LogError("发生异常：%s\n", e.what());
            // 发生异常时进行资源清理和重置
            ResetSDKState();
            TransitionToState(STATE_IDLE);
            Sleep(5000);
        }
    }

    // 线程退出前清理资源
    ResetSDKState();
    AIKITDLL::LogDebug("语音助手控制线程退出\n");
}

// 重置SDK状态和资源
void VoiceStateManager::ResetSDKState() {
    // 避免重入,先检查状态
    if (!m_sdkInitialized.load(std::memory_order_acquire) && 
        !m_wakeupInitialized.load(std::memory_order_acquire)) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    // 获取当前状态用于日志
    VOICE_ASSISTANT_STATE currentState = m_currentState.load(std::memory_order_acquire);
    int failures = m_consecutiveFailures.load(std::memory_order_acquire);
        
    AIKITDLL::LogDebug("开始重置SDK状态和资源...\n");
    
    try {
        // 清理唤醒资源
        if (m_wakeupInitialized) {
            AIKITDLL::LogDebug("清理唤醒资源...\n");
            Ivw70Uninit();
            m_wakeupInitialized = false;
        }
        
        // 清理ESR资源
        AIKITDLL::LogDebug("清理ESR资源...\n");
        ResetEsrStatus();
        
        // 清理SDK
        if (m_sdkInitialized) {
            AIKITDLL::LogDebug("清理SDK资源...\n");
            AIKITDLL::SafeCleanupSDK();
            m_sdkInitialized = false;
        }
        
        // 重置错误计数
        m_consecutiveFailures.store(0);
        
        // 强制等待一小段时间，确保资源完全释放
        Sleep(100);
        
        AIKITDLL::LogDebug("SDK状态和资源重置完成\n");
    }
    catch (const std::exception& e) {
        AIKITDLL::LogError("重置SDK状态时发生异常: %s\n", e.what());
        // 确保计数和状态被重置
        m_consecutiveFailures.store(0);
        m_sdkInitialized = false;
        m_wakeupInitialized = false;
    }
}

// 导出函数实现

int StartVoiceAssistantLoop() {
    try {
        // 获取实例并启动语音助手
        VoiceStateManager* manager = VoiceStateManager::GetInstance();
        if (!manager) {
            AIKITDLL::LogError("获取VoiceStateManager实例失败\n");
            return -1;
        }
        
        bool success = manager->StartVoiceAssistant();
        AIKITDLL::LogDebug("语音助手启动%s\n", success ? "成功" : "失败");
        return success ? 0 : -1;
    }
    catch (const std::exception& e) {
        AIKITDLL::LogError("启动语音助手时发生异常: %s\n", e.what());
        return -1;
    }
}

int StopVoiceAssistantLoop() {
	VoiceStateManager::GetInstance()->StopVoiceAssistant();
	return 0;
}

int GetVoiceAssistantState() {
	return (int)VoiceStateManager::GetInstance()->GetCurrentState();
}

const char* GetLastCommandResult() {
	return AIKITDLL::lastResult.c_str();
}
