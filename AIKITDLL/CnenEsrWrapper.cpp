#include "pch.h"
#include "CnenEsrWrapper.h"
#include "EsrHelper.h"
#include "AudioManager.h"
#include "aikit_biz_api.h"
#include <atomic>
#include <mutex>

// ESR能力结果标识
namespace AIKITDLL {
    const char ESR_ABILITY_ID[] = "e75f07b62"; // 定义常量

    std::atomic<int> esrResultFlag(0);
    std::atomic<int> esrStatus(ESR_STATUS_NONE_INTERNAL); 
    std::string lastEsrKeywordResult;            
    std::string lastEsrErrorInfo;                
    std::mutex esrResultMutex;                   

    AIKIT_HANDLE* g_esrHandle = nullptr;
    AIKIT::AIKIT_DataBuilder* g_esrDataBuilder = nullptr;
}

// ESR初始化能力与引擎
int CnenEsrInit()
{
    AIKITDLL::LogInfo("CnenEsrInit: 初始化开始");
    // 确保在开始任何操作前，句柄是空的
    if (AIKITDLL::g_esrHandle != nullptr) {
        AIKITDLL::LogWarning("CnenEsrInit: 发现残留句柄，将尝试释放。");
        AIKITDLL::g_esrHandle = nullptr;
    }

    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    AIKITDLL::LogInfo("CnenEsrInit 当前工作目录: %s", currentDir);

    if (!AIKITDLL::EnsureEngineDllsLoaded()) {
        AIKITDLL::LogError("引擎动态库加载失败");
        return -1;
    }
    AIKITDLL::LogInfo("引擎动态库加载检查通过");

    AIKIT::AIKIT_ParamBuilder* engine_paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
    if (engine_paramBuilder == nullptr) {
        AIKITDLL::LogError("创建ESR引擎ParamBuilder失败");
        return -1;
    }

    engine_paramBuilder->clear();
    engine_paramBuilder->param("decNetType", "fsa", strlen("fsa"));
    engine_paramBuilder->param("punishCoefficient", 0.0);
    engine_paramBuilder->param("wfst_addType", 0); // 0-中文，1-英文

    AIKITDLL::LogInfo("正在初始化ESR引擎...");
    int ret = AIKIT::AIKIT_EngineInit(AIKITDLL::ESR_ABILITY_ID, AIKIT::AIKIT_Builder::build(engine_paramBuilder)); // 使用常量
    if (ret != 0) {
        AIKITDLL::LogError("AIKIT_EngineInit ESR 失败，错误码: %d", ret);
        delete engine_paramBuilder;
        return ret;
    }
    AIKITDLL::LogInfo("ESR引擎初始化成功");
    delete engine_paramBuilder;

    AIKIT::AIKIT_CustomBuilder* customBuilder = AIKIT::AIKIT_CustomBuilder::create();
    if (customBuilder == nullptr) {
        AIKITDLL::LogError("创建ESR CustomBuilder失败");
        return -1;
    }    customBuilder->clear();
    
    // 根据当前架构确定资源路径
    std::string resourcePath;
#ifdef _WIN64
    resourcePath = ".\\resource\\cnenesr\\fsa\\cn_fsa.txt";
#else
    resourcePath = ".\\Win32\\Debug\\resource\\cnenesr\\fsa\\cn_fsa.txt";
#endif
    
    AIKITDLL::LogInfo("尝试加载ESR FSA文件: %s", resourcePath.c_str());
    customBuilder->textPath("FSA", resourcePath.c_str(), 0);

    AIKITDLL::LogInfo("正在加载ESR FSA数据...");
    ret = AIKIT::AIKIT_LoadData(AIKITDLL::ESR_ABILITY_ID, AIKIT::AIKIT_Builder::build(customBuilder)); // 使用常量
    if (ret != 0) {
        AIKITDLL::LogError("AIKIT_LoadData ESR FSA 失败，错误码: %d", ret);
        delete customBuilder;
        return ret;
    }
    AIKITDLL::LogInfo("ESR FSA数据加载成功");
    delete customBuilder;

    return 0;
}

