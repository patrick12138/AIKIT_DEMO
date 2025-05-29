#include "pch.h"
#include "AudioManager.h"
#include "aikit_biz_builder.h" // For AIKIT_Builder, AiAudio
#include "Common.h"            // For AIKITDLL logging
#include "CnenEsrWrapper.h"    // For ESR variables access
#include "EsrHelper.h"         // For UTF8ToLocalString function
#include <string>
#include <chrono> // For timeout implementation

namespace AIKITDLL {

	AudioManager* AudioManager::instance_ = nullptr;

	AudioManager& AudioManager::GetInstance() {
		if (!instance_) {
			instance_ = new AudioManager();
		}
		return *instance_;
	}	
	
	AudioManager::AudioManager()
		: recorder_(nullptr),
		current_consumer_(AudioConsumer::NONE),
		active_handle_(nullptr),
		active_data_builder_(nullptr),
		audio_status_(AIKIT_DataBegin),
		is_initialized_(false),
		is_recording_(false),
		device_id_(-1),
		esr_start_time_(std::chrono::steady_clock::now()),
		buffer_clear_time_(std::chrono::steady_clock::now()),
		should_ignore_audio_(false) { // 初始化新的音频忽略标志// 初始化ESR开始时间为当前时间
		// Initialize wave_format_ (example, adjust as needed)
		wave_format_.wFormatTag = WAVE_FORMAT_PCM;
		wave_format_.nChannels = 1;
		wave_format_.nSamplesPerSec = 16000;
		wave_format_.wBitsPerSample = 16;
		wave_format_.nBlockAlign = (wave_format_.nChannels * wave_format_.wBitsPerSample) / 8;
		wave_format_.nAvgBytesPerSec = wave_format_.nSamplesPerSec * wave_format_.nBlockAlign;		wave_format_.cbSize = 0;
		LogInfo("AudioManager: 实例已创建。");
	}
	
	AudioManager::~AudioManager() {
		Uninitialize(); // Ensure resources are freed
		LogInfo("AudioManager: 实例已销毁。");
	}
	
	bool AudioManager::Initialize(int devid) {
		if (is_initialized_) {
			LogWarning("AudioManager: 已经初始化。");
			return true;
		}

		device_id_ = devid;
		int errcode = create_recorder(&recorder_, AudioManager::AudioCallback, this);		if (errcode != 0 || !recorder_) {
			recorder_ = nullptr; // Ensure recorder_ is null on failure
			LogError("AudioManager: 创建录音器失败。错误码: %d", errcode);
			return false;
		}
		is_initialized_ = true;
		LogInfo("AudioManager: 初始化成功，设备ID: %d。", devid);
		return true;
	}
	
	void AudioManager::Uninitialize() {
		if (!is_initialized_) {
			LogWarning("AudioManager: 未初始化，无需反初始化。");
			return;
		}

		ForceStopRecording(); // Stop any active recording and clear consumers

		if (recorder_) {
			destroy_recorder(recorder_);
			recorder_ = nullptr;
		}
		is_initialized_ = false;
		LogInfo("AudioManager: 已反初始化。");
	}
	
