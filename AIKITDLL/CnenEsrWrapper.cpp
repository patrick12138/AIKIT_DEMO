#include "pch.h"
#include "CnenEsrWrapper.h"
#include "EsrHelper.h"
#include <atomic>
#include <process.h>
#include <conio.h>
#include <errno.h>
#include <mutex>

// 声明ESRHelper中的函数 (新的签名)
extern "C" {
	__declspec(dllimport) int GetPlainResult(char* buffer, int bufferSize, bool* isNewResult);
}

// 定义事件类型常量
enum {
	EVT_START = 0,
	EVT_STOP,
	EVT_QUIT,
	EVT_TOTAL
};

// ESR结果状态定义
enum ESR_STATUS {
	ESR_STATUS_NONE = 0,      // 无结果
	ESR_STATUS_SUCCESS = 1,   // 识别成功
	ESR_STATUS_FAILED = 2,    // 识别失败
	ESR_STATUS_PROCESSING = 3 // 正在处理
};

// 事件句柄
static HANDLE events[EVT_TOTAL] = { NULL, NULL, NULL };

// ESR能力结果标识
namespace AIKITDLL {
	std::atomic<int> esrResultFlag(0);
	std::string lastEsrResult;
	std::atomic<int> esrStatus(ESR_STATUS_NONE); // 添加ESR状态
	std::string lastEsrKeywordResult;            // 识别到的命令词结果
	std::string lastEsrErrorInfo;                // 错误信息
	std::mutex esrResultMutex;                   // 结果保护互斥锁

	int esr_microphone(const char* abilityID)
	{
		AIKITDLL::LogInfo("正在初始化麦克风语音识别...");

		int errcode;
		HANDLE helper_thread = NULL;
		DWORD waitres;
		char isquit = 0;
		struct EsrRecognizer esr;
		DWORD startTime = GetTickCount();
		const DWORD MAX_WAIT_TIME = 10000; // 10秒超时

		// 初始化语音识别器
		errcode = EsrInit(&esr, ESR_MIC, -1);
		if (errcode) {
			AIKITDLL::LogError("语音识别器初始化失败，错误码: %d", errcode);
			return errcode;
		}

		// 创建事件句柄
		for (int i = 0; i < EVT_TOTAL; ++i) {
			events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (events[i] == NULL) {
				AIKITDLL::LogError("创建事件失败，错误码: %d", GetLastError());
				EsrUninit(&esr);
				return -1;
			}
		}

		AIKITDLL::LogInfo("开始监听语音...，进入EsrStartListening");
		errcode = EsrStartListening(&esr);
		if (errcode) {
			AIKITDLL::LogError("开始监听失败，错误码: %d", errcode);
			isquit = 1; // 标记退出
		}

		char plainResultBuffer[8192]; // 用于接收 plain 结果的缓冲区
		bool hasNewResult = false;

		while (!isquit) {
			// 调用新的 GetPlainResult
			memset(plainResultBuffer, 0, sizeof(plainResultBuffer)); // 清空缓冲区
			hasNewResult = false; // 重置标志
			int resultLen = GetPlainResult(plainResultBuffer, sizeof(plainResultBuffer) - 1, &hasNewResult);

			if (hasNewResult && resultLen > 0) {
				// 成功获取到新的 plain 结果
				// 注意：plainResultBuffer 现在包含的是 "plain: actual_result_json" 这样的格式
				// 你可能需要解析掉 "plain: " 前缀
				char* actualJson = strstr(plainResultBuffer, "plain: ");
				if (actualJson) {
					actualJson += strlen("plain: "); // 跳过 "plain: "
				}
				else {
					actualJson = plainResultBuffer; // 如果没有前缀，则直接使用
				}

				AIKITDLL::LogInfo("获取到 new plain 结果: %s (长度: %d)", actualJson, resultLen);

				std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
				AIKITDLL::lastEsrKeywordResult = actualJson; // 存储实际的JSON部分
				AIKITDLL::esrStatus = ESR_STATUS_SUCCESS;
				AIKITDLL::LogInfo("已识别到命令词: %s，准备退出监听", actualJson);

				errcode = EsrStopListening(&esr);
				if (errcode) {
					AIKITDLL::LogError("停止监听失败，错误码: %d", errcode);
				}
				isquit = 1;
				break;
			}
			else {
				// AIKITDLL::LogDebug("未获取到新的plain结果，或结果为空。hasNewResult: %s, resultLen: %d", hasNewResult ? "true" : "false", resultLen);
			}

			// 检查是否已经失败 (例如由其他逻辑设置)
			if (esrStatus.load() == ESR_STATUS_FAILED) {
				AIKITDLL::LogInfo("命令词识别已失败（由其他部分标记），准备退出监听");
				errcode = EsrStopListening(&esr);
				if (errcode) {
					AIKITDLL::LogError("停止监听失败，错误码: %d", errcode);
				}
				isquit = 1;
				break;
			}

			// 检查是否超时
			if (GetTickCount() - startTime > MAX_WAIT_TIME) {
				AIKITDLL::LogInfo("命令词识别超时，准备退出监听");
				errcode = EsrStopListening(&esr);
				if (errcode) {
					AIKITDLL::LogError("停止监听失败，错误码: %d", errcode);
				}
				std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
				AIKITDLL::esrStatus = ESR_STATUS_FAILED;
				AIKITDLL::lastEsrErrorInfo = "识别超时";
				isquit = 1;
				break;
			}

			// 等待事件，设置较短的超时时间以便定期检查识别结果
			waitres = WaitForMultipleObjects(EVT_TOTAL, events, FALSE, 200); // 缩短等待时间，更频繁检查
			switch (waitres) {
			case WAIT_FAILED:
				AIKITDLL::LogError("等待事件失败，错误码: %d", GetLastError());
				isquit = 1;
				break;
			case WAIT_TIMEOUT:
				// 超时意味着没有事件发生，循环将继续，再次尝试获取结果
				break;
			case WAIT_OBJECT_0 + EVT_STOP:
				AIKITDLL::LogInfo("接收到停止事件，停止监听语音...");
				errcode = EsrStopListening(&esr);
				if (errcode) {
					AIKITDLL::LogError("停止监听失败，错误码: %d", errcode);
				}
				isquit = 1; // 标记退出
				break;
			case WAIT_OBJECT_0 + EVT_QUIT:
				AIKITDLL::LogInfo("接收到退出事件，正在退出...");
				EsrStopListening(&esr); // 尝试停止
				isquit = 1;
				break;
			default:
				// 其他事件
				break;
			}
		}

		// 清理资源
		if (helper_thread != NULL) {
			WaitForSingleObject(helper_thread, INFINITE);
			CloseHandle(helper_thread);
		}

		for (int i = 0; i < EVT_TOTAL; ++i) {
			if (events[i]) {
				CloseHandle(events[i]);
				events[i] = NULL; // 防止重复关闭
			}
		}

		EsrUninit(&esr);
		AIKITDLL::LogInfo("麦克风语音识别已结束");
		return errcode; // 返回最后的错误码
	}
}

