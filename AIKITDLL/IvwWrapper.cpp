#include "pch.h"
#include "IvwWrapper.h"
#include <atomic>
#include <aikit_constant.h>
#include <thread>
#include "AudioManager.h"
#include "aikit_biz_builder.h" // Ensure AIKIT::AIKIT_DataBuilder is known

namespace AIKITDLL {
	std::atomic<int> wakeupFlag(0);

	// 开启语音唤醒会话
	int ivw_start_session(const char* abilityID, AIKIT_HANDLE** outHandle, int threshold) {
		int ret = 0;

		// 创建参数构建器
		AIKIT::AIKIT_ParamBuilder* paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
		if (!paramBuilder) {
			LogError("创建参数构建器失败");
			return -1;
		}
		LogInfo("参数构建器创建成功");

		// 设置参数
		std::string thresholdParam = "0 0:" + std::to_string(threshold);
		paramBuilder->param("wdec_param_nCmThreshold", thresholdParam.c_str(), thresholdParam.length());
		paramBuilder->param("gramLoad", true);
		LogInfo("已设置唤醒阈值: %s", thresholdParam.c_str());

		// 启动能力
		LogInfo("正在启动能力...");
		ret = AIKIT::AIKIT_Start(abilityID, AIKIT::AIKIT_Builder::build(paramBuilder), nullptr, outHandle);
		if (ret != 0) {
			LogError("启动能力失败，错误码: %d", ret);
			delete paramBuilder;
			return ret;
		}
		LogInfo("能力启动成功，句柄: %p", *outHandle);

		// 清理资源
		delete paramBuilder;
		return 0;
	}

	// 停止语音唤醒会话
	int ivw_stop_session(AIKIT_HANDLE* handle) {
		if (!handle) {
			LogError("无效的会话句柄");
			return -1;
		}

		int ret = AIKIT::AIKIT_End(handle);
		if (ret != 0) {
			LogError("结束处理失败，错误码: %d", ret);
			// Do not return yet, try to log
		}

		LogInfo("AIKIT_End 调用完成，句柄: %p, 返回码: %d", handle, ret);
		return ret; // Return the result of AIKIT_End
	}

	//	从文件导入
	int ivw_file(const char* abilityID, const char* audioFilePath, int threshold)
	{
		int ret = 0;
		AIKIT::AIKIT_DataBuilder* dataBuilder = nullptr;
		AIKIT_HANDLE* handle = nullptr;
		AIKIT::AiAudio* aiAudio_raw = nullptr;
		FILE* file = nullptr;
		char data[320] = { 0 };
		long fileSize = 0;
		long readLen = 0;
		// 记录并规范化工作目录
		char currentDir[MAX_PATH];
		GetCurrentDirectoryA(MAX_PATH, currentDir);
		LogInfo("当前工作目录: %s", currentDir);

		// 确保引擎 DLL 已加载
		if (!EnsureEngineDllsLoaded()) {
			LogError("引擎动态库加载失败");
			lastResult = "引擎动态库加载失败";
			return -1;
		}
		LogInfo("引擎动态库加载检查通过");

		// 创建参数构建器
		AIKIT::AIKIT_ParamBuilder* paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
		if (!paramBuilder) {
			LogError("创建参数构建器失败");
			lastResult = "创建参数构建器失败";
			return -1;
		}
		LogInfo("参数构建器创建成功");

		// 设置参数
		std::string thresholdParam = "0 0:" + std::to_string(threshold);
		paramBuilder->param("wdec_param_nCmThreshold", thresholdParam.c_str(), thresholdParam.length());
		paramBuilder->param("gramLoad", true);

		LogInfo("已设置唤醒阈值: %s", thresholdParam.c_str());

		// 重置唤醒标志
		wakeupFlag = 0;
		LogInfo("已重置唤醒标志");

		// 启动能力
		LogInfo("正在启动能力...");
		ret = AIKIT::AIKIT_Start(abilityID, AIKIT::AIKIT_Builder::build(paramBuilder), nullptr, &handle);
		if (ret != 0) {
			LogError("启动能力失败，错误码: %d", ret);
			lastResult = "启动能力失败: " + std::to_string(ret);
			delete paramBuilder;
			return ret;
		}
		LogInfo("能力启动成功，句柄: %p", handle);

		// 打开音频文件
		LogInfo("正在打开音频文件: %s", audioFilePath);
		errno_t err = fopen_s(&file, audioFilePath, "rb");
		if (err != 0 || file == nullptr) {
			LogError("打开音频文件失败: %s，错误码: %d", audioFilePath, err);
			lastResult = "打开音频文件失败: " + std::to_string(err);
			if (paramBuilder) delete paramBuilder;
			if (handle) AIKIT::AIKIT_End(handle);
			return -1;
		}

		// 获取文件大小
		fseek(file, 0, SEEK_END);
		fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);
		LogInfo("音频文件大小: %ld 字节", fileSize);

