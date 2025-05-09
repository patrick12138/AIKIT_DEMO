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
		 * 配置授权离线能力，如果需配置多个能力，可以用";"分隔开, 如"e867a88f2;ece9d3c90"
		 * 语音唤醒 id = e867a88f2
		 * 离线命令词识别 id = e75f07b62
		 * 离线语音合成 id = e2e44feff
		*/
		// 添加所有需要使用的能力ID
		const char* ability_id = "e867a88f2;e75f07b62";  // 添加语音合成能力ID
		if (strlen(ability_id) == 0)
		{
			printf("ability_id is empty!!\n");
			AIKITDLL::lastResult = "ERROR: Ability ID is empty. Please provide valid ability IDs.";
			return -1;
		}
		// 设置详细的日志输出
		AIKITDLL::LogDebug("开始初始化AIKIT SDK和语音唤醒引擎...\n");

		// 定义关键参数
		const char* appID = "80857b22";
		const char* apiSecret = "ZDkyM2RkZjcyYTZmM2VjMDM0MzVmZDJl";
		const char* apiKey = "f27d0261fb5fa1733b7e9a2190006aec";
		const char* workDir = ".";
		const char* abilityID = IVW_ABILITY; // 使用定义好的常量

		// 构建配置并完成SDK初始化
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
			.logPath("D:\\AIKITDLL\\aikit.log");

		int ret = AIKIT::AIKIT_Init();
		if (ret != 0) {
			printf("AIKIT_Init failed, error code: %d\n", ret);
			AIKITDLL::lastResult = "ERROR: AIKIT initialization failed with code: " + std::to_string(ret);
			return -1;
		}

		AIKITDLL::LogDebug("AIKIT SDK初始化成功，开始初始化语音唤醒引擎...\n");
		AIKITDLL::lastResult = "INFO: AIKIT初始化成功。";

		// 设置回调函数
		AIKIT_Callbacks cbs = { AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError };

		AIKITDLL::LogDebug("进入Ivw70Init");
		ret = Ivw70Init();
		if (ret != 0) {
			AIKIT::AIKIT_UnInit();
			return -1;
		}
		AIKITDLL::LogDebug("INFO: Voice wake-up 初始化成功");
		AIKITDLL::LogDebug("进入TestIvw70");
		int ivwResult = 0;
		do {
			AIKITDLL::LogDebug("进入TestIvw70");
			ivwResult = TestIvw70(cbs);
			if (ivwResult != 0) {
				AIKITDLL::LogWarning("TestIvw70 failed with code: %d, retrying...", ivwResult);
				// 可选的: 等待一段时间再重试
				Sleep(1000);  // 等待1秒再重试
			}
		} while (ivwResult != 0);

		AIKITDLL::LogInfo("TestIvw70 succeeded, proceeding to TestEsr");

		AIKITDLL::LogDebug("进入TestEsr");
		TestEsr(cbs);

		// 确保关闭文件资源
		if (fin != nullptr) {
			fclose(fin);
			fin = nullptr;
		}

		// 正常退出流程
		Ivw70Uninit();      //需要和Ivw70Init成对出现
		AIKITDLL::lastResult = "INFO: 语音唤醒模块已卸载。";
		AIKIT::AIKIT_UnInit();
		AIKITDLL::lastResult = "SUCCESS: 测试成功完成。所有资源已释放。";
		return 0;
	}
#ifdef __cplusplus
}
#endif