// ESR初始化能力与引擎
int CnenEsrInit()
{
	// 注册回调
	AIKIT_Callbacks cbs = { AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError };

	AIKITDLL::LogInfo("正在注册ESR能力回调");

	int ret = AIKIT::AIKIT_RegisterAbilityCallback(ESR_ABILITY, cbs);
	if (ret != 0) {
		AIKITDLL::LogError("注册能力回调失败，错误码: %d", ret);
		return -1;
	}
	AIKITDLL::LogInfo("注册能力回调成功");

	// 检查工作目录
	char currentDir[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, currentDir);
	AIKITDLL::LogInfo("当前工作目录: %s", currentDir);

	// 确保引擎 DLL 已加载
	if (!AIKITDLL::EnsureEngineDllsLoaded()) {
		AIKITDLL::LogError("引擎动态库加载失败");
		return -1;
	}
	AIKITDLL::LogInfo("引擎动态库加载检查通过");

	// 初始化引擎
	AIKIT::AIKIT_ParamBuilder* engine_paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
	if (engine_paramBuilder == nullptr) {
		AIKITDLL::LogError("创建ParamBuilder失败");
		return -1;
	}

	engine_paramBuilder->clear();
	engine_paramBuilder->param("decNetType", "fsa", strlen("fsa"));
	engine_paramBuilder->param("punishCoefficient", 0.0);
	engine_paramBuilder->param("wfst_addType", 0);        // 0-中文，1-英文

	AIKITDLL::LogInfo("正在初始化ESR引擎...");
	ret = AIKIT::AIKIT_EngineInit(ESR_ABILITY, AIKIT::AIKIT_Builder::build(engine_paramBuilder));
	if (ret != 0) {
		AIKITDLL::LogError("AIKIT_EngineInit 失败，错误码: %d", ret);
		delete engine_paramBuilder;
		return ret;
	}
	AIKITDLL::LogInfo("ESR引擎初始化成功");

	// 加载词汇表数据
	AIKIT::AIKIT_CustomBuilder* customBuilder = AIKIT::AIKIT_CustomBuilder::create();
	if (customBuilder == nullptr) {
		AIKITDLL::LogError("创建CustomBuilder失败");
		delete engine_paramBuilder;
		return -1;
	}

	customBuilder->clear();
	customBuilder->textPath("FSA", ".\\resource\\cnenesr\\fsa\\cn_fsa.txt", 0);

	AIKITDLL::LogInfo("正在加载FSA数据...");
	ret = AIKIT::AIKIT_LoadData(ESR_ABILITY, AIKIT::AIKIT_Builder::build(customBuilder));
	if (ret != 0) {
		AIKITDLL::LogError("AIKIT_LoadData 失败，错误码: %d", ret);
		delete engine_paramBuilder;
		delete customBuilder;
		return ret;
	}
	AIKITDLL::LogInfo("FSA数据加载成功");

	// 清理资源
	delete engine_paramBuilder;
	delete customBuilder;

	return 0;
}

