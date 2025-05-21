\
#ifndef AUDIOMANAGER_H
#define AUDIOMANAGER_H

#include <windows.h> // Added for WAVEFORMATEX
#include "winrec.h" // For SRecorder
#include "aikit_biz_api.h" // For AIKIT types (AIKIT_HANDLE, AIKIT_DataBuilder)
#include "aikit_constant.h" // For AIKIT_DataStatus
#include "Common.h"   // For AIKITDLL logging (LogDebug, LogInfo etc.)

// Forward declaration if AIKIT_HANDLE is not fully defined by aikit_biz_api.h alone
// struct AIKIT_HANDLE_TAG;
// typedef struct AIKIT_HANDLE_TAG AIKIT_HANDLE;
// class AIKIT_DataBuilder; // Usually defined in aikit_biz_builder.h or similar

namespace AIKITDLL {

enum class AudioConsumer {
    NONE,
    IVW, // 语音唤醒
    ESR  // 命令词识别
};

class AudioManager {
public:
    static AudioManager& GetInstance();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // 初始化录音设备，应在应用启动时调用一次
    // devid: winrec使用的设备ID
    bool Initialize(int devid = 0); 

    // 激活指定的音频消费者。
    // 如果录音未启动，则启动录音。如果已在录音，则切换到新的消费者。
    // consumer: 要激活的消费者类型 (IVW 或 ESR)
    // consumerHandle: 对应AIKIT会话的句柄
    // consumerDataBuilder: 对应AIKIT会话的数据构造器
    bool ActivateConsumer(AudioConsumer consumer, AIKIT_HANDLE* consumerHandle, AIKIT::AIKIT_DataBuilder* consumerDataBuilder);
    
    // 停用指定的音频消费者。
    // 如果没有其他活动的消费者，将停止物理录音。
    bool DeactivateConsumer(AudioConsumer consumer);

    // 强制停止物理录音，并清除所有活动的消费者。
    // 通常在应用退出或需要立即停止所有音频处理时使用。
    bool ForceStopRecording(); 

    // 清理资源，释放录音设备。应在应用退出前调用。
    void Uninitialize(); 

    bool IsRecording() const;

private:
    AudioManager();
    ~AudioManager();

    // winrec 的静态音频回调函数
    static void AudioCallback(char* data, unsigned long len, void* userData);
    // 处理从 winrec 收到的音频数据
    void ProcessAudioData(char* data, unsigned long len);

    recorder* recorder_;
    AudioConsumer current_consumer_;
    AIKIT_HANDLE* active_handle_; 
    AIKIT::AIKIT_DataBuilder* active_data_builder_;
    AIKIT_DataStatus audio_status_; // 当前发送给AIKIT_Write的音频状态
    
    bool is_initialized_;
    bool is_recording_; // 物理录音是否正在进行
    int device_id_;
    WAVEFORMATEX wave_format_; // 音频格式

    static AudioManager* instance_;
};

} // namespace AIKITDLL

#endif // AUDIOMANAGER_H
