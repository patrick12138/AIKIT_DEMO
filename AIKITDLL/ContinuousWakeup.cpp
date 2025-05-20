#include "pch.h"
//#include "ContinuousWakeup.h"
//#include "IvwWrapper.h"
//#include "Common.h"
//
//namespace AIKITDLL {
//
//	// 初始化静态成员
//	ContinuousWakeupManager* ContinuousWakeupManager::s_instance = nullptr;
//	std::mutex ContinuousWakeupManager::s_mutex;
//    
//	// 持续唤醒功能的内部唤醒标志
//	std::atomic<int> continuousWakeupFlag(0);
//
//	// 全局回调函数
//	static WakeupEventCallback g_wakeupCallback = nullptr;
//	// 唤醒SDK回调
//	void OnWakeupOutput(AIKIT_HANDLE* handle, const AIKIT_OutputData* output) {
//		if (!handle || !output || !output->node || !output->node->value) {
//			LogWarning("无效的唤醒输出数据");
//			return;
//		}
//
//		// 检查结果类型
//		if (strcmp(output->node->key, "rlt") == 0) {
//			// 简单解析 JSON 结果以获取关键信息
//			std::string result((const char*)output->node->value);
//
//			// 解析唤醒词和置信度（此处简化处理，实际项目中可能需要更复杂的JSON解析）
//			std::string keyword;
//			int confidence = 0;
//
//			// 查找关键字段
//			size_t keywordPos = result.find("\"keyword\":\"");
//			if (keywordPos != std::string::npos) {
//				keywordPos += 11; // 跳过 "keyword":"
//				size_t keywordEnd = result.find("\"", keywordPos);
//				if (keywordEnd != std::string::npos) {
//					keyword = result.substr(keywordPos, keywordEnd - keywordPos);
//				}
//			}
//
//			// 查找置信度
//			size_t confidencePos = result.find("\"ncm\":");
//			if (confidencePos != std::string::npos) {
//				confidencePos += 6; // 跳过 "ncm":
//				size_t confidenceEnd = result.find(",", confidencePos);
//				if (confidenceEnd != std::string::npos) {
//					try {
//						confidence = std::stoi(result.substr(confidencePos, confidenceEnd - confidencePos));
//					}
//					catch (...) {
//						confidence = 0;
//					}
//				}
//			}
//
//			LogInfo("检测到唤醒词: %s，置信度: %d", keyword.c_str(), confidence);			// 设置内部唤醒标志（但不结束会话）
//			continuousWakeupFlag = 1;
//            
//			// 触发全局回调
//			if (g_wakeupCallback && !keyword.empty()) {
//				g_wakeupCallback(keyword.c_str(), confidence);
//			}
//            
//			// 重置唤醒标志，准备检测下一次唤醒
//			continuousWakeupFlag = 0;
//		}
//	}
//
//	void OnWakeupEvent(AIKIT_HANDLE* handle, AIKIT_EVENT eventType, const AIKIT_OutputEvent* eventValue) {
//		switch (eventType) {
//		case AIKIT_Event_Start:
//			LogDebug("唤醒处理开始");
//			break;
//		case AIKIT_Event_End:
//			LogDebug("唤醒处理结束");
//			break;
//		case AIKIT_Event_Timeout:
//			LogWarning("唤醒处理超时");
//			break;
//		default:
//			LogDebug("未知唤醒事件: %d", eventType);
//			break;
//		}
//	}
//
//	void OnWakeupError(AIKIT_HANDLE* handle, int32_t err, const char* desc) {
//		LogError("唤醒错误，错误码: %d，描述: %s", err, desc ? desc : "未知错误");
//	}
//
//	ContinuousWakeupManager::ContinuousWakeupManager()
//		: m_state(CWS_IDLE)
//		, m_keepRunning(false)
//		, m_handle(nullptr)
//		, m_recorder(nullptr)
//		, m_dataBuilder(nullptr)
//		, m_threshold(900)
//		, m_callback(nullptr) {
//		LogInfo("持续唤醒管理器已创建");
//	}
//
//	ContinuousWakeupManager::~ContinuousWakeupManager() {
//		// 确保停止所有活动
//		if (m_state != CWS_IDLE) {
//			Stop();
//		}
//		LogInfo("持续唤醒管理器已销毁");
//	}
//
//	ContinuousWakeupManager* ContinuousWakeupManager::GetInstance() {
//		if (!s_instance) {
//			std::lock_guard<std::mutex> lock(s_mutex);
//			if (!s_instance) {
//				s_instance = new ContinuousWakeupManager();
//			}
//		}
//		return s_instance;
//	}
//
//	void ContinuousWakeupManager::ReleaseInstance() {
//		std::lock_guard<std::mutex> lock(s_mutex);
//		if (s_instance) {
//			delete s_instance;
//			s_instance = nullptr;
//		}
//	}
//
//	void ContinuousWakeupManager::AudioCallback(char* data, unsigned long len, void* user_para) {
//		// 确保回调参数有效
//		if (!data || len == 0 || !user_para) {
//			return;
//		}
//
//		// 转换用户参数
//		ContinuousWakeupManager* manager = static_cast<ContinuousWakeupManager*>(user_para);
//		if (!manager || !manager->m_dataBuilder || !manager->m_handle) {
//			return;
//		}
//
//		// 清除旧数据
//		manager->m_dataBuilder->clear();
//
//		// 创建音频数据对象并送入引擎
//		auto aiAudio_raw = AIKIT::AiAudio::get("wav")->data(data, (int)len)->valid();
//		manager->m_dataBuilder->payload(aiAudio_raw);
//
//		// 写入数据到引擎
//		int ret = AIKIT::AIKIT_Write(manager->m_handle, AIKIT::AIKIT_Builder::build(manager->m_dataBuilder));
//		if (ret != 0) {
//			LogError("音频数据写入失败，错误码: %d", ret);
//		}
//	}
//	void ContinuousWakeupManager::Cleanup() {
//		LogDebug("开始清理资源...");
//
//		// 停止录音
//		if (m_recorder) {
//			stop_record(m_recorder);
//			close_recorder(m_recorder);
//			destroy_recorder(m_recorder);
//			m_recorder = nullptr;
//			LogDebug("录音设备已关闭");
//		}
//
//		// 结束会话 - 仅在完全停止持续唤醒功能时才结束会话
//		// 这是持续唤醒功能的关键 - 在整个监听过程中保持会话活跃
//		if (m_handle && !m_keepRunning) {
//			AIKIT::AIKIT_End(m_handle);
//			m_handle = nullptr;
//			LogDebug("唤醒会话已结束");
//		}
//
//		// 清理数据构建器
//		if (m_dataBuilder) {
//			delete m_dataBuilder;
//			m_dataBuilder = nullptr;
//			LogDebug("数据构建器已清理");
//		}
//
//		LogDebug("资源清理完成");
//	}
//
//	void ContinuousWakeupManager::WorkerThread() {
//		LogInfo("持续唤醒工作线程已启动");
//		// 创建数据构建器
//		m_dataBuilder = AIKIT::AIKIT_DataBuilder::create();
//		if (!m_dataBuilder) {
//			LogError("创建数据构建器失败");
//			m_state = CWS_ERROR;
//			return;
//		}
//
//		// 注册SDK回调
//		AIKIT_Callbacks callbacks = { OnWakeupOutput, OnWakeupEvent, OnWakeupError };
//		int ret = AIKIT::AIKIT_RegisterAbilityCallback(IVW_ABILITY, callbacks);
//		if (ret != 0) {
//			LogError("注册唤醒回调函数失败，错误码: %d", ret);
//			m_state = CWS_ERROR;
//			delete m_dataBuilder;
//			m_dataBuilder = nullptr;
//			return;
//		}
//
//		// 启动唤醒会话
//		ret = ivw_start_session(IVW_ABILITY, &m_handle, m_threshold);
//		if (ret != 0) {
//			LogError("启动唤醒会话失败，错误码: %d", ret);
//			m_state = CWS_ERROR;
//			delete m_dataBuilder;
//			m_dataBuilder = nullptr;
//			return;
//		}
//
//		// 创建录音设备
//		ret = create_recorder(&m_recorder, AudioCallback, this);
//		if (ret != 0 || !m_recorder) {
//			LogError("创建录音设备失败，错误码: %d", ret);
//			m_state = CWS_ERROR;
//			AIKIT::AIKIT_End(m_handle);
//			m_handle = nullptr;
//			delete m_dataBuilder;
//			m_dataBuilder = nullptr;
//			return;
//		}
//
//		// 设置音频格式
//		WAVEFORMATEX waveform;
//		waveform.wFormatTag = WAVE_FORMAT_PCM;
//		waveform.nSamplesPerSec = 16000;
//		waveform.wBitsPerSample = 16;
//		waveform.nChannels = 1;
//		waveform.nAvgBytesPerSec = 16000 * 2;
//		waveform.nBlockAlign = 2;
//		waveform.cbSize = 0;
//
//		// 打开录音设备
//		//ret = open_recorder(m_recorder, get_default_input_dev(), &waveform);
//		if (ret != 0) {
//			LogError("打开录音设备失败，错误码: %d", ret);
//			m_state = CWS_ERROR;
//			destroy_recorder(m_recorder);
//			m_recorder = nullptr;
//			AIKIT::AIKIT_End(m_handle);
//			m_handle = nullptr;
//			delete m_dataBuilder;
//			m_dataBuilder = nullptr;
//			return;
//		}
//
//		// 开始录音
//		ret = start_record(m_recorder);
//		if (ret != 0) {
//			LogError("启动录音失败，错误码: %d", ret);
//			m_state = CWS_ERROR;
//			close_recorder(m_recorder);
//			destroy_recorder(m_recorder);
//			m_recorder = nullptr;
//			AIKIT::AIKIT_End(m_handle);
//			m_handle = nullptr;
//			delete m_dataBuilder;
//			m_dataBuilder = nullptr;
//			return;
//		}
//
//		// 更新状态为正在监听
//		m_state = CWS_LISTENING;
//		LogInfo("持续唤醒监听已启动，阈值: %d", m_threshold);
//		// 持续运行直到收到停止信号
//		while (m_keepRunning) {
//			// 检查会话状态是否仍然有效
//			if (!m_handle) {
//				LogError("唤醒会话句柄已失效，持续唤醒中断");
//				m_state = CWS_ERROR;
//				break;
//			}
//            
//			// 检查录音设备是否仍然在工作
//			if (!m_recorder || is_record_stopped(m_recorder)) {
//				LogError("录音设备已停止工作，持续唤醒中断");
//				m_state = CWS_ERROR;
//				break;
//			}
//            
//			std::this_thread::sleep_for(std::chrono::milliseconds(100));
//		}
//
//		// 更新状态为正在停止
//		m_state = CWS_STOPPING;
//		LogInfo("持续唤醒正在停止...");
//
//		// 清理资源 - 此时m_keepRunning已经为false，会正确关闭会话
//		Cleanup();
//
//		// 更新状态为空闲
//		m_state = CWS_IDLE;
//
//		LogInfo("持续唤醒工作线程已退出");
//	}
//
//	int ContinuousWakeupManager::Start(int threshold, WakeupEventCallback callback) {
//		// 检查当前状态
//		ContinuousWakeupState currentState = m_state.load();
//		if (currentState != CWS_IDLE) {
//			LogError("无法启动持续唤醒：已在运行中或处于错误状态 (状态: %d)", currentState);
//			return -1;
//		}
//
//		// 更新参数
//		m_threshold = threshold;
//		m_callback = callback;
//		g_wakeupCallback = callback;
//
//		// 更新状态为正在启动
//		m_state = CWS_STARTING;
//		m_keepRunning = true;
//
//		// 启动工作线程
//		try {
//			m_workerThread = std::thread(&ContinuousWakeupManager::WorkerThread, this);
//			LogInfo("持续唤醒检测已启动，阈值: %d", threshold);
//			return 0;
//		}
//		catch (const std::exception& e) {
//			LogError("启动唤醒工作线程失败: %s", e.what());
//			m_state = CWS_IDLE;
//			m_keepRunning = false;
//			return -1;
//		}
//	}
//	int ContinuousWakeupManager::Stop() {
//		// 检查当前状态
//		ContinuousWakeupState currentState = m_state.load();
//		if (currentState == CWS_IDLE) {
//			LogWarning("持续唤醒已处于停止状态");
//			return 0;
//		}
//		
//		LogInfo("正在停止持续唤醒检测...");
//		
//		// 先设置停止标志
//		m_keepRunning = false;
//		
//		// 等待工作线程结束 - 这将触发Cleanup()并正确关闭会话
//		if (m_workerThread.joinable()) {
//			try {
//				m_workerThread.join();
//				LogDebug("工作线程已正常结束");
//			} catch (const std::exception& e) {
//				LogError("等待工作线程结束时出错: %s", e.what());
//				// 即使发生异常，也确保进入停止状态
//				m_state = CWS_IDLE;
//				return -1;
//			}
//		}
//		
//		// 确保状态一致性
//		if (m_state.load() != CWS_IDLE) {
//			LogWarning("工作线程结束后状态不一致，强制设置为空闲状态");
//			m_state = CWS_IDLE;
//		}
//		
//		LogInfo("持续唤醒检测已停止");
//		return 0;
//	}
//
//	ContinuousWakeupState ContinuousWakeupManager::GetState() const {
//		return m_state.load();
//	}
//
//} // namespace AIKITDLL
//
//// 导出函数实现
//
//// 唤醒事件回调函数实例
//static void DefaultWakeupCallback(const char* keyword, int confidence) {
//	AIKITDLL::LogInfo("持续唤醒：检测到唤醒词 \"%s\"，置信度: %d", keyword, confidence);
//	// 这里可以添加唤醒成功后的默认行为
//	AIKITDLL::lastResult = "唤醒成功：" + std::string(keyword);
//}
//
//// 开始持续唤醒检测
//AIKITDLL_API int StartContinuousWakeupDetection(int threshold) {
//	AIKITDLL::LogInfo("启动持续唤醒检测，阈值: %d", threshold);
//	return AIKITDLL::ContinuousWakeupManager::GetInstance()->Start(threshold, DefaultWakeupCallback);
//}
//
//// 停止持续唤醒检测
//AIKITDLL_API int StopContinuousWakeupDetection() {
//	AIKITDLL::LogInfo("停止持续唤醒检测");
//	return AIKITDLL::ContinuousWakeupManager::GetInstance()->Stop();
//}
//
//// 获取持续唤醒状态
//AIKITDLL_API int GetContinuousWakeupState() {
//	return static_cast<int>(AIKITDLL::ContinuousWakeupManager::GetInstance()->GetState());
//}
//
//// 使用自定义回调启动持续唤醒检测
//AIKITDLL_API int StartContinuousWakeupDetectionWithCallback(int threshold, WakeupEventCallback callback) {
//	if (!callback) {
//		AIKITDLL::LogError("无效的回调函数");
//		return -1;
//	}
//	AIKITDLL::LogInfo("使用自定义回调启动持续唤醒检测，阈值: %d", threshold);
//	return AIKITDLL::ContinuousWakeupManager::GetInstance()->Start(threshold, callback);
//}