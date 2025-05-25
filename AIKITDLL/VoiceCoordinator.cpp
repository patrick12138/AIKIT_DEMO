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
extern int wakeupFlag;               // 全局变量，定义在Common.cpp中
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
	}    int VoiceCoordinator::StartVoiceInteraction(int wakeupThreshold, int esrTimeout) {
		std::lock_guard<std::mutex> lock(state_mutex_);

		// 检查是否已经在运行
		if (is_running_.load()) {
			LogWarning("语音交互循环已在运行，跳过重复启动");
			return 0; // 返回成功，避免上层认为是错误
		}

		// SDK初始化
		int sdkInitRet = InitializeAIKitSDK();
		if (sdkInitRet != 0) {
			LogError("AIKIT SDK 初始化失败，错误码: %d", sdkInitRet);
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

		LogInfo("SDK已准备就绪，开始语音交互循环");

		// 启动循环线程
		try {
			is_running_ = true;
			loop_thread_ = std::thread(&VoiceCoordinator::VoiceInteractionLoop, this);
			LogInfo("语音交互循环线程已启动");
			return 0;
		}
		catch (const std::exception& e) {
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
			}
			catch (const std::exception& e) {
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

			}
			catch (const std::exception& e) {
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

		// 检查AudioManager超时
		AudioManager::GetInstance().CheckTimeout();

		// 检查命令词状态
		if (CheckCommandStatus()) {
			LogInfo("命令词识别完成！");
			TransitionToState(VoiceState::CommandCompleted);
		}
	}	void VoiceCoordinator::HandleCommandCompleted() {
		LogInfo("处理命令词完成状态");

		// 停止命令词识别
		StopCommandRecognition();

		// **添加**：清理音频缓冲区，确保下次唤醒检测的纯净性
		LogInfo("命令完成状态下清理音频缓冲区");
		AudioManager::GetInstance().ClearAudioBuffers();

		// 确保句柄被清理，强制重新启动唤醒检测
		unified_handle_ = nullptr;

		// 适当延长延迟时间，确保缓冲区清理完成
		std::this_thread::sleep_for(std::chrono::milliseconds(1200));

		LogInfo("命令处理完成，准备返回唤醒监听状态");

		// 返回唤醒监听
		TransitionToState(VoiceState::Idle);
	}void VoiceCoordinator::HandleTimeout() {
		LogInfo("处理超时状态");

		// 清理当前会话
		StopCommandRecognition();

		// **关键修复**：清理音频缓冲区，防止残留音频数据造成误检测
		LogInfo("超时状态下清理音频缓冲区，防止误检测");
		AudioManager::GetInstance().ClearAudioBuffers();

		// 延长延迟时间，确保缓冲区清理完成
		std::this_thread::sleep_for(std::chrono::milliseconds(800));

		// 确保句柄被清理，强制重新启动唤醒检测
		unified_handle_ = nullptr;

		LogInfo("超时处理完成，准备返回唤醒监听状态");

		// 返回唤醒监听
		TransitionToState(VoiceState::Idle);
	}void VoiceCoordinator::HandleError() {
		LogError("处理错误状态: %s", last_error_.c_str());

		// 清理当前会话资源（不是全局SDK）
		CleanupCurrentSession();

		// **添加**：清理音频缓冲区，防止错误状态下的音频残留
		LogInfo("错误状态下清理音频缓冲区");
		AudioManager::GetInstance().ClearAudioBuffers();

		// 确保句柄被清理，强制重新启动唤醒检测
		unified_handle_ = nullptr;

		// 延长延迟时间，确保彻底清理
		std::this_thread::sleep_for(std::chrono::milliseconds(1200));

		// 重置错误状态，返回待机状态重新开始会话
		LogInfo("错误恢复完成，准备返回唤醒监听状态");
		last_error_.clear();
		TransitionToState(VoiceState::Idle);
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
			"wav" // 统一使用"audio"
		);

		if (!activated) {
			LogError("激活AudioManager IVW消费者失败");
			ivw_stop_session(unified_handle_);
			unified_handle_ = nullptr;
			return -1;
		}
		// 重置唤醒标志        AIKITDLL::wakeupFlag = 0;
		::wakeupFlag = 0; // 也重置全局变量
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
	}    bool VoiceCoordinator::CheckWakeupStatus() {
		// 检查全局唤醒标志（两个变量都检查）
		if (AIKITDLL::wakeupFlag.load() == 1 || ::wakeupFlag == 1) {
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

		// 初始化ESR模块
		//int esrInitRet = CnenEsrInit();
		//if (esrInitRet != 0) {
		//	LogError("CnenEsrInit 初始化失败: %d", esrInitRet);
		//	return -1;
		//}

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

		int index[] = { 0 };
		int ret = AIKIT::AIKIT_SpecifyDataSet(ESR_ABILITY, "FSA", index, sizeof(index) / sizeof(int));
		if (ret != 0) {
			AIKITDLL::LogError("AIKIT_SpecifyDataSet FSA 失败，错误码: %d", ret);
			delete paramBuilder;
			return ret;
		}
		AIKITDLL::LogInfo("AIKIT_SpecifyDataSet FSA 成功");

		LogInfo("正在启用ESR能力");
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
		}

		// 重置ESR状态
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

		// 检查是否成功识别到命令词
		if (AIKITDLL::esrStatus.load() == AIKITDLL::ESR_STATUS_SUCCESS_INTERNAL && !AIKITDLL::lastEsrKeywordResult.empty()) {
			return true;
		}

		// 检查是否失败或超时
		if (AIKITDLL::esrStatus.load() == AIKITDLL::ESR_STATUS_FAILED_INTERNAL ||
			AIKITDLL::esrStatus.load() == AIKITDLL::ESR_STATUS_NO_MATCH_INTERNAL) {
			LogInfo("ESR识别失败或超时，状态: %d", AIKITDLL::esrStatus.load());
			TransitionToState(VoiceState::Timeout);
			return false;
		}

		return false;
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
		AIKITDLL::wakeupFlag = 0;
		AIKITDLL::esrResultFlag = 0;
		ResetWakeupStatus();
	}

	void VoiceCoordinator::CleanupCurrentSession() {
		LogInfo("清理当前会话资源（保持SDK初始化状态）");

		// 停用所有AudioManager消费者
		AudioManager::GetInstance().DeactivateConsumer(AudioConsumer::IVW);
		AudioManager::GetInstance().DeactivateConsumer(AudioConsumer::ESR);

		// 停止当前会话（只有IVW有会话概念）
		if (unified_handle_ != nullptr) {
			// 停止IVW会话
			ivw_stop_session(unified_handle_);
			// ESR是实时处理，没有会话概念，只需要停用消费者即可
		}

		// 清理数据构建器内容但不删除对象
		if (unified_data_builder_ != nullptr) {
			unified_data_builder_->clear();
		}

		// 重置状态标志
		AIKITDLL::wakeupFlag = 0;
		::wakeupFlag = 0;
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
	}    // 手动触发函数（用于测试）
	void VoiceCoordinator::TriggerWakeupDetected() {
		LogInfo("手动触发唤醒检测");
		AIKITDLL::wakeupFlag = 1;
		::wakeupFlag = 1; // 也设置全局变量
		if (state_manager_) {
			state_manager_->setWakeupDetected(true);
		}
	}

	void VoiceCoordinator::TriggerCommandCompleted(const std::string& command) {
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

// ================================
// 全局便捷函数的实现 (extern "C")
// ================================

extern "C" {
	// 启动统一的语音交互流程
	int StartUnifiedVoiceInteraction(int wakeupThreshold, int esrTimeout) {
		try {
			AIKITDLL::LogInfo("调用 StartUnifiedVoiceInteraction，参数: wakeupThreshold=%d, esrTimeout=%d",
				wakeupThreshold, esrTimeout);

			auto& coordinator = AIKITDLL::VoiceCoordinator::GetInstance();
			int result = coordinator.StartVoiceInteraction(wakeupThreshold, esrTimeout);

			AIKITDLL::LogInfo("StartUnifiedVoiceInteraction 完成，返回码: %d", result);
			return result;
		}
		catch (const std::exception& ex) {
			AIKITDLL::LogError("StartUnifiedVoiceInteraction 异常: %s", ex.what());
			return -1;
		}
		catch (...) {
			AIKITDLL::LogError("StartUnifiedVoiceInteraction 发生未知异常");
			return -1;
		}
	}

	// 停止统一的语音交互流程
	int StopUnifiedVoiceInteraction() {
		try {
			AIKITDLL::LogInfo("调用 StopUnifiedVoiceInteraction");

			auto& coordinator = AIKITDLL::VoiceCoordinator::GetInstance();
			int result = coordinator.StopVoiceInteraction();

			AIKITDLL::LogInfo("StopUnifiedVoiceInteraction 完成，返回码: %d", result);
			return result;
		}
		catch (const std::exception& ex) {
			AIKITDLL::LogError("StopUnifiedVoiceInteraction 异常: %s", ex.what());
			return -1;
		}
		catch (...) {
			AIKITDLL::LogError("StopUnifiedVoiceInteraction 发生未知异常");
			return -1;
		}
	}

	// 获取统一语音交互状态
	int GetUnifiedVoiceState() {
		try {
			auto& coordinator = AIKITDLL::VoiceCoordinator::GetInstance();
			AIKITDLL::VoiceState state = coordinator.GetCurrentState();

			// 将枚举转换为整数返回
			int stateValue = static_cast<int>(state);
			AIKITDLL::LogInfo("GetUnifiedVoiceState 返回状态: %d", stateValue);
			return stateValue;
		}
		catch (const std::exception& ex) {
			AIKITDLL::LogError("GetUnifiedVoiceState 异常: %s", ex.what());
			return -1;
		}
		catch (...) {
			AIKITDLL::LogError("GetUnifiedVoiceState 发生未知异常");
			return -1;
		}
	}

	// 检查统一语音交互是否运行
	int IsUnifiedVoiceInteractionRunning() {
		try {
			auto& coordinator = AIKITDLL::VoiceCoordinator::GetInstance();
			bool isRunning = coordinator.IsRunning();

			int result = isRunning ? 1 : 0;
			AIKITDLL::LogInfo("IsUnifiedVoiceInteractionRunning 返回: %d", result);
			return result;
		}
		catch (const std::exception& ex) {
			AIKITDLL::LogError("IsUnifiedVoiceInteractionRunning 异常: %s", ex.what());
			return -1;
		}
		catch (...) {
			AIKITDLL::LogError("IsUnifiedVoiceInteractionRunning 发生未知异常");
			return -1;
		}
	}
}
