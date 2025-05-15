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
	m_isRunning(false) {
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

// 获取单例实例
VoiceStateManager* VoiceStateManager::GetInstance() {
	if (s_instance == nullptr) {
		s_instance = new VoiceStateManager();
	}
	return s_instance;
}

// 开始语音助手循环
bool VoiceStateManager::StartVoiceAssistant() {
	std::lock_guard<std::mutex> lock(m_stateMutex);

	// 检查是否已在运行
	if (m_isRunning.load()) {
		AIKITDLL::LogWarning("语音助手已经在运行中\n");
		return false;
	}

	// 设置运行标志
	m_isRunning.store(true);

	// 设置初始状态
	m_currentState.store(STATE_IDLE);

	// 重置事件
	ResetEvent(m_stateChangeEvent);

	// 启动控制线程
	m_controlThread = std::thread(&VoiceStateManager::ControlThreadProc, this);

	// 触发初始状态变化
	TransitionToState(STATE_WAKEUP_LISTENING);

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
	// 如果状态相同，不需要转换
	if (m_currentState.load() == newState) {
		return;
	}

	// 记录状态转换
	AIKITDLL::LogDebug("语音助手状态转换: %d -> %d\n", m_currentState.load(), newState);

	// 设置新状态
	m_currentState.store(newState);

	// 触发状态变化事件
	SetEvent(m_stateChangeEvent);
}

// 处理事件回调
void VoiceStateManager::HandleEvent(AIKIT_EVENT_TYPE eventType, const char* result) {
	AIKITDLL::LogDebug("处理事件: %d, 结果: %s\n", eventType, result);

	switch (eventType) {
	case EVENT_WAKEUP_SUCCESS:
		// 唤醒成功，转到命令词识别状态
		TransitionToState(STATE_COMMAND_LISTENING);
		break;

	case EVENT_WAKEUP_FAILED:
		// 唤醒失败，重新回到唤醒监听状态
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
	std::lock_guard<std::mutex> lock(m_stateMutex);

	// 清理当前状态的资源
	switch (m_currentState.load()) {
	case STATE_WAKEUP_LISTENING:
		Ivw70Uninit();
		break;

	case STATE_COMMAND_LISTENING:
		ResetEsrStatus();
		break;

	default:
		break;
	}

	// 重置到唤醒监听状态
	TransitionToState(STATE_WAKEUP_LISTENING);
}

// 控制线程主函数
void VoiceStateManager::ControlThreadProc() {
	AIKITDLL::LogDebug("语音助手控制线程启动\n");
	// 设置回调函数
	AIKIT_Callbacks cbs = { AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError };
	
	// 只在线程开始时初始化一次SDK
	bool sdkInitialized = AIKITDLL::SafeInitSDK();
	if (!sdkInitialized) {
		AIKITDLL::LogError("SDK初始化失败，语音助手无法启动\n");
		m_isRunning.store(false);
		return;
	}

	// 记录唤醒功能初始化状态
	bool wakeupInitialized = false;
	
	// 记录连续失败次数，用于错误恢复
	int consecutiveFailures = 0;
	const int MAX_FAILURES = 3; // 最大连续失败次数

	while (m_isRunning.load()) {
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
		{			// 检查SDK是否仍然有效，如果无效则重新初始化
			if (!sdkInitialized) {
				sdkInitialized = AIKITDLL::SafeInitSDK();
				if (!sdkInitialized) {
					AIKITDLL::LogError("SDK重新初始化失败，尝试恢复...\n");
					// 回到空闲状态，避免循环
					TransitionToState(STATE_IDLE);
					Sleep(2000);
					continue;
				}
			}
			
			AIKITDLL::LogDebug("开始启动语音唤醒...\n");
			AIKITDLL::LogDebug("开始唤醒监听...\n");

			// 初始化唤醒功能（如果尚未初始化）
			if (!wakeupInitialized) {
				int ret = Ivw70Init();
				if (ret != 0) {
					AIKITDLL::LogError("唤醒功能初始化失败，错误码：%d\n", ret);
					consecutiveFailures++;
							if (consecutiveFailures >= MAX_FAILURES) {
						AIKITDLL::LogError("连续多次初始化失败，重置系统...\n");
						// 重置SDK
						AIKITDLL::SafeCleanupSDK();
						sdkInitialized = false;
						consecutiveFailures = 0;
						// 回到空闲状态，避免循环
						TransitionToState(STATE_IDLE);
					}
					// 延迟一段时间后重试
					Sleep(2000);
					continue;
				}
				wakeupInitialized = true;
				consecutiveFailures = 0;
			}

			// 启动麦克风唤醒
			int ret = TestIvw70Microphone(cbs);
			if (ret != 0) {
				AIKITDLL::LogError("启动麦克风唤醒失败，错误码：%d\n", ret);
				// 清理唤醒资源并标记为未初始化，以便下次重新初始化
				Ivw70Uninit();
				wakeupInitialized = false;
				consecutiveFailures++;
						if (consecutiveFailures >= MAX_FAILURES) {
					AIKITDLL::LogError("连续多次启动失败，重置系统...\n");
					// 重置SDK
					AIKITDLL::SafeCleanupSDK();
					sdkInitialized = false;
					consecutiveFailures = 0;
					// 回到空闲状态，避免循环
					TransitionToState(STATE_IDLE);
				}
				// 延迟一段时间后重试
				Sleep(2000);
				continue;
			}

			AIKITDLL::LogDebug("唤醒监听已启动，等待唤醒事件...\n");

			// 等待状态变化或停止信号
			WaitForSingleObject(m_stateChangeEvent, INFINITE);
			ResetEvent(m_stateChangeEvent);

			// 清理唤醒资源
			if (wakeupInitialized) {
				Ivw70Uninit();
				wakeupInitialized = false;
			}
			
			// 重置连续失败计数
			consecutiveFailures = 0;
		}
		break;		case STATE_COMMAND_LISTENING:
		{
			AIKITDLL::LogDebug("开始命令词识别...\n");

			// 重置ESR状态
			ResetEsrStatus();

			// 启动命令词识别 - 函数没有返回值，无法直接检查成功或失败
			TestEsrMicrophone(cbs);

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
			} else if (waitResult == WAIT_FAILED) {
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
		}
	}
	// 确保在线程退出前释放所有资源
	if (sdkInitialized) {
		if (wakeupInitialized) {
			Ivw70Uninit();
		}
		ResetEsrStatus();
		AIKITDLL::SafeCleanupSDK();
	}

	AIKITDLL::LogDebug("语音助手控制线程退出\n");
}

// 导出函数实现

AIKITDLL_API int StartVoiceAssistantLoop() {
	return VoiceStateManager::GetInstance()->StartVoiceAssistant() ? 0 : -1;
}

AIKITDLL_API int StopVoiceAssistantLoop() {
	VoiceStateManager::GetInstance()->StopVoiceAssistant();
	return 0;
}

AIKITDLL_API int GetVoiceAssistantState() {
	return (int)VoiceStateManager::GetInstance()->GetCurrentState();
}

AIKITDLL_API const char* GetLastCommandResult() {
	return AIKITDLL::lastResult.c_str();
}
