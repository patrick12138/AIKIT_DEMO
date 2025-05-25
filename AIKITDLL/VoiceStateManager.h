#ifndef VOICE_STATE_MANAGER_H
#define VOICE_STATE_MANAGER_H

#include <atomic>
#include <string>
#include <mutex>

namespace AIKITDLL {
	enum VoiceState {
		Idle,                // 待机状态（监听唤醒词）
		WakeupDetected,      // 检测到唤醒词
		PlayingPrompt,       // 播放提示音
		ListeningCommand,    // 监听命令词
		CommandCompleted,    // 命令完成
		Timeout,             // 超时状态
		Error,               // 错误状态
		
		// 保持兼容性的旧值
		WAKEUP_LISTENING = Idle,
		COMMAND_RECOGNITION = ListeningCommand
	};
	class VoiceStateManager {
	public:
		VoiceStateManager();
		~VoiceStateManager();

		// 获取单例实例
		static VoiceStateManager* GetInstance();

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
		
		// 单例模式
		static VoiceStateManager* instance;
		static std::mutex instance_mutex;
	};
}

#endif // VOICE_STATE_MANAGER_H