// ESR资源释放
int CnenEsrUninit()
{
	AIKITDLL::LogInfo("正在释放ESR资源...");

	// 卸载FSA数据
	int ret = AIKIT::AIKIT_UnLoadData(ESR_ABILITY, "FSA", 0);
	if (ret != 0) {
		AIKITDLL::LogWarning("卸载FSA数据时出现警告，错误码: %d", ret);
	}

	AIKITDLL::LogInfo("ESR资源释放完成");
	return 0;
}

// 麦克风输入的ESR测试函数
extern "C" __declspec(dllexport) int StartEsrMicrophoneDetection()
{
	// 检查是否已有识别会话在运行
	if (AIKITDLL::esrStatus.load() == ESR_STATUS_PROCESSING) {
		AIKITDLL::LogError("已有ESR识别会话在运行");
		return -1;
	}

	int ret = AIKITDLL::InitializeAIKitSDK();
	if (ret != 0) {
		AIKITDLL::LogError("AIKit SDK 初始化失败，错误码: %d", ret);
		return -1;
	}

	AIKITDLL::LogInfo("======================= WPF调用ESR麦克风检测开始 ===========================");

	// 启动麦克风识别线程，避免阻塞主线程
	try {
		// 设置状态为处理中
		AIKITDLL::esrStatus = ESR_STATUS_PROCESSING;

		// 开始麦克风识别
		AIKITDLL::LogInfo("开始从麦克风获取音频数据");
		int ret = AIKITDLL::esr_microphone(ESR_ABILITY);
		if (ret != 0) {
			AIKITDLL::LogError("麦克风处理失败，错误码: %d", ret);
			AIKITDLL::esrStatus = ESR_STATUS_FAILED;
		}
		else {
			if (AIKITDLL::esrStatus.load() != ESR_STATUS_SUCCESS) {
				if (AIKITDLL::esrStatus.load() == ESR_STATUS_PROCESSING) {
					AIKITDLL::LogInfo("麦克风处理已结束，但未找到明确结果");
					AIKITDLL::esrStatus = ESR_STATUS_FAILED;
					AIKITDLL::lastEsrErrorInfo = "未找到明确的命令词结果";
				}
			}
			else {
				AIKITDLL::LogInfo("麦克风处理成功");
			}
		}
	}
	catch (const std::exception& e) {
		AIKITDLL::LogError("发生异常: %s", e.what());
		AIKITDLL::esrStatus = ESR_STATUS_FAILED;
	}
	catch (...) {
		AIKITDLL::LogError("发生未知异常");
		AIKITDLL::esrStatus = ESR_STATUS_FAILED;
	}
exit:
	// 释放资源
	CnenEsrUninit();


	return 0;
}

// 获取ESR状态
extern "C" __declspec(dllexport) int GetEsrStatus()
{
	return AIKITDLL::esrStatus.load();
}

// 重置ESR状态
extern "C" __declspec(dllexport) void ResetEsrStatus()
{
	std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
	AIKITDLL::esrStatus = ESR_STATUS_NONE;
	AIKITDLL::lastEsrKeywordResult.clear();
	AIKITDLL::lastEsrErrorInfo.clear();
}

// 获取ESR命令词结果
extern "C" __declspec(dllexport) const char* GetEsrKeywordResult()
{
	std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
	// 返回静态缓冲区，确保字符串不会在函数返回后被销毁
	static char resultBuffer[1024] = { 0 };
	memset(resultBuffer, 0, sizeof(resultBuffer));
	strcpy_s(resultBuffer, sizeof(resultBuffer), AIKITDLL::lastEsrKeywordResult.c_str());
	return resultBuffer;
}

// 获取ESR错误信息
extern "C" __declspec(dllexport) const char* GetEsrErrorInfo()
{
	std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
	static char errorBuffer[1024] = { 0 };
	memset(errorBuffer, 0, sizeof(errorBuffer));
	strcpy_s(errorBuffer, sizeof(errorBuffer), AIKITDLL::lastEsrErrorInfo.c_str());
	return errorBuffer;
}