		// 创建数据构建器
		dataBuilder = AIKIT::AIKIT_DataBuilder::create();
		if (!dataBuilder) {
			LogError("创建数据构建器失败");
			lastResult = "创建数据构建器失败";
			if (file) fclose(file);
			if (paramBuilder) delete paramBuilder;
			if (handle) AIKIT::AIKIT_End(handle);
			return -1;
		}

		// 逐块读取并处理文件数据
		LogInfo("开始处理音频数据...");
		int processCount = 0;
		while (fileSize > 0 && wakeupFlag != 1) {
			readLen = fread(data, 1, sizeof(data), file);
			dataBuilder->clear();

			aiAudio_raw = AIKIT::AiAudio::get("wav")->data(data, (int)readLen)->valid();
			dataBuilder->payload(aiAudio_raw);
			ret = AIKIT::AIKIT_Write(handle, AIKIT::AIKIT_Builder::build(dataBuilder));
			if (ret != 0) {
				LogError("写入数据失败，错误码: %d", ret);
				lastResult = "写入数据失败: " + std::to_string(ret);
				break;
			}
			fileSize -= readLen;
			processCount++;

			if (processCount % 50 == 0) {
				LogInfo("已处理 %d 个音频块...", processCount);
			}
		}

		// 结束处理
		LogInfo("音频处理完成，结束处理...");
		if (handle) {
			ret = AIKIT::AIKIT_End(handle);
			if (ret != 0) {
				LogError("结束处理失败，错误码: %d", ret);
			}
			else {
				LogInfo("结束处理成功");
			}
		}

		// 清理资源
		if (file) fclose(file);
		if (dataBuilder) delete dataBuilder;
		if (paramBuilder) delete paramBuilder;

		if (wakeupFlag == 1) {
			LogInfo("唤醒成功");
			lastResult = "唤醒成功";
			return 0;
		}
		else {
			LogError("唤醒测试失败，未检测到唤醒词，错误码: %d", ret);
			lastResult = "未检测到唤醒词";
			return -1;
		}
	}
}

