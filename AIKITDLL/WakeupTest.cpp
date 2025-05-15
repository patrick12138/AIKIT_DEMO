#include "pch.h"
#include "Common.h"
#include "IvwWrapper.h"
#include <thread>
#include <chrono>

// 用于测试唤醒状态检测功能的函数
#ifdef __cplusplus
extern "C" {
#endif

	AIKITDLL_API int TestWakeupDetection()
	{
		// 重置唤醒状态
		ResetWakeupStatus();
		AIKITDLL::LogInfo("TestWakeupDetection: 已重置唤醒状态");

		// 创建一个测试线程，模拟延迟设置唤醒标志
		std::thread testThread([]() {
			// 睡眠3秒，模拟延迟
			std::this_thread::sleep_for(std::chrono::seconds(3));
			// 设置唤醒标志
			AIKITDLL::LogInfo("TestWakeupDetection: 测试线程正在设置唤醒标志");
			std::lock_guard<std::mutex> lock(AIKITDLL::g_ivwMutex);
			AIKITDLL::wakeupFlag.store(1);
			AIKITDLL::wakeupDetected.store(true);
			AIKITDLL::lastEventType = EVENT_WAKEUP_SUCCESS;
			AIKITDLL::wakeupInfoString = "{\"rlt\":[{\"keyword\":\"小白小白\"}]}";

			// 睡眠1秒，让主线程有机会检测到唤醒
			std::this_thread::sleep_for(std::chrono::seconds(1));

			// 通知唤醒
			AIKITDLL::LogInfo("TestWakeupDetection: 测试线程正在发送唤醒通知");
			AIKITDLL::NotifyWakeupDetected();
			});

		// 主线程等待唤醒
		AIKITDLL::LogInfo("TestWakeupDetection: 主线程开始等待唤醒");
		int attempts = 0;
		bool detected = false;

		// 重复检查唤醒状态，最多10次
		while (!detected && attempts < 10) {
			attempts++;

			// 使用主动检测
			if (GetWakeupStatus() == 1) {
				AIKITDLL::LogInfo("TestWakeupDetection: 主线程通过GetWakeupStatus检测到唤醒");
				detected = true;
				break;
			}

			// 使用条件变量等待
			if (AIKITDLL::WaitForWakeup(1000)) {
				AIKITDLL::LogInfo("TestWakeupDetection: 主线程通过WaitForWakeup检测到唤醒");
				detected = true;
				break;
			}

			AIKITDLL::LogInfo("TestWakeupDetection: 主线程等待1秒后再次检查");
		}

		// 等待测试线程结束
		if (testThread.joinable()) {
			testThread.join();
		}

		// 输出详细状态
		AIKITDLL::LogInfo("TestWakeupDetection: 测试结束，详细状态: %s", GetWakeupStatusDetails());

		return detected ? 1 : 0;
	}

#ifdef __cplusplus
}
#endif