	bool AudioManager::ActivateConsumer(AudioConsumer consumer, AIKIT_HANDLE* consumerHandle, AIKIT::AIKIT_DataBuilder* consumerDataBuilder, const char* audioKey) {
		if (!is_initialized_) {
			LogError("AudioManager: 无法激活消费者，AudioManager未初始化。");
			return false;
		}
		if (!consumerHandle || !consumerDataBuilder) {
			LogError("AudioManager: 无法激活消费者，consumerHandle或consumerDataBuilder为空。");
			return false;
		}
		if (consumer == AudioConsumer::NONE) {
			LogError("AudioManager: 无法激活AudioConsumer::NONE。");
			return false;
		}

		LogInfo("AudioManager: ===== 开始激活消费者 =====");
		LogInfo("AudioManager: 请求激活消费者类型: %d (%s)", static_cast<int>(consumer), 
			consumer == AudioConsumer::IVW ? "IVW唤醒检测" : "ESR命令词识别");
		LogInfo("AudioManager: 当前活动消费者: %d (%s)", static_cast<int>(current_consumer_),
			current_consumer_ == AudioConsumer::NONE ? "无" : 
			(current_consumer_ == AudioConsumer::IVW ? "IVW唤醒检测" : "ESR命令词识别"));
		LogInfo("AudioManager: 当前录音状态: %s", is_recording_ ? "正在录音" : "未录音");

		if (current_consumer_ != AudioConsumer::NONE && current_consumer_ != consumer) {
			LogWarning("AudioManager: 检测到消费者切换: %d -> %d", static_cast<int>(current_consumer_), static_cast<int>(consumer));
			LogWarning("AudioManager: 之前的消费者未正确调用DeactivateConsumer，强制清理");
			// This implies the previous consumer did not clean up properly by calling DeactivateConsumer.
			// We will overwrite, but this indicates a potential logic flaw in the calling code.
			// For safety, ensure the old consumer's resources are nulled out here.
			active_handle_ = nullptr;
			active_data_builder_ = nullptr;
			// 清理音频缓冲区防止旧数据干扰新会话
			LogInfo("AudioManager: 调用ClearAudioBuffers清理旧数据");
			ClearAudioBuffers();
		}
		
		// 记录旧的消费者类型用于判断
		AudioConsumer oldConsumer = current_consumer_;
		
		LogInfo("AudioManager: 设置新的消费者参数");
		current_consumer_ = consumer;
		active_handle_ = consumerHandle;
		active_data_builder_ = consumerDataBuilder;
		active_audio_key_ = audioKey; // 保存audioKey
		audio_status_ = AIKIT_DataBegin;

		// 如果是消费者切换且正在录音，清理音频缓冲区
		if (is_recording_ && oldConsumer != consumer && oldConsumer != AudioConsumer::NONE) {
			LogInfo("AudioManager: 检测到录音中的消费者切换，调用ClearAudioBuffers");
			ClearAudioBuffers();
		}		// 如果是ESR消费者，记录开始时间用于超时检查
		if (consumer == AudioConsumer::ESR) {
			esr_start_time_ = std::chrono::steady_clock::now();
			should_ignore_audio_ = false; // ESR时不需要静音期
			LogInfo("AudioManager: ESR开始时间已记录，超时时间为%d秒，清除静音期状态", ESR_TIMEOUT_SECONDS);
		} else if (consumer == AudioConsumer::IVW) {
			// **关键修复**：IVW启动时检查静音期是否已过期，如果过期则清除状态
			if (should_ignore_audio_) {
				auto now = std::chrono::steady_clock::now();
				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - buffer_clear_time_).count();
				
				if (elapsed >= AUDIO_IGNORE_DURATION_MS) {
					// 静音期已过期，清除忽略状态
					should_ignore_audio_ = false;
					LogInfo("AudioManager: IVW启动时检测到静音期已过期(%ldms)，清除忽略状态", elapsed);
				} else {
					LogInfo("AudioManager: IVW启动时静音期仍活动，剩余%ldms", AUDIO_IGNORE_DURATION_MS - elapsed);
				}
			} else {
				LogInfo("AudioManager: 切换到IVW模式，无静音期限制");
			}
		}

		if (!is_recording_) {
			LogInfo("AudioManager: 录音未启动，准备启动录音设备");			if (!recorder_) {
				LogError("AudioManager: 录音器为空，无法开始录音。");
				current_consumer_ = AudioConsumer::NONE;
				active_handle_ = nullptr;
				active_data_builder_ = nullptr;
				return false;
			}
			int open_ret = open_recorder(recorder_, device_id_, &wave_format_);
			if (open_ret != 0) {
				LogError("AudioManager: 打开录音器失败。错误: %d。设备ID: %d", open_ret, device_id_);
				current_consumer_ = AudioConsumer::NONE;
				active_handle_ = nullptr;
				active_data_builder_ = nullptr;
				return false;
			}
			LogInfo("AudioManager: 录音器打开成功。");			int start_ret = start_record(recorder_);
			if (start_ret != 0) {
				LogError("AudioManager: 开始录音失败。错误: %d", start_ret);
				close_recorder(recorder_);
				current_consumer_ = AudioConsumer::NONE;
				active_handle_ = nullptr;
				active_data_builder_ = nullptr;
				return false;
			}
			is_recording_ = true;
			LogInfo("AudioManager: 为消费者开始录音: %d", static_cast<int>(consumer));
		}
		else {
			LogInfo("AudioManager: 录音已在进行，直接切换到新消费者: %d", static_cast<int>(consumer));
		}

