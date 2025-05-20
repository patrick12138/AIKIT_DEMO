#include "pch.h"
#include "Common.h"
#include "EsrHelper.h"
#include "IvwWrapper.h"
#include <string.h>
#include <aikit_constant.h>
#include <Windows.h>

namespace AIKITDLL {
	int InitializeAIKitSDK()
	{
		// 设置需要使用的能力ID (唤醒和合成能力)
		const char* ability_id = "e867a88f2;e75f07b62";

		if (strlen(ability_id) == 0)
		{
			AIKITDLL::LogError("ability_id is empty!!\n");
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
			.logPath("C:\\AIKITDLL\\aikit_wpf.log");

		// SDK初始化
		int ret = AIKIT::AIKIT_Init();
		if (ret != 0)
		{
			AIKITDLL::LogError("AIKIT_Init failed, error code: %d\n", ret);
			AIKITDLL::lastResult = "ERROR: AIKIT initialization failed with code: " + std::to_string(ret);
			return -1;
		}

		AIKITDLL::LogDebug("AIKIT SDK初始化成功，开始初始化功能组件...\n");
		AIKITDLL::lastResult = "AIKIT SDK初始化成功！";

		return 0;
	}

	int TestIvw70(const AIKIT_Callbacks& cbs)
	{
		AIKIT::AIKIT_ParamBuilder* paramBuilder = nullptr;
		int ret = 0;

		AIKITDLL::LogInfo("======================= IVW70 测试开始 ===========================");

		// 注册能力回调
		ret = AIKIT::AIKIT_RegisterAbilityCallback(IVW_ABILITY, cbs);
		if (ret != 0) {
			AIKITDLL::LogError("注册能力回调失败，错误码: %d", ret);
			return ret;
		}
		AIKITDLL::LogInfo("注册能力回调成功");

		// 创建参数构建器
		paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
		if (!paramBuilder) {
			AIKITDLL::LogError("创建参数构建器失败");
			return -1;
		}

		// 设置参数
		paramBuilder->param("wdec_param_nCmThreshold", "0 0:900", strlen("0 0:900"));
		paramBuilder->param("gramLoad", true);
		AIKITDLL::LogInfo("参数已设置：唤醒阈值=900");

		// 默认使用音频文件进行测试
		const char* testAudioPath = ".\\resource\\ivw70\\testAudio\\xbxb.pcm";
		AIKITDLL::LogInfo("开始从文件测试唤醒功能：%s", testAudioPath);

		ret = AIKITDLL::ivw_file(IVW_ABILITY, testAudioPath, 100);

		if (ret == 0) {
			AIKITDLL::LogInfo("唤醒测试成功，检测到唤醒词");
		}
		else {
			AIKITDLL::LogError("唤醒测试失败，未检测到唤醒词，错误码: %d", ret);
		}

		// 清理资源
		if (paramBuilder != nullptr) {
			delete paramBuilder;
			paramBuilder = nullptr;
		}

		AIKITDLL::LogInfo("======================= IVW70 测试结束 ===========================");
		return ret;
	}

	int TestIvw70Microphone(const AIKIT_Callbacks& cbs)
	{
		int ret = 0;

		AIKITDLL::LogInfo("======================= IVW70 麦克风输入开启 ===========================");

		// 注册能力回调
		ret = AIKIT::AIKIT_RegisterAbilityCallback(IVW_ABILITY, cbs);
		if (ret != 0) {
			AIKITDLL::LogError("注册能力回调失败，错误码: %d", ret);
			return ret;
		}
		AIKITDLL::LogInfo("注册能力回调成功");

		// 使用麦克风进行测试
		AIKITDLL::LogInfo("开始从麦克风测试唤醒功能");

		ret = AIKITDLL::ivw_microphone(IVW_ABILITY, 900, 10000); // 10秒超时

		if (ret == 0) {
			AIKITDLL::LogInfo("麦克风唤醒测试成功，检测到唤醒词");
		}
		else {
			AIKITDLL::LogError("麦克风唤醒测试失败，未检测到唤醒词，错误码: %d", ret);
		}

		AIKITDLL::LogInfo("======================= IVW70 麦克风输出结束 ===========================");
		return ret;
	}
}

#ifdef __cplusplus
extern "C"
{
#endif
	AIKITDLL_API int StartWakeup()
	{
		AIKITDLL::InitializeAIKitSDK();
		AIKITDLL::LogDebug("开始启动语音唤醒...\n");

		// 设置回调函数
		AIKIT_Callbacks cbs = { AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError };

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
		ret = AIKITDLL::TestIvw70Microphone(cbs);
		//ret = AIKITDLL::TestIvw70(cbs);
		if (ret != 0)
		{
			AIKITDLL::LogError("启动麦克风唤醒失败，错误码：%d\n", ret);
			AIKITDLL::lastResult = "ERROR: 启动麦克风唤醒失败";
			Ivw70Uninit();
			return -1;
		}

		AIKITDLL::LogDebug("语音唤醒麦克风流程已完成\n");
		AIKITDLL::lastResult = "语音唤醒麦克风流程已完成";
		return 0;
	}

	AIKITDLL_API int StartEsrMicrophone()
	{
		AIKITDLL::InitializeAIKitSDK();

		// 设置回调函数
		AIKIT_Callbacks cbs = { AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError };

		AIKITDLL::LogDebug("调用TestEsr");
		//TestEsr(cbs);
		TestEsrMicrophone(cbs);

		// 清理退出操作
		AIKIT::AIKIT_UnInit();
		AIKITDLL::lastResult = "命令词麦克风唤醒流程完成";
		return 0;
	}

#ifdef __cplusplus
}
#endif