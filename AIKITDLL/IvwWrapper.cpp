#include "pch.h"
#include "IvwWrapper.h"
#include "IvwResourceManager.h"
#include "SdkHelper.h"
#include <atomic>
#include <aikit_constant.h>
namespace AIKITDLL {
	std::atomic<int> wakeupFlag(0);

	int ivw_microphone(const char* abilityID, int threshold, int timeoutMs)
	{
		// 使用互斥锁保护，确保同一时刻只有一个线程能运行此函数
		std::lock_guard<std::mutex> lock(g_ivwMutex);
		
		// 检查是否已有活动会话
		if (g_ivwSessionActive.load()) {
			AIKITDLL::LogError("ivw_microphone: 已有一个活动的唤醒会话，无法启动新会话");
			lastResult = "已有唤醒会话运行中";
			return -1;
		}
		
		// 标记会话为活动状态
		g_ivwSessionActive.store(true);

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

		// 确保引擎 DLL 已加载
		if (!AIKITDLL::EnsureEngineDllsLoaded()) {
			AIKITDLL::LogError("ivw_microphone: 引擎动态库加载失败");
			lastResult = "引擎动态库加载失败";
			delete paramBuilder;
			if (hWaveIn) waveInClose(hWaveIn);
			if (wait) CloseHandle(wait);
			return -1;
		}
		AIKITDLL::LogInfo("ivw_microphone: 引擎动态库加载检查通过");

		// 启动能力
		AIKITDLL::LogInfo("ivw_microphone: 正在启动能力...");
		
		// 检查构建结果
		auto builtParam = AIKIT::AIKIT_Builder::build(paramBuilder);
		if (!builtParam) {
			AIKITDLL::LogError("ivw_microphone: 参数构建结果无效");
			lastResult = "参数构建结果无效";
			delete paramBuilder;
			if (hWaveIn) waveInClose(hWaveIn);
			if (wait) CloseHandle(wait);
			return -1;
		}		
		
	// 尝试启动能力
	AIKITDLL::LogInfo("ivw_microphone: 正在启动能力...");
	
	// 检查参数和SDK状态
	if (!abilityID) {
		AIKITDLL::LogError("ivw_microphone: abilityID参数无效");
		lastResult = "abilityID参数无效";
		delete paramBuilder;
		if (hWaveIn) waveInClose(hWaveIn);
		if (wait) CloseHandle(wait);
		return -1;
	}
	
	// 先检查SDK状态
	if (!AIKITDLL::isInitialized) {
		AIKITDLL::LogError("ivw_microphone: SDK尚未初始化，先尝试初始化");
		if (!SafeInitSDK()) {
			AIKITDLL::LogError("ivw_microphone: SDK初始化失败");
			lastResult = "SDK初始化失败";
			delete paramBuilder;
			if (hWaveIn) waveInClose(hWaveIn);
			if (wait) CloseHandle(wait);
			return -1;
		}
	}

	// 尝试启动，如果失败且是由于会话问题，则尝试清理后重新启动
	ret = AIKIT::AIKIT_Start(abilityID, builtParam, nullptr, &handle);
	if (ret == 18310 || ret == 18301) { // 会话已存在或授权状态错误
		AIKITDLL::LogWarning("ivw_microphone: 检测到会话状态错误，尝试清理并重启...");
		// 强制清理所有相关资源
		if (AIKIT::AIKIT_End(handle) == 0) {
			AIKITDLL::LogInfo("ivw_microphone: 成功终止现有会话");
		}
		Ivw70Uninit(); // 完全清理
		Sleep(100);    // 等待资源释放
		Ivw70Init();   // 重新初始化
		
		// 重新尝试启动
		AIKITDLL::LogInfo("ivw_microphone: 重新尝试启动能力...");
		ret = AIKIT::AIKIT_Start(abilityID, builtParam, nullptr, &handle);
	}
	
	if (ret != 0) {
		AIKITDLL::LogError("ivw_microphone: 启动能力失败，错误码: %d", ret);
		lastResult = "启动能力失败: " + std::to_string(ret);
		delete paramBuilder;
		if (hWaveIn) waveInClose(hWaveIn);
		if (wait) CloseHandle(wait);
		return ret;
	}
		
		// 检查handle是否有效再记录日志
		if (handle) {
			AIKITDLL::LogInfo("ivw_microphone: 能力启动成功，句柄: %p", handle);
		} else {
			AIKITDLL::LogWarning("ivw_microphone: 能力启动成功，但句柄为空");
		}

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
		wakeupFlag.store(0);
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
		int count = 0;		DWORD startTime = GetTickCount();
		AIKIT_DataStatus status = AIKIT_DataBegin;
		AIKITDLL::LogInfo("ivw_microphone: 进入音频数据处理循环，超时时间: %d ms", timeoutMs);
		// 循环处理音频数据直到唤醒或超时
		while (audio_count < bufsize && wakeupFlag.load() != 1)
		{
			// 检查是否超时
			if (timeoutMs > 0 && (GetTickCount() - startTime) > (DWORD)timeoutMs) {
				AIKITDLL::LogError("ivw_microphone: 等待唤醒超时");
				lastResult = "等待唤醒超时";
				break;
			}

			// 方法1：使用条件变量等待唤醒事件
			if (WaitForWakeup(100)) {
				AIKITDLL::LogInfo("ivw_microphone: 收到唤醒通知，退出录音循环");
				break;
			}
			
			// 方法2：主动检查唤醒状态
			if (GetWakeupStatus() == 1) {
				AIKITDLL::LogInfo("ivw_microphone: 主动检测到唤醒状态，退出录音循环");
				break;
			}
			
			len = 10 * FRAME_LEN; // 16k音频，10帧 （时长200ms）

			if (audio_count >= wHdr.dwBytesRecorded) {
				len = 0;
				status = AIKIT_DataEnd;
			}			dataBuilder->clear();
			aiAudio_raw = AIKIT::AiAudio::get("wav")->data((const char*)&pBuffer[audio_count], len)->valid();
			dataBuilder->payload(aiAudio_raw);
			
			// 检查handle是否有效
			if (!handle) {
				AIKITDLL::LogError("ivw_microphone: 写入数据失败：句柄无效");
				lastResult = "写入数据失败: 句柄无效";
				break;
			}
			
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
		if (paramBuilder) delete paramBuilder;		// 标记会话为非活动状态，无论成功或失败
		g_ivwSessionActive.store(false);
				// 最后一次检查唤醒状态
		if (wakeupFlag.load() != 1) {
			// 此时可能已经收到唤醒回调，但我们错过了，再次通过GetWakeupStatus主动检查
			if (GetWakeupStatus() == 1) {
				wakeupFlag.store(1);
				AIKITDLL::LogInfo("ivw_microphone: 结束时检测到唤醒状态已设置");
			}
		}
		
		if (wakeupFlag.load() == 1) {
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
		// 使用互斥锁保护，确保同一时刻只有一个线程能运行此函数
		std::lock_guard<std::mutex> lock(g_ivwMutex);
		
		// 检查是否已有活动会话
		if (g_ivwSessionActive.load()) {
			AIKITDLL::LogError("ivw_file: 已有一个活动的唤醒会话，无法启动新会话");
			lastResult = "已有唤醒会话运行中";
			return -1;
		}
		
		// 标记会话为活动状态
		g_ivwSessionActive.store(true);

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
		wakeupFlag.store(0);
		LogInfo("已重置唤醒标志");
		// 启动能力
		LogInfo("正在启动能力...");
		// 检查参数有效性
		if (!abilityID) {
			LogError("abilityID参数无效");
			lastResult = "abilityID参数无效";
			delete paramBuilder;
			return -1;
		}
		
		ret = AIKIT::AIKIT_Start(abilityID, AIKIT::AIKIT_Builder::build(paramBuilder), nullptr, &handle);
		if (ret != 0) {
			LogError("启动能力失败，错误码: %d", ret);
			lastResult = "启动能力失败: " + std::to_string(ret);
			delete paramBuilder;
			return ret;
		}
		
		// 检查handle是否有效再记录日志
		if (handle) {
			LogInfo("能力启动成功，句柄: %p", handle);
		} else {
			LogWarning("能力启动成功，但句柄为空");
		}

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
		int processCount = 0;		while (fileSize > 0 && wakeupFlag.load() != 1) {
			readLen = fread(data, 1, sizeof(data), file);
			dataBuilder->clear();			aiAudio_raw = AIKIT::AiAudio::get("wav")->data(data, (int)readLen)->valid();
			dataBuilder->payload(aiAudio_raw);
			
			// 检查handle是否有效
			if (!handle) {
				LogError("写入数据失败：句柄无效");
				lastResult = "写入数据失败: 句柄无效";
				break;
			}
			
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
		} else {
			LogWarning("结束处理：句柄为空");
		}
		// 清理资源
		if (file) fclose(file);
		if (dataBuilder) delete dataBuilder;
		if (paramBuilder) delete paramBuilder;

		// 标记会话为非活动状态，无论成功或失败
		g_ivwSessionActive.store(false);
		if (wakeupFlag.load() == 1) {
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
	// 初始化IVW互斥资源
	AIKITDLL::InitIvwResources();

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
		AIKITDLL::LogWarning("Ivw70Uninit: SDK未初始化，跳过清理");
		// 即使SDK未初始化，也要重置标志位确保一致性
		AIKITDLL::g_ivwSessionActive.store(false);
		AIKITDLL::lastResult = "SDK未初始化";
		return 0;
	}

	// 尝试获取互斥锁，确保安全释放资源
	std::lock_guard<std::mutex> lock(AIKITDLL::g_ivwMutex);
	AIKITDLL::LogInfo("Ivw70Uninit: 开始释放唤醒资源...");

	// 强制重置所有内部唤醒标志位
	ResetWakeupStatus();
	AIKITDLL::LogInfo("Ivw70Uninit: 已重置唤醒状态标志");

	// 尝试卸载资源，忽略潜在错误
	int ret = AIKIT::AIKIT_UnLoadData(IVW_ABILITY, "key_word", 0);
	if (ret != 0) {
		AIKITDLL::LogWarning("Ivw70Uninit: 卸载资源异常，错误码: %d，继续清理", ret);
	}

	// 反初始化引擎，忽略潜在错误
	ret = AIKIT::AIKIT_EngineUnInit(IVW_ABILITY);
	if (ret != 0) {
		AIKITDLL::LogWarning("Ivw70Uninit: 引擎反初始化异常，错误码: %d", ret);
	}

	// 释放IVW互斥资源
	AIKITDLL::CleanupIvwResources();
	
	// 确保重置会话状态标志位
	AIKITDLL::g_ivwSessionActive.store(false);

	// 强制等待一小段时间，确保资源完全释放
	Sleep(100);

	AIKITDLL::LogInfo("Ivw70Uninit: 唤醒资源释放完成");
	AIKITDLL::lastResult = "语音唤醒资源已释放";
	return 0;
}

int TestIvw70(const AIKIT_Callbacks& cbs)
{
	AIKIT::AIKIT_ParamBuilder* paramBuilder = nullptr;
	int ret = 0;

	AIKITDLL::LogInfo("======================= IVW70 测试开始 ===========================");

	// 检查是否已有活动会话
	if (AIKITDLL::g_ivwSessionActive.load()) {
		AIKITDLL::LogError("TestIvw70: 已有一个活动的唤醒会话，无法启动新会话");
		return -1;
	}

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

int Ivw70Microphone(const AIKIT_Callbacks& cbs)
{
	AIKIT::AIKIT_ParamBuilder* paramBuilder = nullptr;
	int ret = 0;

	AIKITDLL::LogInfo("======================= IVW70 麦克风输入开启 ===========================");

	// 检查是否已有活动会话，并尝试清理相关资源
	if (AIKITDLL::g_ivwSessionActive.load()) {
		AIKITDLL::LogWarning("发现已有活动的唤醒会话，尝试先清理现有会话");
		// 强制重置SDK状态，确保没有残留会话
		Ivw70Uninit();
		// 等待资源释放
		Sleep(100);
		// 重新初始化
		int initRet = Ivw70Init();
		if (initRet != 0) {
			AIKITDLL::LogError("重新初始化失败，错误码: %d", initRet);
			return initRet;
		}
		AIKITDLL::LogInfo("重新初始化成功");
	}

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

	AIKITDLL::LogInfo("======================= IVW70 麦克风输出结束 ===========================");
	return ret;
}