// ESR资源释放
int CnenEsrUninit()
{
    AIKITDLL::LogInfo("CnenEsrUninit: 正在释放ESR资源...");
    int ret_unload = AIKIT::AIKIT_UnLoadData(AIKITDLL::ESR_ABILITY_ID, "FSA", 0); // 使用常量
    if (ret_unload != 0) { // Use AIKIT_ERR_SUCCESS for comparison
        AIKITDLL::LogError("AIKIT_UnLoadData FSA 失败，错误码: %d", ret_unload);
    }

    if (AIKITDLL::g_esrHandle != nullptr) {
        AIKITDLL::LogInfo("CnenEsrUninit: 发现活动句柄，尝试 AIKIT_End(%p)", AIKITDLL::g_esrHandle);
        int ret_end = AIKIT::AIKIT_End(AIKITDLL::g_esrHandle); 
        if (ret_end != 0) {
            AIKITDLL::LogError("CnenEsrUninit: AIKIT_End 失败，错误码: %d", ret_end);
        }
        // delete AIKITDLL::g_esrHandle; // SDK 内存管理责任问题
        AIKITDLL::g_esrHandle = nullptr;
    }

    if (AIKITDLL::g_esrDataBuilder) {
        delete AIKITDLL::g_esrDataBuilder;
        AIKITDLL::g_esrDataBuilder = nullptr;
    }
    AIKITDLL::LogInfo("ESR资源释放完成");
    return 0;
}

