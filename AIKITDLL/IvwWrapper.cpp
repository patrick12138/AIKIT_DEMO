#include "pch.h"
#include "IvwWrapper.h"
#include <atomic>
#include <aikit_constant.h>
#include <thread>
#include "winrec.h"

namespace AIKITDLL {
	std::atomic<int> wakeupFlag(0);
	AIKIT_HANDLE* handle = nullptr;

	// 唤醒音频数据回调
	static void onWakeupAudioData(char* data, unsigned long len, void* user_para) {
		AIKIT::AIKIT_DataBuilder* dataBuilder = (AIKIT::AIKIT_DataBuilder*)user_para;
		if (!dataBuilder) return;
		dataBuilder->clear();
		auto aiAudio_raw = AIKIT::AiAudio::get("wav")->data(data, (int)len)->valid();
		dataBuilder->payload(aiAudio_raw);
		if (handle) {
			AIKIT::AIKIT_Write(handle, AIKIT::AIKIT_Builder::build(dataBuilder));
		}
	}

	//  开启麦克风，开启唤醒会话
	int ivw_microphone(const char* abilityID, int threshold, int timeoutMs)
	{
		int ret = 0;
		struct recorder* rec = nullptr;
		static AIKIT::AIKIT_DataBuilder* dataBuilder = nullptr;

		// 只初始化一次会话和数据构建器
		if (handle == nullptr) {
			dataBuilder = AIKIT::AIKIT_DataBuilder::create();
			if (!dataBuilder) {
				AIKITDLL::LogError("ivw_microphone: 创建数据构建器失败");
				return -1;
			}
			// 启动唤醒会话
			ret = ivw_start_session(abilityID, &handle, threshold);
			if (ret != 0) {
				AIKITDLL::LogError("ivw_microphone: 启动会话失败，错误码: %d", ret);
				delete dataBuilder;
				dataBuilder = nullptr;
				return ret;
			}
			AIKITDLL::LogInfo("ivw_microphone: 唤醒会话已启动");
		}

		// 创建recorder对象，回调中送入唤醒引擎
		ret = create_recorder(&rec, onWakeupAudioData, dataBuilder);
		if (ret != 0 || !rec) {
			AIKITDLL::LogError("ivw_microphone: 创建recorder失败");
			return -1;
		}
		// 设置音频格式
		WAVEFORMATEX waveform;
		waveform.wFormatTag = WAVE_FORMAT_PCM;
		waveform.nSamplesPerSec = 16000;
		waveform.wBitsPerSample = 16;
		waveform.nChannels = 1;
		waveform.nAvgBytesPerSec = 16000 * 2;
		waveform.nBlockAlign = 2;
		waveform.cbSize = 0;
		ret = open_recorder(rec, get_default_input_dev(), &waveform);
		if (ret != 0) {
			AIKITDLL::LogError("ivw_microphone: open_recorder失败");
			destroy_recorder(rec);
			return -1;
		}

		// 启动录音
		ret = start_record(rec);
		if (ret != 0) {
			AIKITDLL::LogError("ivw_microphone: start_record失败");
			close_recorder(rec);
			destroy_recorder(rec);
			return -1;
		}
		AIKITDLL::LogInfo("ivw_microphone: 麦克风已开启，进入音频数据处理循环");

		// 主循环，麦克风一直开启
		DWORD startTime = GetTickCount64();
		int waitMs = 50;
		while (true) {
			// 可选：处理超时逻辑
			if (timeoutMs > 0 && (GetTickCount64() - startTime) > (DWORD)timeoutMs) {
				AIKITDLL::LogInfo("ivw_microphone: 超时退出主循环");
				break;
			}
			Sleep(waitMs);
			// 可根据需要添加退出条件
		}

		// 停止录音
		/*stop_record(rec);
		close_recorder(rec);
		destroy_recorder(rec);*/

		// 注意：不结束会话，不释放dataBuilder，保持会话持续
		AIKITDLL::LogInfo("ivw_microphone: 麦克风主循环已退出");
		return 0;
	}

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
			return ret;
		}

		// 清理退出操作
		AIKIT::AIKIT_UnInit();
		LogInfo("结束会话成功");
		return 0;
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

// 全局变量，用于跟踪当前会话句柄
static AIKIT_HANDLE* g_currentHandle = nullptr;

// 开始语音唤醒检测
AIKITDLL_API int StartWakeupDetection(int threshold) {
	// 检查是否已经有会话在运行
	if (g_currentHandle != nullptr) {
		AIKITDLL::LogError("已有唤醒会话在运行");
		return -1;
	}

	// 初始化SDK
	int ret =  AIKITDLL::InitializeAIKitSDK();
	if (ret != 0) {
		AIKITDLL::LogError("AIKit SDK 初始化失败，错误码: %d", ret);
		return -1;
	}

	// 使用麦克风进行唤醒检测（默认10秒超时）
	std::thread([threshold]() {
		AIKITDLL::ivw_microphone(IVW_ABILITY, threshold, 10000);
		}).detach();

	AIKITDLL::LogInfo("开始单次语音唤醒检测，阈值: %d", threshold);
	return 0;
}

// 停止语音唤醒检测
AIKITDLL_API int StopWakeupDetection() {
	AIKITDLL::LogInfo("停止语音唤醒检测");

	if (g_currentHandle == nullptr) {
		AIKITDLL::LogError("没有正在运行的唤醒会话");
		return -1;
	}

	// 停止会话
	int ret = AIKITDLL::ivw_stop_session(g_currentHandle);
	g_currentHandle = nullptr;

	return ret;
}