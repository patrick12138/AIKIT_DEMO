#include "pch.h"
#include "Common.h"
#include "IvwWrapper.h"
#include <string.h>
#include <aikit_constant.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C"
{
#endif
	AIKITDLL_API int InitializeAIKitSDK()
	{
		// 设置需要使用的能力ID (唤醒和合成能力)
		const char *ability_id = "e867a88f2;e75f07b62";

		if (strlen(ability_id) == 0)
		{
			AIKITDLL::LogError("ability_id is empty!!\n");
			AIKITDLL::lastResult = "ERROR: Ability ID is empty. Please provide valid ability IDs.";
			return -1;
		}

		// 输出详细日志信息
		AIKITDLL::LogDebug("开始初始化AIKIT SDK基础功能组件...\n");

		// 设置关键参数
		const char *appID = "80857b22";
		const char *apiSecret = "ZDkyM2RkZjcyYTZmM2VjMDM0MzVmZDJl";
		const char *apiKey = "f27d0261fb5fa1733b7e9a2190006aec";
		const char *workDir = ".";

		// 配置并设置参数
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

		// SDK初始化
		int ret = AIKIT::AIKIT_Init();
		if (ret != 0)
		{
			AIKITDLL::LogError("AIKIT_Init failed, error code: %d\n", ret);
			AIKITDLL::lastResult = "ERROR: AIKIT initialization failed with code: " + std::to_string(ret);
			return -1;
		}

		AIKITDLL::LogDebug("AIKIT SDK初始化成功，开始初始化功能组件...\n");
		AIKITDLL::lastResult = "INFO: AIKIT初始化成功！";

		return 0;
	}

	AIKITDLL_API int StartWakeup()
	{
		InitializeAIKitSDK();
		AIKITDLL::LogDebug("开始启动语音唤醒...\n");

		// 设置回调函数
		AIKIT_Callbacks cbs = {AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError};

		// 初始化唤醒功能
		int ret = Ivw70Init();
		if (ret != 0)
		{
			AIKITDLL::LogError("唤醒功能初始化失败，错误码：%d\n", ret);
			AIKITDLL::lastResult = "ERROR: 唤醒功能初始化失败";
			return -1;
		}

		AIKITDLL::LogDebug("唤醒功能初始化成功\n");

		// 启动麦克风唤醒
		ret = TestIvw70Microphone(cbs);
		//ret = TestIvw70(cbs);
		if (ret != 0)
		{
			AIKITDLL::LogError("启动麦克风唤醒失败，错误码：%d\n", ret);
			AIKITDLL::lastResult = "ERROR: 启动麦克风唤醒失败";
			Ivw70Uninit();
			return -1;
		}
		Ivw70Uninit();
		AIKITDLL::LogDebug("语音唤醒启动成功\n");
		AIKITDLL::lastResult = "SUCCESS: 语音唤醒已启动";
		return 0;
	}

	AIKITDLL_API int StartEsrMicrophone()
	{
		InitializeAIKitSDK();

		// 设置回调函数
		AIKIT_Callbacks cbs = {AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError};

		AIKITDLL::LogDebug("调用TestEsr");
		TestEsrMicrophone(cbs);
		//TestEsr(cbs);

		// 清理退出操作
		AIKIT::AIKIT_UnInit();
		AIKITDLL::LogDebug("命令词识别启动成功\n");
		AIKITDLL::lastResult = "SUCCESS: 命令词识别成功";
		return 0;
	}
#ifdef __cplusplus
}
#endif