// 启动ESR麦克风检测（使用AudioManager）
extern "C" __declspec(dllexport) int StartEsrMicrophoneDetection()
{
    AIKITDLL::LogInfo("======================= WPF调用ESR麦克风检测开始 (AudioManager) ===========================");

    if (AIKITDLL::esrStatus.load() == AIKITDLL::ESR_STATUS_PROCESSING_INTERNAL) { 
        AIKITDLL::LogWarning("已有ESR识别会话在运行，请先停止");
        return -1; 
    }

    if (AIKITDLL::g_esrHandle != nullptr) {
        AIKITDLL::LogWarning("StartEsrMicrophoneDetection: 发现残留的g_esrHandle，先调用StopEsrMicrophoneDetection清理");
        //StopEsrMicrophoneDetection(); 
    }
    
    // 初始化SDK (This function should also initialize AudioManager)
    int ret = AIKITDLL::InitializeAIKitSDK(); // Ensure SDK and AudioManager are initialized
    if (ret != 0) {
        AIKITDLL::LogError("AIKit SDK 初始化失败，错误码: %d", ret);
        return ret;
    }
    AIKITDLL::LogInfo("AIKit SDK 初始化成功。");

    AIKIT::AIKIT_ParamBuilder* paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
    if (!paramBuilder) {
        AIKITDLL::LogError("创建ESR paramBuilder失败");
        return -1;
    }
    paramBuilder->clear();
    paramBuilder->param("languageType", 0);
    paramBuilder->param("vadEndGap", 75); 
    paramBuilder->param("vadOn", true);     
    
    int index[] = { 0 };
    ret = AIKIT::AIKIT_SpecifyDataSet(ESR_ABILITY, "FSA", index, sizeof(index) / sizeof(int));
    if (ret != 0) {
        AIKITDLL::LogError("AIKIT_SpecifyDataSet FSA 失败，错误码: %d", ret);
        delete paramBuilder;
        return ret;
    }
    AIKITDLL::LogInfo("AIKIT_SpecifyDataSet FSA 成功");

    ret = AIKIT::AIKIT_Start(ESR_ABILITY, AIKIT::AIKIT_Builder::build(paramBuilder), nullptr, &AIKITDLL::g_esrHandle);
    delete paramBuilder; 
    if (ret != 0 || AIKITDLL::g_esrHandle == nullptr) { 
        AIKITDLL::LogError("AIKIT_Start ESR失败，错误码: %d, 句柄: %p", ret, AIKITDLL::g_esrHandle);
        if (AIKITDLL::g_esrHandle != nullptr) {
             // AIKIT::AIKIT_End(AIKITDLL::g_esrHandle); // Potentially clean up partially initialized handle
             // delete AIKITDLL::g_esrHandle;
             AIKITDLL::g_esrHandle = nullptr;
        }
        return ret;
    }
    AIKITDLL::LogInfo("AIKIT_Start ESR成功，句柄: %p, abilityID: %s, handleID: %zu", AIKITDLL::g_esrHandle, (AIKITDLL::g_esrHandle->abilityID ? AIKITDLL::g_esrHandle->abilityID : "null"), AIKITDLL::g_esrHandle->handleID);

    if (AIKITDLL::g_esrDataBuilder == nullptr) {
        AIKITDLL::g_esrDataBuilder = AIKIT::AIKIT_DataBuilder::create();
        if (!AIKITDLL::g_esrDataBuilder) {
            AIKITDLL::LogError("创建ESR DataBuilder失败");
            AIKIT::AIKIT_End(AIKITDLL::g_esrHandle); 
            // delete AIKITDLL::g_esrHandle;
            AIKITDLL::g_esrHandle = nullptr;
            return -1;
        }
    }
    AIKITDLL::g_esrDataBuilder->clear();
    AIKITDLL::LogInfo("ESR DataBuilder创建/清空完毕 (%p)", AIKITDLL::g_esrDataBuilder);

    // Assuming ActivateConsumer expects AIKIT_HANDLE* (pointer to the struct)
      // 激活AudioManager的ESR消费者，并指定audioKey为"pcm"
    bool activated = AIKITDLL::AudioManager::GetInstance().ActivateConsumer(
        AIKITDLL::AudioConsumer::ESR,
        AIKITDLL::g_esrHandle,
        AIKITDLL::g_esrDataBuilder,
        "audio" // 指定ESR使用 "pcm" 作为 audioKey
    );
    if (!activated) {
        AIKITDLL::LogError("激活AudioManager ESR消费者失败");
        AIKIT::AIKIT_End(AIKITDLL::g_esrHandle); 
        // delete AIKITDLL::g_esrHandle;
        AIKITDLL::g_esrHandle = nullptr;
        return -1; 
    }
    AIKITDLL::LogInfo("AudioManager ESR消费者激活成功");

    {
        std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
        AIKITDLL::lastEsrKeywordResult.clear();
        AIKITDLL::lastEsrErrorInfo.clear();
        AIKITDLL::esrStatus = AIKITDLL::ESR_STATUS_PROCESSING_INTERNAL; 
    }
    AIKITDLL::esrResultFlag = 0; 

    AIKITDLL::LogInfo("ESR麦克风检测已启动 (AudioManager)");
    return 0;
}

