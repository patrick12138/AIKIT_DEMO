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
		esr_start_time_(std::chrono::steady_clock::now()) { // 初始化ESR开始时间为当前时间
		// Initialize wave_format_ (example, adjust as needed)
		wave_format_.wFormatTag = WAVE_FORMAT_PCM;
		wave_format_.nChannels = 1;
		wave_format_.nSamplesPerSec = 16000;
		wave_format_.wBitsPerSample = 16;
		wave_format_.nBlockAlign = (wave_format_.nChannels * wave_format_.wBitsPerSample) / 8;
		wave_format_.nAvgBytesPerSec = wave_format_.nSamplesPerSec * wave_format_.nBlockAlign;
		wave_format_.cbSize = 0;
		LogInfo("AudioManager: Instance created.");
	}

	AudioManager::~AudioManager() {
		Uninitialize(); // Ensure resources are freed
		LogInfo("AudioManager: Instance destroyed.");
	}

	bool AudioManager::Initialize(int devid) {
		if (is_initialized_) {
			LogWarning("AudioManager: Already initialized.");
			return true;
		}

		device_id_ = devid;
		int errcode = create_recorder(&recorder_, AudioManager::AudioCallback, this);
		if (errcode != 0 || !recorder_) {
			recorder_ = nullptr; // Ensure recorder_ is null on failure
			LogError("AudioManager: Failed to create recorder. Error code: %d", errcode);
			return false;
		}

		is_initialized_ = true;
		LogInfo("AudioManager: Initialized successfully with device ID: %d.", devid);
		return true;
	}

	void AudioManager::Uninitialize() {
		if (!is_initialized_) {
			LogWarning("AudioManager: Not initialized, nothing to uninitialize.");
			return;
		}

		ForceStopRecording(); // Stop any active recording and clear consumers

		if (recorder_) {
			destroy_recorder(recorder_);
			recorder_ = nullptr;
		}

		is_initialized_ = false;
		LogInfo("AudioManager: Uninitialized.");
	}

	bool AudioManager::ActivateConsumer(AudioConsumer consumer, AIKIT_HANDLE* consumerHandle, AIKIT::AIKIT_DataBuilder* consumerDataBuilder, const char* audioKey) {
		if (!is_initialized_) {
			LogError("AudioManager: Cannot activate consumer, AudioManager not initialized.");
			return false;
		}
		if (!consumerHandle || !consumerDataBuilder) {
			LogError("AudioManager: Cannot activate consumer, consumerHandle or consumerDataBuilder is NULL.");
			return false;
		}
		if (consumer == AudioConsumer::NONE) {
			LogError("AudioManager: Cannot activate AudioConsumer::NONE.");
			return false;
		}

		LogInfo("AudioManager: Attempting to activate consumer: %d", static_cast<int>(consumer));

		if (current_consumer_ != AudioConsumer::NONE && current_consumer_ != consumer) {
			LogWarning("AudioManager: Another consumer (%d) was active. Deactivating it first. The previous consumer should have called DeactivateConsumer.", static_cast<int>(current_consumer_));
			// This implies the previous consumer did not clean up properly by calling DeactivateConsumer.
			// We will overwrite, but this indicates a potential logic flaw in the calling code.
			// For safety, ensure the old consumer's resources are nulled out here.
			active_handle_ = nullptr;
			active_data_builder_ = nullptr;
			// We don't stop/start physical recording if it's already running and we're just switching logical consumer.
		}
		current_consumer_ = consumer;
		active_handle_ = consumerHandle;
		active_data_builder_ = consumerDataBuilder;
		active_audio_key_ = audioKey; // 保存audioKey
		audio_status_ = AIKIT_DataBegin;

		// 如果是ESR消费者，记录开始时间用于超时检查
		if (consumer == AudioConsumer::ESR) {
			esr_start_time_ = std::chrono::steady_clock::now();
			LogInfo("AudioManager: ESR开始时间已记录，超时时间为%d秒", ESR_TIMEOUT_SECONDS);
		}

		if (!is_recording_) {
			if (!recorder_) {
				LogError("AudioManager: Recorder is NULL, cannot start recording.");
				current_consumer_ = AudioConsumer::NONE;
				active_handle_ = nullptr;
				active_data_builder_ = nullptr;
				return false;
			}
			int open_ret = open_recorder(recorder_, device_id_, &wave_format_);
			if (open_ret != 0) {
				LogError("AudioManager: Failed to open recorder. Error: %d. Device ID: %d", open_ret, device_id_);
				current_consumer_ = AudioConsumer::NONE;
				active_handle_ = nullptr;
				active_data_builder_ = nullptr;
				return false;
			}
			LogInfo("AudioManager: Recorder opened successfully.");

			int start_ret = start_record(recorder_);
			if (start_ret != 0) {
				LogError("AudioManager: Failed to start recording. Error: %d", start_ret);
				close_recorder(recorder_);
				current_consumer_ = AudioConsumer::NONE;
				active_handle_ = nullptr;
				active_data_builder_ = nullptr;
				return false;
			}
			is_recording_ = true;
			LogInfo("AudioManager: Recording started for consumer: %d", static_cast<int>(consumer));
		}
		else {
			LogInfo("AudioManager: Recording was already active. Switched consumer to: %d", static_cast<int>(consumer));
		}

		return true;
	}

	bool AudioManager::DeactivateConsumer(AudioConsumer consumer) {
		if (!is_initialized_) {
			LogWarning("AudioManager: Not initialized. Cannot deactivate consumer.");
			return false;
		}
		LogInfo("AudioManager: Attempting to deactivate consumer: %d. Current active: %d", static_cast<int>(consumer), static_cast<int>(current_consumer_));

		if (current_consumer_ == consumer && consumer != AudioConsumer::NONE) {
			current_consumer_ = AudioConsumer::NONE;
			active_handle_ = nullptr;
			active_data_builder_ = nullptr;
			LogInfo("AudioManager: Consumer %d deactivated.", static_cast<int>(consumer));

			if (is_recording_) {
				if (recorder_) {
					int stop_ret = stop_record(recorder_);
					if (stop_ret != 0) {
						LogError("AudioManager: Failed to stop recording. Error: %d", stop_ret);
					}
					else {
						LogInfo("AudioManager: Recording stopped.");
					}
					close_recorder(recorder_);
					LogInfo("AudioManager: Recorder closed.");
				}
				is_recording_ = false;
			}
			return true;
		}
		else if (consumer == AudioConsumer::NONE) {
			LogWarning("AudioManager: Attempted to deactivate AudioConsumer::NONE.");
			return false;
		}
		else {
			LogWarning("AudioManager: Consumer %d was not the active consumer (%d). No action taken for deactivation.", static_cast<int>(consumer), static_cast<int>(current_consumer_));
			return false;
		}
	}

	bool AudioManager::ForceStopRecording() {
		LogInfo("AudioManager: ForceStopRecording called.");
		if (!is_initialized_ && !is_recording_) {
			LogWarning("AudioManager: Not initialized or not recording. Nothing to stop forcefully.");
			return true;
		}

		current_consumer_ = AudioConsumer::NONE;
		active_handle_ = nullptr;
		active_data_builder_ = nullptr;

		if (is_recording_ && recorder_) {
			int stop_ret = stop_record(recorder_);
			if (stop_ret != 0) {
				LogError("AudioManager: Failed to stop recording during ForceStopRecording. Error: %d", stop_ret);
			}
			else {
				LogInfo("AudioManager: Recording stopped due to ForceStopRecording.");
			}
			close_recorder(recorder_);
			LogInfo("AudioManager: Recorder closed due to ForceStopRecording.");
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
			return;
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

		active_data_builder_->clear();
		AIKIT::AiAudio* aiAudio = AIKIT::AiAudio::get(active_audio_key_)
			->data(data, len)
			->status(audio_status_)
			->valid();

		if (!aiAudio) {
			LogError("AudioManager: Failed to create AiAudio object.");
			return;
		}
		active_data_builder_->payload(aiAudio);

		AIKIT_InputData* input_data = AIKIT::AIKIT_Builder::build(active_data_builder_);
		if (!input_data) {
			LogError("AudioManager: Failed to build AIKIT_InputData.");
			return;
		}
		int ret = AIKIT::AIKIT_Write(active_handle_, input_data);
		if (ret != 0) {
			LogError("AudioManager: AIKIT_Write failed. Error: %d. Consumer: %d", ret, static_cast<int>(current_consumer_));
			return; // 写入失败时直接返回，避免继续读取
		}

		// 根据audioKey判断是否需要read
		// IVW（audioKey为"wav"）不需要read，结果通过OnOutput回调返回
		// ESR需要read来获取识别结果
		if (active_audio_key_ && strcmp(active_audio_key_, "wav") == 0) {
			LogInfo("AudioManager: IVW模式，音频写入完成，等待OnOutput回调");
			// IVW模式：只write，不read，结果通过OnOutput回调返回
		} else {
			LogInfo("AudioManager: ESR模式，进入 AIKIT_Read");
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
			LogInfo("AudioManager: 音频状态从 DataBegin 切换到 DataContinue");
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
			}
		}
	}

} // namespace AIKITDLL
