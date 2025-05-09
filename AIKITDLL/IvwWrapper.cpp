#include "pch.h"
#include "IvwWrapper.h"
#include <atomic>
#include <aikit_constant.h>

namespace AIKITDLL {
	std::atomic<int> wakeupFlag(0);

	int ivw_microphone(const char* abilityID, int threshold, int timeoutMs)
	{
		int ret = 0;
		AIKIT::AIKIT_DataBuilder* dataBuilder = nullptr;
		AIKIT_HANDLE* handle = nullptr;
		AIKIT::AiAudio* aiAudio_raw = nullptr;
		DWORD bufsize;

		HWAVEIN hWaveIn = nullptr;  // 输入设备
		WAVEFORMATEX waveform;      // 采集音频的格式，结构体
		BYTE* pBuffer = nullptr;    // 采集音频时的数据缓存
		WAVEHDR wHdr;               // 采集音频时包含数据缓存的结构体
		HANDLE wait = nullptr;

		AIKITDLL::LogInfo("ivw_microphone: 开始麦克风唤醒流程");

		// 设置音频格式
		waveform.wFormatTag = WAVE_FORMAT_PCM;      // 声音格式为PCM
		waveform.nSamplesPerSec = 16000;            // 音频采样率
		waveform.wBitsPerSample = 16;               // 采样比特
		waveform.nChannels = 1;                     // 采样声道数
		waveform.nAvgBytesPerSec = 16000 * 2;       // 每秒的数据率
		waveform.nBlockAlign = 2;                   // 一个块的大小，采样bit的字节数乘以声道数
		waveform.cbSize = 0;                        // 一般为0

		AIKITDLL::LogInfo("ivw_microphone: 设置音频格式完成");

		// 创建事件并打开音频设备
		wait = CreateEvent(NULL, 0, 0, NULL);
		if (waveInOpen(&hWaveIn, WAVE_MAPPER, &waveform, (DWORD_PTR)wait, 0L, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
			AIKITDLL::LogError("ivw_microphone: 打开音频设备失败");
			lastResult = "打开音频设备失败";
			return -1;
		}
		AIKITDLL::LogInfo("ivw_microphone: 打开音频设备成功");

		// 创建参数构建器
		AIKIT::AIKIT_ParamBuilder* paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
		if (!paramBuilder) {
			AIKITDLL::LogError("ivw_microphone: 创建参数构建器失败");
			lastResult = "创建参数构建器失败";
			return -1;
		}
		AIKITDLL::LogInfo("ivw_microphone: 参数构建器创建成功");

		// 设置参数
		std::string thresholdParam = "0 0:" + std::to_string(threshold);
		paramBuilder->param("wdec_param_nCmThreshold", thresholdParam.c_str(), thresholdParam.length());
		paramBuilder->param("gramLoad", true);
		AIKITDLL::LogInfo("ivw_microphone: 已设置唤醒阈值: %s", thresholdParam.c_str());

		// 启动能力
		AIKITDLL::LogInfo("ivw_microphone: 正在启动能力...");
		ret = AIKIT::AIKIT_Start(abilityID, AIKIT::AIKIT_Builder::build(paramBuilder), nullptr, &handle);
		if (ret != 0) {
			AIKITDLL::LogError("ivw_microphone: 启动能力失败，错误码: %d", ret);
			lastResult = "启动能力失败: " + std::to_string(ret);
			delete paramBuilder;
			if (hWaveIn) waveInClose(hWaveIn);
			if (wait) CloseHandle(wait);
			return ret;
		}
		AIKITDLL::LogInfo("ivw_microphone: 能力启动成功，句柄: %p", handle);

		// 分配音频缓冲区
		bufsize = 1024 * 500; // 开辟适当大小的内存存储音频数据
		pBuffer = (BYTE*)malloc(bufsize);
		if (!pBuffer) {
			AIKITDLL::LogError("ivw_microphone: 内存分配失败");
			lastResult = "内存分配失败";
			delete paramBuilder;
			if (handle) AIKIT::AIKIT_End(handle);
			if (hWaveIn) waveInClose(hWaveIn);
			if (wait) CloseHandle(wait);
			return -1;
		}
		AIKITDLL::LogInfo("ivw_microphone: 分配音频缓冲区成功，大小: %lu", bufsize);

		// 设置录音缓冲区
		wHdr.lpData = (LPSTR)pBuffer;
		wHdr.dwBufferLength = bufsize;
		wHdr.dwBytesRecorded = 0;
		wHdr.dwUser = 0;
		wHdr.dwFlags = 0;
		wHdr.dwLoops = 1;
		waveInPrepareHeader(hWaveIn, &wHdr, sizeof(WAVEHDR));
		waveInAddBuffer(hWaveIn, &wHdr, sizeof(WAVEHDR));
		waveInStart(hWaveIn); // 开始录音
		AIKITDLL::LogInfo("ivw_microphone: 开始录音");

		// 重置唤醒标志
		wakeupFlag = 0;
		AIKITDLL::LogInfo("ivw_microphone: 已重置唤醒标志");

		// 创建数据构建器
		dataBuilder = AIKIT::AIKIT_DataBuilder::create();
		if (!dataBuilder) {
			AIKITDLL::LogError("ivw_microphone: 创建数据构建器失败");
			free(pBuffer);
			delete paramBuilder;
			if (handle) AIKIT::AIKIT_End(handle);
			if (hWaveIn) {
				waveInStop(hWaveIn);
				waveInClose(hWaveIn);
			}
			if (wait) CloseHandle(wait);
			return -1;
		}
		AIKITDLL::LogInfo("ivw_microphone: 数据构建器创建成功");

		int len = 0;
		int audio_count = 0;
		int count = 0;
		DWORD startTime = GetTickCount();
		AIKIT_DataStatus status = AIKIT_DataBegin;

		AIKITDLL::LogInfo("ivw_microphone: 进入音频数据处理循环，超时时间: %d ms", timeoutMs);

		// 循环处理音频数据直到唤醒或超时
		while (audio_count < bufsize && wakeupFlag != 1)
		{
			// 检查是否超时
			if (timeoutMs > 0 && (GetTickCount() - startTime) > (DWORD)timeoutMs) {
				AIKITDLL::LogError("ivw_microphone: 等待唤醒超时");
				lastResult = "等待唤醒超时";
				break;
			}

			Sleep(200); // 等待200ms
			len = 10 * FRAME_LEN; // 16k音频，10帧 （时长200ms）

			if (audio_count >= wHdr.dwBytesRecorded) {
				len = 0;
				status = AIKIT_DataEnd;
			}

			dataBuilder->clear();
			aiAudio_raw = AIKIT::AiAudio::get("wav")->data((const char*)&pBuffer[audio_count], len)->valid();
			dataBuilder->payload(aiAudio_raw);
			ret = AIKIT::AIKIT_Write(handle, AIKIT::AIKIT_Builder::build(dataBuilder));
			if (ret != 0) {
				AIKITDLL::LogError("ivw_microphone: 写入数据失败，错误码: %d", ret);
				lastResult = "写入数据失败: " + std::to_string(ret);
				break;
			}

			status = AIKIT_DataContinue;
			audio_count += len;
			count++;

			if (count % 10 == 0) {
				AIKITDLL::LogInfo("ivw_microphone: 已处理 %d 个音频块, 当前累计字节: %d", count, audio_count);
			}
		}

		AIKITDLL::LogInfo("ivw_microphone: 音频处理循环结束，开始清理资源");

		// 清理资源
		if (handle) AIKIT::AIKIT_End(handle);
		if (hWaveIn) {
			waveInStop(hWaveIn);
			waveInReset(hWaveIn);
			waveInClose(hWaveIn);
		}
		if (wait) CloseHandle(wait);
		if (pBuffer) free(pBuffer);
		if (dataBuilder) delete dataBuilder;
		if (paramBuilder) delete paramBuilder;

		if (wakeupFlag == 1) {
			AIKITDLL::LogInfo("ivw_microphone: 唤醒成功");
			lastResult = "唤醒成功";
			return 0;
		}
		else {
			if (ret == 0) lastResult = "未检测到唤醒";
			AIKITDLL::LogError("ivw_microphone: 唤醒失败，未检测到唤醒词，错误码: %d", ret);
			return -1;
		}
	}

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
	// 初始化阶段 - 检查参数并记录日志
	AIKITDLL::LogInfo("开始语音唤醒引擎初始化...");

	//// 确保引擎DLL被正确加载（关键修复步骤）
	AIKITDLL::LogInfo("正在测试引擎加载...");
	if (!AIKITDLL::EnsureEngineDllsLoaded()) {
		AIKITDLL::LogError("引擎动态库加载失败，无法继续初始化");
		AIKITDLL::lastResult = "ERROR: Failed to load engine DLLs.";
		return -1;
	}

	// 初始化引擎
	AIKITDLL::LogInfo("正在初始化唤醒引擎...");

	int ret = 0;
	// 尝试无参数方式初始化引擎
	ret = AIKIT::AIKIT_EngineInit(IVW_ABILITY, nullptr);
	if (ret != 0) {
		const char* errDesc = AIKITDLL::GetErrorDescription(ret);
		AIKITDLL::LogError("引擎初始化失败，错误码: %d, 描述: %s", ret, errDesc);
		AIKITDLL::lastResult = "ERROR: Engine initialization failed with code: " + std::to_string(ret) +
			". " + std::string(errDesc) + ". Resource path: ";
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
		const char* errDesc = AIKITDLL::GetErrorDescription(ret);

		AIKIT::AIKIT_EngineUnInit(IVW_ABILITY);
		return ret;
	}
	AIKITDLL::LogInfo("唤醒资源加载成功");

	AIKITDLL::LogInfo("正在指定唤醒数据集...");
	int index[] = { 0 };
	ret = AIKIT::AIKIT_SpecifyDataSet(IVW_ABILITY, "key_word", index, 1);
	if (ret != 0) {
		const char* errDesc = AIKITDLL::GetErrorDescription(ret);
		AIKITDLL::LogError("指定数据集失败，错误码: %d, 描述: %s", ret, errDesc);
		AIKITDLL::lastResult = "ERROR: Failed to specify data set with code: " + std::to_string(ret) +
			". " + std::string(errDesc) + ". Please check your resource configuration.";
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
	AIKIT::AIKIT_ParamBuilder* paramBuilder = nullptr;
	int ret = 0;

	AIKITDLL::LogInfo("======================= IVW70 麦克风测试开始 ===========================");

	// 注册能力回调
	ret = AIKIT::AIKIT_RegisterAbilityCallback(IVW_ABILITY, cbs);
	if (ret != 0) {
		AIKITDLL::LogError("注册能力回调失败，错误码: %d", ret);
		return;
	}
	AIKITDLL::LogInfo("注册能力回调成功");

	// 创建参数构建器
	paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
	if (!paramBuilder) {
		AIKITDLL::LogError("创建参数构建器失败");
		return;
	}

	// 设置参数
	paramBuilder->param("wdec_param_nCmThreshold", "0 0:900", strlen("0 0:900"));
	paramBuilder->param("gramLoad", true);

	// 使用麦克风进行测试
	AIKITDLL::LogInfo("开始从麦克风测试唤醒功能");

	ret = AIKITDLL::ivw_microphone(IVW_ABILITY, 900, 10000); // 10秒超时

	if (ret == 0) {
		AIKITDLL::LogInfo("麦克风唤醒测试成功，检测到唤醒词");
	}
	else {
		AIKITDLL::LogError("麦克风唤醒测试失败，未检测到唤醒词，错误码: %d", ret);
	}

	// 清理资源
	if (paramBuilder != nullptr) {
		delete paramBuilder;
		paramBuilder = nullptr;
	}

	AIKITDLL::LogInfo("======================= IVW70 麦克风测试结束 ===========================");
	return ret;
}
