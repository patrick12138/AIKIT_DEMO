#include "pch.h"
#include "AudioManager.h"
#include "aikit_biz_builder.h" // For AIKIT_Builder, AiAudio
#include "Common.h"            // For AIKITDLL logging

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
		device_id_(-1) { // Default device ID for winrec
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

	bool AudioManager::ActivateConsumer(AudioConsumer consumer, AIKIT_HANDLE* consumerHandle, AIKIT::AIKIT_DataBuilder* consumerDataBuilder) {
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
		audio_status_ = AIKIT_DataBegin;

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
		if (!active_handle_ || !active_data_builder_) {
			return;
		}

		if (len == 0) {
			return;
		}

		active_data_builder_->clear();
		AIKIT::AiAudio* aiAudio = AIKIT::AiAudio::get("wav")
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
		}

		if (audio_status_ == AIKIT_DataBegin) {
			audio_status_ = AIKIT_DataContinue;
		}
	}
} // namespace AIKITDLL