// 语音唤醒能力初始化，接受资源文件路径作为参数
int Ivw70Init()
{
	// 设置回调函数
	AIKIT_Callbacks cbs = { AIKITDLL::OnOutput, AIKITDLL::OnEvent, AIKITDLL::OnError };

	// 注册能力回调
	int ret = AIKIT::AIKIT_RegisterAbilityCallback(IVW_ABILITY, cbs);
	if (ret != 0) {
		AIKITDLL::LogError("注册能力回调失败，错误码: %d", ret);
		return ret;
	}

	// 初始化阶段 - 检查参数并记录日志
	AIKITDLL::LogInfo("开始语音唤醒引擎初始化...");

	// 确保引擎DLL被正确加载
	AIKITDLL::LogInfo("正在测试引擎加载...");
	if (!AIKITDLL::EnsureEngineDllsLoaded()) {
		AIKITDLL::LogError("引擎动态库加载失败，无法继续初始化");
		AIKITDLL::lastResult = "ERROR: Failed to load engine DLLs.";
		return -1;
	}

	// 初始化引擎
	AIKITDLL::LogInfo("正在初始化唤醒引擎...");

	ret = AIKIT::AIKIT_EngineInit(IVW_ABILITY, nullptr);
	if (ret != 0) {
		AIKITDLL::LogError("引擎初始化失败，错误码: %d, 描述: %s", ret);
		AIKITDLL::lastResult = "ERROR: Engine initialization failed with code: " + std::to_string(ret) +
			". " + ". Resource path: ";
		return ret;
	}
	AIKITDLL::LogInfo("唤醒引擎初始化成功");

	// 准备数据阶段
	AIKIT_CustomData customData;
	customData.key = "key_word";
	customData.index = 0;
	customData.from = AIKIT_DATA_PTR_PATH;
	customData.value = (void*)".\\resource\\ivw70\\xbxb.txt";
	customData.len = strlen(".\\resource\\ivw70\\xbxb.txt");
	customData.next = nullptr;
	customData.reserved = nullptr;

	ret = AIKIT::AIKIT_LoadData(IVW_ABILITY, &customData);
	if (ret != 0) {
		AIKIT::AIKIT_EngineUnInit(IVW_ABILITY);
		return ret;
	}
	AIKITDLL::LogInfo("唤醒资源加载成功");

	AIKITDLL::LogInfo("正在指定唤醒数据集...");
	int index[] = { 0 };
	ret = AIKIT::AIKIT_SpecifyDataSet(IVW_ABILITY, "key_word", index, 1);
	if (ret != 0) {
		AIKITDLL::LogError("指定数据集失败，错误码: %d, 描述: %s", ret);
		AIKITDLL::lastResult = "ERROR: Failed to specify data set with code: " + std::to_string(ret) +
			". " + ". Please check your resource configuration.";
		AIKIT::AIKIT_UnLoadData(IVW_ABILITY, "key_word", 0);
		AIKIT::AIKIT_EngineUnInit(IVW_ABILITY);
		return ret;
	}
	AIKITDLL::LogInfo("数据集指定成功");

	// 初始化完成
	AIKITDLL::LogInfo("语音唤醒初始化完成");
	AIKITDLL::lastResult = "成功：语音唤醒初始化已完成。";
	return 0;
}

// 语音唤醒资源释放
int Ivw70Uninit()
{
	if (!AIKITDLL::isInitialized) {
		AIKITDLL::lastResult = "请先初始化SDK";
		return -1;
	}

	// 卸载资源
	AIKIT::AIKIT_UnLoadData(IVW_ABILITY, "key_word", 0);

	// 反初始化引擎
	AIKIT::AIKIT_EngineUnInit(IVW_ABILITY);

	AIKITDLL::lastResult = "语音唤醒资源已释放";
	return 0;
}

// 全局变量，用于跟踪当前会话句柄和对应的数据构建器
static AIKIT_HANDLE* g_currentHandle = nullptr;
static AIKIT::AIKIT_DataBuilder* g_ivwDataBuilder = nullptr; // Added for AudioManager