// 停止ESR麦克风检测（使用AudioManager）
extern "C" __declspec(dllexport) int StopEsrMicrophoneDetection()
{
    AIKITDLL::LogInfo("======================= WPF调用停止ESR麦克风检测 (AudioManager) ===========================");

    bool wasProcessing = (AIKITDLL::esrStatus.load() == AIKITDLL::ESR_STATUS_PROCESSING_INTERNAL);

    AIKITDLL::AudioManager::GetInstance().DeactivateConsumer(AIKITDLL::AudioConsumer::ESR); // Use :: for global namespace
    AIKITDLL::LogInfo("AudioManager ESR消费者已尝试停用");

    if (AIKITDLL::g_esrHandle != nullptr) {
        AIKITDLL::LogInfo("准备发送AIKIT_DataEnd信号到ESR句柄: %p", AIKITDLL::g_esrHandle);
        if (AIKITDLL::g_esrDataBuilder) { 
            AIKITDLL::g_esrDataBuilder->clear();
            AIKIT::AiAudio* audioEndObj = AIKIT::AiAudio::get("wav")->status(AIKIT_DataEnd)->data(nullptr, 0)->valid();
            //if (audioEndObj) {
            //    int ret_write = AIKIT::AIKIT_Write(AIKITDLL::g_esrHandle, AIKIT::AIKIT_Builder::build(audioEndObj)); 
            //    if (ret_write != AIKIT_ERR_SUCCESS) {
            //        AIKITDLL::LogError("AIKIT_Write DataEnd for ESR failed: %d", ret_write);
            //    }
            //    // delete audioEndObj; // SDK Doc needed for AiAudio lifetime, and also if AIKIT_Builder::build creates a new object that needs deletion.
            //} else {
            //     AIKITDLL::LogError("创建ESR AudioEnd AiAudio 对象失败");
            //}
        } else {
            AIKITDLL::LogWarning("ESR DataBuilder为空，无法通过DataBuilder发送DataEnd信号的payload. 将直接调用AIKIT_End.");
        }

        int ret_end = AIKIT::AIKIT_End(AIKITDLL::g_esrHandle); 
        if (ret_end != 0) {
            AIKITDLL::LogError("AIKIT_End ESR失败，错误码: %d", ret_end);
        }
        AIKITDLL::LogInfo("AIKIT_End ESR调用完毕，句柄已释放: %p", AIKITDLL::g_esrHandle);
        // delete AIKITDLL::g_esrHandle; // SDK memory management
        AIKITDLL::g_esrHandle = nullptr;
    }
    else {
        AIKITDLL::LogWarning("ESR句柄已为空 (nullptr)，无需执行AIKIT_End");
    }

    if (wasProcessing) {
         std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
         if (AIKITDLL::esrStatus.load() == AIKITDLL::ESR_STATUS_PROCESSING_INTERNAL) { 
            AIKITDLL::esrStatus = AIKITDLL::ESR_STATUS_FAILED_INTERNAL; 
            if (AIKITDLL::lastEsrKeywordResult.empty() && AIKITDLL::lastEsrErrorInfo.empty()) {
                AIKITDLL::lastEsrErrorInfo = "用户主动停止";
            }
         }
    } else {
        AIKITDLL::LogInfo("ESR会话已处于非处理状态 (%d)，不因Stop调用而改变最终结果状态。", AIKITDLL::esrStatus.load());
    }
    AIKITDLL::LogInfo("ESR麦克风检测已停止 (AudioManager)");
    return 0;
}

// 获取ESR识别结果的函数保持不变，它们由OnOutput回调填充
extern "C" __declspec(dllexport) int GetEsrResult(char* buffer, int bufferSize)
{
    std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
    if (buffer == nullptr || bufferSize <= 0) return 0;

    if (AIKITDLL::esrStatus.load() == AIKITDLL::ESR_STATUS_SUCCESS_INTERNAL && !AIKITDLL::lastEsrKeywordResult.empty()) { // 使用内部枚举值
        strncpy_s(buffer, bufferSize, AIKITDLL::lastEsrKeywordResult.c_str(), _TRUNCATE);
        buffer[bufferSize - 1] = '\0'; 
        return static_cast<int>(strlen(buffer));
    }
    else if (AIKITDLL::esrStatus.load() == AIKITDLL::ESR_STATUS_FAILED_INTERNAL && !AIKITDLL::lastEsrErrorInfo.empty()) { // 使用内部枚举值
        strncpy_s(buffer, bufferSize, AIKITDLL::lastEsrErrorInfo.c_str(), _TRUNCATE);
        buffer[bufferSize - 1] = '\0'; 
        return static_cast<int>(strlen(buffer));
    }
    buffer[0] = '\0';
    return 0;
}


extern "C" __declspec(dllexport) int GetEsrStatus()
{
    // 返回内部状态值，C#端可以映射回自己的状态定义
    return AIKITDLL::esrStatus.load();
}