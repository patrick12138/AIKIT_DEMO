#include "pch.h"
#include "Common.h"
#include "IvwWrapper.h"
#include <string.h>
#include <aikit_constant.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif
	AIKITDLL_API int RunFullTest()
	{
		FILE* fin = nullptr;
		/**
		 * 传入的权限参数，多个能力用分号";"分隔，如"e867a88f2;ece9d3c90"
		 * 唤醒词 id = e867a88f2
		 * 中英文识别 id = e75f07b62
		 * 声音合成 id = e2e44feff
		*/
		// 设置需要使用的能力ID
		const char* ability_id = "e867a88f2;e75f07b62";  // 唤醒和合成能力ID
		if (strlen(ability_id) == 0)
		{
			printf("ability_id is empty!!\n");
			AIKITDLL::lastResult = "ERROR: Ability ID is empty. Please provide valid ability IDs.";
			return -1;
		}
		// 输出详细日志信息
		AIKITDLL::LogDebug("开始初始化AIKIT SDK基础功能组件...\n");

		// 设置关键参数
		const char* appID = "80857b22";
		const char* apiSecret = "ZDkyM2RkZjcyYTZmM2VjMDM0MzVmZDJl";
		const char* apiKey = "f27d0261fb5fa1733b7e9a2190006aec";
		const char* workDir = ".";
		const char* abilityID = IVW_ABILITY; // 使用定义好的常量

		// 配置并设置参数，SDK初始化
		auto config = AIKIT::AIKIT_Configurator::builder()
			.app()
			.appID(appID)
			.apiSecret(apiSecret)
			.apiKey(apiKey)
			.workDir(workDir)
			.auth()
			.authType(0)
			.ability(ability_id)
			.log()
			.logLevel(LOG_LVL_INFO)
			.logMode(2)
			.logPath("D:\\AIKITDLL\\aikit_wpf.log");

		int ret = AIKIT::AIKIT_Init();
		if (ret != 0) {
			printf("AIKIT_Init failed, error code: %d\n", ret);
			AIKITDLL::lastResult = "ERROR: AIKIT initialization failed with code: " + std::to_string(ret);
			return -1;
		}

		AIKITDLL::LogDebug("AIKIT SDK初始化成功，开始初始化功能组件...\n");
		AIKITDLL::lastResult = "INFO: AIKIT初始化成功！";

		// 设置回调函数
		AIKIT_Callbacks cbs = { AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError };

		AIKITDLL::LogDebug("调用Ivw70Init");
		ret = Ivw70Init();
		if (ret != 0) {
			AIKIT::AIKIT_UnInit();
			return -1;
		}
		AIKITDLL::LogDebug("INFO: Voice wake-up 初始化成功");
		AIKITDLL::LogDebug("调用麦克风唤醒");
		int ivwResult = 0;
		do {
			ivwResult = TestIvw70(cbs);
			if (ivwResult != 0) {
				AIKITDLL::LogWarning("TestIvw70 failed with code: %d, retrying...", ivwResult);
				// 可选项: 等待一段时间后重试
				Sleep(1000);  // 等待1秒后重试
			}
		} while (ivwResult != 0);

		AIKITDLL::LogInfo("TestIvw70 succeeded, proceeding to TestEsr");

		AIKITDLL::LogDebug("调用TestEsr");
		TestEsrMicrophone(cbs);

		// 确保关闭文件资源
		if (fin != nullptr) {
			fclose(fin);
			fin = nullptr;
		}

		// 清理退出操作
		Ivw70Uninit();      //需要和Ivw70Init成对出现
		AIKIT::AIKIT_UnInit();
		AIKITDLL::lastResult = "RUN ALL TEST SUCCESS!";
		return 0;
	}
#ifdef __cplusplus
}
#endif