// 开始语音唤醒检测
AIKITDLL_API int StartWakeupDetection(int threshold) {
	AIKITDLL::LogInfo("StartWakeupDetection called with threshold: %d", threshold);
	// 检查是否已经有会话在运行
	if (g_currentHandle != nullptr) {
		AIKITDLL::LogError("已有唤醒会话在运行 (g_currentHandle: %p)", g_currentHandle);
		return -1; // Indicate error: already running
	}

	// 初始化SDK (This function should also initialize AudioManager)
	// Assuming InitializeAIKitSDK internally calls AIKIT_Init and AudioManager::GetInstance().Initialize()
	int ret = AIKITDLL::InitializeAIKitSDK();
	if (ret != 0) {
		AIKITDLL::LogError("AIKit SDK 初始化失败，错误码: %d", ret);
		return ret; // Propagate error
	}
	AIKITDLL::LogInfo("AIKit SDK 初始化成功。");

	// 启动唤醒会话
	ret = AIKITDLL::ivw_start_session(IVW_ABILITY, &g_currentHandle, threshold);
	if (ret != 0 || g_currentHandle == nullptr) {
		AIKITDLL::LogError("ivw_start_session 失败，错误码: %d, 句柄: %p", ret, g_currentHandle);
		// Consider calling UninitializeAIKitSDK or parts of it if only session failed but SDK init was ok
		return (ret != 0 ? ret : -1); // Ensure a non-zero error code
	}
	AIKITDLL::LogInfo("ivw_start_session 成功，句柄: %p", g_currentHandle);

	// 创建数据构建器
	if (g_ivwDataBuilder == nullptr) {
		g_ivwDataBuilder = AIKIT::AIKIT_DataBuilder::create();
	}
	if (!g_ivwDataBuilder) {
		AIKITDLL::LogError("创建 g_ivwDataBuilder 失败");
		AIKITDLL::ivw_stop_session(g_currentHandle); // Clean up session
		g_currentHandle = nullptr;
		return -1; // Indicate error
	}
	AIKITDLL::LogInfo("g_ivwDataBuilder 创建成功: %p", g_ivwDataBuilder);

	// 激活 AudioManager 以开始录音并输送给 IVW
	if (!AIKITDLL::AudioManager::GetInstance().ActivateConsumer(AIKITDLL::AudioConsumer::IVW, g_currentHandle, g_ivwDataBuilder)) {
		AIKITDLL::LogError("AudioManager ActivateConsumer 失败 (IVW)");
		delete g_ivwDataBuilder;
		g_ivwDataBuilder = nullptr;
		AIKITDLL::ivw_stop_session(g_currentHandle);
		g_currentHandle = nullptr;
		return -1; // Indicate error
	}
	AIKITDLL::LogInfo("AudioManager ActivateConsumer 成功 (IVW)");

	AIKITDLL::LogInfo("StartWakeupDetection 完成，已激活 AudioManager 进行唤醒。");
	return 0; // Success
}

// 停止语音唤醒检测
AIKITDLL_API int StopWakeupDetection() {
	AIKITDLL::LogInfo("StopWakeupDetection called.");

	if (g_currentHandle == nullptr) {
		AIKITDLL::LogWarning("没有正在运行的唤醒会话 (g_currentHandle is null)");
		// return -1; // Or return 0 if stopping a non-existent session is not an error
		return 0; // Let's consider it not an error to stop if not started.
	}
	AIKITDLL::LogInfo("正在停止唤醒会话，句柄: %p", g_currentHandle);

	// 1. 停用 AudioManager 的 IVW 消费者
	if (!AIKITDLL::AudioManager::GetInstance().DeactivateConsumer(AIKITDLL::AudioConsumer::IVW)) {
		AIKITDLL::LogError("AudioManager DeactivateConsumer 失败 (IVW)");
		// Continue with cleanup as much as possible
	} else {
		AIKITDLL::LogInfo("AudioManager DeactivateConsumer 成功 (IVW)");
	}

	// 2. 停止 AIKIT 会话
	int ret = AIKITDLL::ivw_stop_session(g_currentHandle);
	if (ret != 0) {
		AIKITDLL::LogError("ivw_stop_session 失败，错误码: %d", ret);
		// Continue with cleanup
	} else {
		AIKITDLL::LogInfo("ivw_stop_session 成功。");
	}
	
	// 3. 清理 DataBuilder
	if (g_ivwDataBuilder != nullptr) {
		delete g_ivwDataBuilder;
		g_ivwDataBuilder = nullptr;
		AIKITDLL::LogInfo("g_ivwDataBuilder 已释放。");
	}

	// 4. 重置句柄
	g_currentHandle = nullptr;
	AIKITDLL::LogInfo("g_currentHandle 已重置。");
	
	// Note: AIKITDLL::UninitializeAIKitSDK() should be called globally when app exits, not here.

	AIKITDLL::LogInfo("StopWakeupDetection 完成。");
	return ret; // Return the result of stopping the session, or 0 if it was already stopped.
}