		LogInfo("AudioManager: ===== 消费者激活完成 =====");
		return true;
	}
	
	bool AudioManager::DeactivateConsumer(AudioConsumer consumer) {
		if (!is_initialized_) {
			LogWarning("AudioManager: 未初始化。无法停用消费者。");
			return false;
		}
		LogInfo("AudioManager: 尝试停用消费者: %d。当前活动消费者: %d", static_cast<int>(consumer), static_cast<int>(current_consumer_));

		if (current_consumer_ == consumer && consumer != AudioConsumer::NONE) {			current_consumer_ = AudioConsumer::NONE;
			active_handle_ = nullptr;
			active_data_builder_ = nullptr;
			LogInfo("AudioManager: 消费者 %d 已停用。", static_cast<int>(consumer));

			if (is_recording_) {				if (recorder_) {
					int stop_ret = stop_record(recorder_);
					if (stop_ret != 0) {
						LogError("AudioManager: 停止录音失败。错误: %d", stop_ret);
					}
					else {
						LogInfo("AudioManager: 录音已停止。");
					}
					close_recorder(recorder_);
					LogInfo("AudioManager: 录音器已关闭。");
				}
				is_recording_ = false;
			}
			return true;
		}		else if (consumer == AudioConsumer::NONE) {
			LogWarning("AudioManager: 尝试停用AudioConsumer::NONE。");
			return false;
		}
		else {
			LogWarning("AudioManager: 消费者 %d 不是活动消费者 (%d)。停用时未采取任何操作。", static_cast<int>(consumer), static_cast<int>(current_consumer_));
			return false;
		}
	}
	
	bool AudioManager::ForceStopRecording() {
		LogInfo("AudioManager: 调用强制停止录音。");
		if (!is_initialized_ && !is_recording_) {
			LogWarning("AudioManager: 未初始化或未录音。无需强制停止。");
			return true;
		}

		current_consumer_ = AudioConsumer::NONE;
		active_handle_ = nullptr;
		active_data_builder_ = nullptr;
		if (is_recording_ && recorder_) {
			int stop_ret = stop_record(recorder_);
			if (stop_ret != 0) {
				LogError("AudioManager: 强制停止录音期间停止录音失败。错误: %d", stop_ret);
			}
			else {
				LogInfo("AudioManager: 由于强制停止录音而停止录音。");
			}
			close_recorder(recorder_);
			LogInfo("AudioManager: 由于强制停止录音而关闭录音器。");
		}
		is_recording_ = false;
		return true;
	}

	bool AudioManager::IsRecording() const {
		return is_recording_;
	}

	void AudioManager::AudioCallback(char* data, unsigned long len, void* userData) {
		if (userData) {
			AudioManager* manager = static_cast<AudioManager*>(userData);
			manager->ProcessAudioData(data, len);
		}
	}	
	
	void AudioManager::ProcessAudioData(char* data, unsigned long len) {
		if (!active_handle_ || !active_data_builder_ || !active_audio_key_) {
			LogWarning("AudioManager: ProcessAudioData - 缺少必要参数，跳过处理");
			return;
		}

		// *** 关键修复：检查是否在静音期内，如果是则忽略音频数据 ***
		if (should_ignore_audio_) {
			auto now = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - buffer_clear_time_).count();
			
			if (elapsed < AUDIO_IGNORE_DURATION_MS) {
				// 仍在静音期内，丢弃音频数据
				static int ignoreCounter = 0;
				ignoreCounter++;
				if (ignoreCounter % 50 == 1) {  // 每50次打印一次，避免日志过多
					LogInfo("AudioManager: [静音期] 丢弃音频数据 - 已忽略%ldms，剩余%ldms", 
						elapsed, AUDIO_IGNORE_DURATION_MS - elapsed);
				}
				return; // 直接返回，不处理这批音频数据
			} else {
				// 静音期结束
				should_ignore_audio_ = false;
				LogInfo("AudioManager: *** 静音期结束，恢复正常音频处理 ***");
			}
		}

		// 检查ESR超时
		CheckTimeout();
		if (len == 0) {
			// 当接收到空数据时，可能意味着音频流结束
			if (audio_status_ != AIKIT_DataEnd) {
				audio_status_ = AIKIT_DataEnd;
				LogInfo("AudioManager: 音频流结束，设置状态为 AIKIT_DataEnd");
			}
			return;
		}

		// 添加详细的音频数据日志用于调试缓冲区问题
		if (current_consumer_ == AudioConsumer::IVW) {
			// 计算音频数据的简单特征来判断是否为静音或重复数据
			int sum = 0;
			int nonZeroCount = 0;
			for (unsigned long i = 0; i < len; i++) {
				if (data[i] != 0) {
					sum += abs(data[i]);
					nonZeroCount++;
				}
			}
			double avgAmplitude = nonZeroCount > 0 ? (double)sum / nonZeroCount : 0.0;
			
			// 每隔几次音频数据才打印一次，避免日志过多
			static int audioDataCounter = 0;
			audioDataCounter++;
			if (audioDataCounter % 10 == 1) {  // 每10次音频数据打印一次
				LogInfo("AudioManager: IVW音频数据#%d - 长度:%lu, 非零样本:%d, 平均幅度:%.2f", 
					audioDataCounter, len, nonZeroCount, avgAmplitude);
			}
			
			// 如果平均幅度过高，可能是缓冲区中的旧数据
			if (avgAmplitude > 50.0) {
				LogWarning("AudioManager: *** 检测到高幅度音频数据(可能是缓冲区残留) - 幅度:%.2f ***", avgAmplitude);
			}
			
			// 检测会话刚开始时的音频特征
			if (audio_status_ == AIKIT_DataBegin) {
				LogInfo("AudioManager: IVW会话开始的首批音频 - 长度:%lu, 非零样本:%d, 平均幅度:%.2f", 
					len, nonZeroCount, avgAmplitude);
			}
		} else if (current_consumer_ == AudioConsumer::ESR) {
			// ESR模式下也添加一些音频特征日志
			static int esrAudioCounter = 0;
			esrAudioCounter++;
			if (esrAudioCounter % 20 == 1) {  // ESR时每20次打印一次
				LogInfo("AudioManager: ESR音频数据#%d - 长度:%lu", esrAudioCounter, len);
			}
		}

		active_data_builder_->clear();
		AIKIT::AiAudio* aiAudio = AIKIT::AiAudio::get(active_audio_key_)
			->data(data, len)
			->status(audio_status_)
			->valid();
		if (!aiAudio) {
			LogError("AudioManager: 创建AiAudio对象失败。");
			return;
		}
		active_data_builder_->payload(aiAudio);
		AIKIT_InputData* input_data = AIKIT::AIKIT_Builder::build(active_data_builder_);
		if (!input_data) {
			LogError("AudioManager: 构建AIKIT_InputData失败。");
			return;
		}		int ret = AIKIT::AIKIT_Write(active_handle_, input_data);
		if (ret != 0) {
			LogError("AudioManager: AIKIT_Write失败。错误: %d。消费者: %d", ret, static_cast<int>(current_consumer_));
			return; // 写入失败时直接返回，避免继续读取
		}

		// 根据audioKey判断是否需要read
		// IVW（audioKey为"wav"）不需要read，结果通过OnOutput回调返回
		// ESR需要read来获取识别结果
		if (active_audio_key_ && strcmp(active_audio_key_, "wav") == 0) {
			// 只在第一次或状态变化时打印
			if (audio_status_ == AIKIT_DataBegin) {
				LogInfo("AudioManager: IVW模式，音频写入完成，等待OnOutput回调");
			}
			// IVW模式：只write，不read，结果通过OnOutput回调返回
		} else {
			if (audio_status_ == AIKIT_DataBegin) {
				LogInfo("AudioManager: ESR模式，进入 AIKIT_Read");
			}
			AIKIT_OutputData* output = nullptr;
			ret = AIKIT::AIKIT_Read(active_handle_, &output);
			if (ret != 0) {
				LogError("AudioManager: AIKIT_Read 失败，错误码: %d", ret);
			}
			else if (output != nullptr) {
				// 处理识别结果
				ProcessRecognitionResult(output);
			}
		}

		// 更新音频状态
		if (audio_status_ == AIKIT_DataBegin) {
			audio_status_ = AIKIT_DataContinue;
			LogInfo("AudioManager: 音频状态从 DataBegin 切换到 DataContinue (消费者: %s)", 
				current_consumer_ == AudioConsumer::IVW ? "IVW" : "ESR");
		}
	}

	// 新增方法：处理识别结果
	void AudioManager::ProcessRecognitionResult(AIKIT_OutputData* output) {
		if (!output) return;

		AIKIT_BaseData* node = output->node;
		bool foundCommand = false;
		std::string recognizedText = "";
		std::string resultType = "";

		while (node != nullptr) {
			if (node->key) {
				resultType = std::string(node->key);
				LogInfo("AudioManager: 结果类型 = %s", node->key);				if (node->value && node->len > 0) {
					std::string valueStr((char*)node->value, node->len);
					
					// 对识别结果进行UTF-8编码转换以防止中文乱码
					std::string processedValueStr = valueStr;
					try {
						// 使用EsrHelper中的UTF8转换函数
						processedValueStr = UTF8ToLocalString(valueStr.c_str());
						if (processedValueStr.empty()) {
							processedValueStr = valueStr; // 转换失败时保持原文
						}
					} catch (...) {
						processedValueStr = valueStr;
					}
					
					LogInfo("AudioManager: 识别结果 = %s", processedValueStr.c_str());

					// 根据结果类型进行不同处理
					if (resultType == "plain") {
						// plain格式：最终完整识别结果
						recognizedText = processedValueStr;  // 使用转换后的结果
						foundCommand = true;
						LogInfo("AudioManager: 检测到完整命令词: %s", processedValueStr.c_str());
					}					else if (resultType == "readable") {
						// readable格式：JSON格式结果，包含置信度等详细信息
						ProcessReadableResult(processedValueStr);  // 使用转换后的结果
					}
					else if (resultType == "vad") {
						// VAD结果：语音端点检测
						ProcessVadResult(processedValueStr);  // 使用转换后的结果
					}
					else if (resultType == "pgs") {
						// 渐进式结果：实时刷屏显示
						lastPgsResult_ = processedValueStr;  // 保存转换后的PGS结果
						LogInfo("AudioManager: 渐进式识别: %s", processedValueStr.c_str());
					}
				}
			}
			node = node->next;
		}

		// 如果检测到有效命令词，可以触发相应回调
		if (foundCommand && !recognizedText.empty()) {
			OnCommandDetected(recognizedText);
		}
	}

	// 新增方法：处理JSON格式的readable结果
	void AudioManager::ProcessReadableResult(const std::string& jsonResult) {
		// 这里可以解析JSON获取更详细的识别信息
		// 包括置信度(sc)、命中的槽名(slot)、拼音(pinyin)等
		LogInfo("AudioManager: JSON结果解析: %s", jsonResult.c_str());

		// TODO: 可以添加JSON解析逻辑，提取置信度等关键信息
		// 例如使用 nlohmann/json 或其他JSON库
	}

	// 新增方法：处理VAD结果
	void AudioManager::ProcessVadResult(const std::string& vadResult) {
		LogInfo("AudioManager: VAD检测结果: %s", vadResult.c_str());
		// TODO: 解析VAD JSON结果，检查status字段
		// 如果status为"SpeechAutoFinish"，表示语音自动结束
		if (vadResult.find("SpeechAutoFinish") != std::string::npos) {
			audio_status_ = AIKIT_DataEnd;
			LogInfo("AudioManager: 检测到语音结束，设置状态为 AIKIT_DataEnd");
		}
	}
	
	// 新增方法：命令词检测回调
	void AudioManager::OnCommandDetected(const std::string& command) {
		LogInfo("AudioManager: 检测到命令词: %s", command.c_str());

		// 保存识别结果供C#查询，确保编码正确
		lastEsrResult_ = command;  // command已经在ProcessRecognitionResult中进行了UTF-8转换

		// 这里可以添加命令词处理逻辑
		// 例如：触发相应的操作、通知上层应用等

		// TODO: 根据具体业务需求实现命令响应逻辑
	}
	
	// 实现超时检查
	void AudioManager::CheckTimeout() {
		if (current_consumer_ == AudioConsumer::ESR) {
			auto now = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - esr_start_time_).count();

			if (elapsed >= ESR_TIMEOUT_SECONDS) {
				LogInfo("AudioManager: ESR超时(%d秒)，自动停止", ESR_TIMEOUT_SECONDS);
				
				// 设置ESR超时状态
				std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
				AIKITDLL::esrStatus = AIKITDLL::ESR_STATUS_FAILED_INTERNAL;
				AIKITDLL::lastEsrErrorInfo = "识别超时";
				
				ForceStopRecording();
			}		}
	}	// 新增方法：清理音频缓冲区，防止旧数据干扰新会话
	
	void AudioManager::ClearAudioBuffers() {
		LogInfo("AudioManager: ========== 开始彻底清理音频缓冲区 ==========");
		LogInfo("AudioManager: 清理原因：防止旧数据干扰新会话");
		LogInfo("AudioManager: 当前录音状态: %s", is_recording_ ? "正在录音" : "未录音");
		LogInfo("AudioManager: 当前消费者: %d", static_cast<int>(current_consumer_));
		
		if (recorder_ && is_recording_) {
			LogInfo("AudioManager: 检测到录音设备活动，开始彻底重置流程");
			
			// 1. 停止录音
			LogInfo("AudioManager: [步骤1/5] 调用stop_record停止录音");
			DWORD start_time = GetTickCount();
			stop_record(recorder_);
			DWORD stop_time = GetTickCount();
			LogInfo("AudioManager: stop_record完成，耗时: %dms", stop_time - start_time);
			
			// 2. 关闭录音设备（这会强制清理驱动程序缓冲区）
			LogInfo("AudioManager: [步骤2/5] 调用close_recorder关闭设备清理驱动缓冲区");
			start_time = GetTickCount();
			close_recorder(recorder_);
			DWORD close_time = GetTickCount();
			LogInfo("AudioManager: close_recorder完成，耗时: %dms", close_time - start_time);
			
			// 3. 等待更长时间确保所有音频缓冲区被完全清理
			LogInfo("AudioManager: [步骤3/5] 等待300ms确保驱动缓冲区完全清理");
			Sleep(300);  // 增加等待时间到300ms
			LogInfo("AudioManager: 缓冲区清理等待完成");
			
			// 4. 重新打开录音设备
			LogInfo("AudioManager: [步骤4/5] 调用open_recorder重新打开录音设备");
			start_time = GetTickCount();
			int open_ret = open_recorder(recorder_, device_id_, &wave_format_);
			DWORD open_time = GetTickCount();
			LogInfo("AudioManager: open_recorder完成，耗时: %dms，返回码: %d", open_time - start_time, open_ret);
			if (open_ret != 0) {
				LogError("AudioManager: 重新打开录音设备失败，错误码: %d", open_ret);
				is_recording_ = false;
				LogInfo("AudioManager: ========== 音频缓冲区清理失败 ==========");
				return;
			}
			
			// 5. 重新启动录音
			LogInfo("AudioManager: [步骤5/5] 调用start_record重新启动录音");
			start_time = GetTickCount();
			int start_ret = start_record(recorder_);
			DWORD restart_time = GetTickCount();
			LogInfo("AudioManager: start_record完成，耗时: %dms，返回码: %d", restart_time - start_time, start_ret);
			if (start_ret != 0) {
				LogError("AudioManager: 重新启动录音失败，错误码: %d", start_ret);
				close_recorder(recorder_);
				is_recording_ = false;
				LogInfo("AudioManager: ========== 音频缓冲区清理失败 ==========");
			} else {
				LogInfo("AudioManager: 录音设备重新启动成功，缓冲区已彻底清理");
			}
		} else {
			LogInfo("AudioManager: 录音设备未活动，跳过物理清理步骤");
		}
				// 重置音频状态
		audio_status_ = AIKIT_DataBegin;
		LogInfo("AudioManager: 音频状态重置为 AIKIT_DataBegin");
		
		// *** 关键修复：设置静音期，忽略接下来的音频数据 ***
		buffer_clear_time_ = std::chrono::steady_clock::now();
		should_ignore_audio_ = true;
		LogInfo("AudioManager: *** 设置%dms静音期，忽略缓冲区清理后的音频数据 ***", AUDIO_IGNORE_DURATION_MS);
		
		LogInfo("AudioManager: ========== 音频缓冲区清理完成 ==========");
	}

} // namespace AIKITDLL
