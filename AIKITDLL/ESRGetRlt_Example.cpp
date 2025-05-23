// ESRGetRlt_Example.cpp
// 示例：离线命令词识别结果获取

#include "pch.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>         // For std::this_thread::sleep_for
#include <chrono>         // For std::chrono::milliseconds

// 假设的头文件，提供必要的声明
#include "ESRImproved.h"  // 包含 ESRGetResultImproved, OnCommandWordDetected 等
// #include "AikitMain.h"    // 假设包含 InitializeAIKitSDK, UninitializeAIKitSDK
// #include "CnenEsrWrapper.h" // 假设包含 CommandWordEngineInit, CommandWordEngineUninit, StartCommandWordSession, StopCommandWordSession
// #include "Common.h"       // 假设包含 LogInfo, LogError, LogWarning

// --- 模拟来自项目其他部分的声明 ---
// 这些通常在各自的头文件中声明，并由链接器解析。
// 为了使此示例概念上完整，我们在此处列出一些关键的外部依赖项。

namespace AIKITDLL {
    // 能力ID常量
    const char* const COMMAND_WORD_ABILITY_ID = "e75f07b62"; // 来自讯飞离线命令词SDK文档

    // 假设这些函数在 AikitMain.cpp 或类似位置定义
    // InitializeAIKitSDK 应该配置 appID, apiKey, apiSecret, workDir, 
    // 并在 auth() 中指定 .ability(COMMAND_WORD_ABILITY_ID) 来调用 AIKIT_Init()
    bool InitializeAIKitSDK(); 
    void UninitializeAIKitSDK();
    void LogInfo(const char* format, ...);
    void LogError(const char* format, ...);
    void LogWarning(const char* format, ...); // OnCommandWordDetected 使用了 LogWarning

    // 假设这些函数在 CnenEsrWrapper.cpp 或类似位置定义 (负责命令词引擎特定逻辑)
    // CommandWordEngineInit 负责 AIKIT_EngineInit, AIKIT_LoadData (如FSA语法), AIKIT_SpecifyDataSet, AIKIT_RegisterAbilityCallback
    int CommandWordEngineInit(); 
    int CommandWordEngineUninit();

    // 启动命令词识别会话的辅助函数 (封装 AIKIT_Start)
    // 返回 AIKIT_HANDLE*，失败则返回 nullptr
    // 参数可根据需要调整，例如音频格式、场景等
    AIKIT_HANDLE* StartCommandWordSession(const char* audioFormat = "wav", const char* scene = "fsa") {
        AIKIT::AIKIT_ParamBuilder* paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
        if (!paramBuilder) {
            LogError("StartCommandWordSession: 创建参数构建器失败");
            return nullptr;
        }
        LogInfo("StartCommandWordSession: 参数构建器创建成功");

        // 根据需要设置参数，例如：
        // paramBuilder->param(AIKIT_PARAM_KEY_AUDIO_TYPE, audioFormat); // "wav", "pcm" 等
        // paramBuilder->param(AIKIT_PARAM_KEY_SAMPLE_RATE, "16000");
        // paramBuilder->param(AIKIT_PARAM_KEY_SCENE, scene); // "fsa" for command word with FSA grammar
        // paramBuilder->param("PARAM_KEY_VAD_ENABLE", "true"); // 是否启用VAD
        // paramBuilder->param("PARAM_KEY_VAD_EOS", "3000");   // VAD后端点超时

        // 此处仅为示例，参数请根据实际FSA语法及需求配置
        paramBuilder->param("aue", "raw"); // 假设输入原始PCM
        paramBuilder->param("sample_rate", "16000");
        paramBuilder->param("scene", scene);


        AIKIT_InputData* startParams = AIKIT::AIKIT_Builder::build(paramBuilder);
        // AIKIT_Builder::build 通常不会获取 paramBuilder 的所有权，需要手动删除
        // 但某些SDK实现可能不同，请参考具体文档。此处假设需要手动删除。
        // delete paramBuilder; //讯飞示例通常在AIKIT_Start后删除

        AIKIT_HANDLE* handle = nullptr;
        LogInfo("StartCommandWordSession: 正在启动能力 (能力ID: %s)...", COMMAND_WORD_ABILITY_ID);
        int ret = AIKIT::AIKIT_Start(COMMAND_WORD_ABILITY_ID, startParams, nullptr, &handle);
        
        if (paramBuilder) delete paramBuilder; // 在AIKIT_Start之后删除

        if (ret != 0) {
            LogError("StartCommandWordSession: AIKIT_Start 失败，错误码: %d", ret);
            // AIKIT_Builder::build 返回的 startParams 是否需要删除？ 通常不需要，由SDK内部管理或handle结束时清理
            return nullptr;
        }
        LogInfo("StartCommandWordSession: 能力启动成功，句柄: %p", handle);
        return handle;
    }

    // 停止命令词识别会话 (封装 AIKIT_End)
    int StopCommandWordSession(AIKIT_HANDLE* handle) {
        if (!handle) {
            LogError("StopCommandWordSession: 无效的会话句柄");
            return -1;
        }
        LogInfo("StopCommandWordSession: 正在结束会话，句柄: %p", handle);
        int ret = AIKIT::AIKIT_End(handle);
        if (ret != 0) {
            LogError("StopCommandWordSession: AIKIT_End 失败，错误码: %d", ret);
        } else {
            LogInfo("StopCommandWordSession: AIKIT_End 调用完成。");
        }
        return ret;
    }

} // namespace AIKITDLL

// --- 示例主逻辑 ---
int RunCommandWordRecognitionExample(const char* audioFilePath) {
    std::cout << "--- 离线命令词识别示例 ---" << std::endl;
    AIKITDLL::LogInfo("离线命令词识别示例开始，音频文件: %s", audioFilePath);

    // 1. 初始化 AIKIT SDK (全局，应配置命令词能力ID进行授权)
    if (!AIKITDLL::InitializeAIKitSDK()) {
        std::cerr << "错误: AIKIT SDK 初始化失败。" << std::endl;
        AIKITDLL::LogError("AIKIT SDK 初始化失败。");
        return -1;
    }
    std::cout << "AIKIT SDK 初始化成功。" << std::endl;
    AIKITDLL::LogInfo("AIKIT SDK 初始化成功。");

    // 2. 初始化命令词识别引擎 (加载FSA等资源)
    if (AIKITDLL::CommandWordEngineInit() != 0) {
        std::cerr << "错误: 命令词识别引擎初始化失败。" << std::endl;
        AIKITDLL::LogError("命令词识别引擎初始化失败。");
        AIKITDLL::UninitializeAIKitSDK();
        return -1;
    }
    std::cout << "命令词识别引擎初始化成功。" << std::endl;
    AIKITDLL::LogInfo("命令词识别引擎初始化成功。");

    // 3. 启动命令词识别会话
    // 假设音频文件是16k PCM，所以参数中 aue="raw", sample_rate="16000"
    // scene="fsa" 表示使用FSA语法进行命令词识别
    AIKIT_HANDLE* esrHandle = AIKITDLL::StartCommandWordSession("raw", "fsa");
    if (!esrHandle) {
        std::cerr << "错误: 启动命令词识别会话失败。" << std::endl;
        AIKITDLL::LogError("启动命令词识别会话失败。");
        AIKITDLL::CommandWordEngineUninit();
        AIKITDLL::UninitializeAIKitSDK();
        return -1;
    }
    std::cout << "命令词识别会话已启动。" << std::endl;
    AIKITDLL::LogInfo("命令词识别会话已启动，句柄: %p", esrHandle);

    // 4. 打开并处理音频文件
    std::ifstream audioFile(audioFilePath, std::ios::binary);
    if (!audioFile.is_open()) {
        std::cerr << "错误: 无法打开音频文件: " << audioFilePath << std::endl;
        AIKITDLL::LogError("无法打开音频文件: %s", audioFilePath);
        AIKITDLL::StopCommandWordSession(esrHandle);
        AIKITDLL::CommandWordEngineUninit();
        AIKITDLL::UninitializeAIKitSDK();
        return -1;
    }
    std::cout << "正在处理音频文件: " << audioFilePath << std::endl;

    char audioBuffer[3200]; // 假设每次读取3200字节 (例如16kHz, 16bit, 100ms的PCM数据量: 16000*2*0.1 = 3200)
    AIKIT::AIKIT_DataBuilder* dataBuilder = nullptr;
    int loopCount = 0;

    while (audioFile.read(audioBuffer, sizeof(audioBuffer)) || audioFile.gcount() > 0) {
        loopCount++;
        std::streamsize bytesRead = audioFile.gcount();
        if (bytesRead == 0) break;

        AIKITDLL::LogInfo("读取到 %d 字节音频数据 (第 %d 次)", (int)bytesRead, loopCount);

        dataBuilder = AIKIT::AIKIT_DataBuilder::create();
        if (!dataBuilder) {
            AIKITDLL::LogError("创建 DataBuilder 失败 (循环中)");
            break; 
        }
        // 注意：假设音频文件是纯PCM数据。如果文件包含头部（如WAV头），需要跳过头部。
        // audioKey 通常是 "audio"，或者根据 AIKIT_InputData 的格式要求。
        // SDK文档或示例通常会指明正确的 audioKey，对于通用音频一般是 "audio"
        // 但您提供的IvwWrapper.cpp中用的是"wav", 这里我们尝试用 "audio" 或 "wav"
        // 讯飞很多示例用 "audio" 作为 key。
        // AiAudio::get("wav") 表示数据是wav格式或者裸的pcm，如果是裸pcm，参数要对应
        // 如果是裸PCM，aue="raw" (已在StartCommandWordSession设置)，status应为DataContinue/DataEnd
        AIKIT::AiAudio* aiAudio = AIKIT::AiAudio::get("wav") // 或者 "pcm" 如果是纯PCM并且aue="raw"
                                    ->data(audioBuffer, (int)bytesRead)
                                    ->status(AIKIT_DataContinue) 
                                    ->valid();
        dataBuilder->payload("audio", aiAudio); // 使用 "audio" 作为 key

        AIKITDLL::LogInfo("调用 ESRGetResultImproved (DataContinue)");
        int esrRet = AIKITDLL::ESRGetResultImproved(esrHandle, dataBuilder);
        if (esrRet != 0) {
            AIKITDLL::LogError("ESRGetResultImproved 返回错误: %d", esrRet);
        }
        delete dataBuilder; // ESRGetResultImproved 内部不删除传入的 dataBuilder
        dataBuilder = nullptr;

        // 模拟真实场景的延迟，避免CPU占用过高
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 100ms, 与3200字节对应
    }
    audioFile.close();
    
    // 发送结束标记
    AIKITDLL::LogInfo("发送音频结束标记 (DataEnd)");
    dataBuilder = AIKIT::AIKIT_DataBuilder::create();
    if (!dataBuilder) {
        AIKITDLL::LogError("创建 DataBuilder 失败 (结束标记)");
    } else {
        AIKIT::AiAudio* audioEnd = AIKIT::AiAudio::get("wav")->status(AIKIT_DataEnd)->valid(); // 无数据，仅状态
        dataBuilder->payload("audio", audioEnd);

        AIKITDLL::LogInfo("调用 ESRGetResultImproved (DataEnd)");
        int esrRet = AIKITDLL::ESRGetResultImproved(esrHandle, dataBuilder);
        if (esrRet != 0) {
            AIKITDLL::LogError("ESRGetResultImproved (DataEnd) 返回错误: %d", esrRet);
        }
        delete dataBuilder;
        dataBuilder = nullptr;
    }
    
    std::cout << "音频文件处理完毕。请检查日志获取识别结果。" << std::endl;
    AIKITDLL::LogInfo("音频文件处理完毕。OnCommandWordDetected 函数中应包含具体命令处理逻辑。");

    // 5. 结束命令词识别会话
    AIKITDLL::StopCommandWordSession(esrHandle);

    // 6. 反初始化命令词识别引擎
    if (AIKITDLL::CommandWordEngineUninit() != 0) {
        std::cerr << "警告: 命令词识别引擎反初始化失败。" << std::endl;
        AIKITDLL::LogWarning("命令词识别引擎反初始化失败。");
    } else {
        std::cout << "命令词识别引擎反初始化成功。" << std::endl;
        AIKITDLL::LogInfo("命令词识别引擎反初始化成功。");
    }

    // 7. 反初始化 AIKIT SDK
    AIKITDLL::UninitializeAIKitSDK();
    std::cout << "AIKIT SDK 反初始化成功。" << std::endl;
    AIKITDLL::LogInfo("AIKIT SDK 反初始化成功。");

    std::cout << "--- 示例结束 ---" << std::endl;
    return 0;
}

// 如果此文件作为独立的可执行文件编译:
// #define STANDALONE_EXAMPLE_MAIN // 取消注释以启用main
#ifdef STANDALONE_EXAMPLE_MAIN

// 模拟实现依赖的函数和变量 (仅为独立编译测试用)
// 实际项目中这些应由其他模块提供
namespace AIKITDLL {
    // 模拟全局变量
    // extern std::atomic<int> wakeupFlag; // 来自IVW示例，此处不需要
    // extern std::string lastResult;    // 来自IVW示例，此处命令词结果由 OnCommandWordDetected 处理

    // 模拟日志函数
    void LogInfo(const char* format, ...) {
        va_list args;
        va_start(args, format);
        printf("[INFO] ");
        vprintf(format, args);
        printf("\n");
        va_end(args);
    }
    void LogError(const char* format, ...) {
        va_list args;
        va_start(args, format);
        fprintf(stderr, "[ERROR] ");
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
        va_end(args);
    }
    void LogWarning(const char* format, ...) {
        va_list args;
        va_start(args, format);
        printf("[WARN] ");
        vprintf(format, args);
        printf("\n");
        va_end(args);
    }

    // 模拟SDK和引擎初始化/反初始化函数
    bool InitializeAIKitSDK() {
        LogInfo("模拟: InitializeAIKitSDK (应包含 AIKIT_Init 及 appID, apiKey, apiSecret, workDir, .auth().ability(\"%s\") 配置)", COMMAND_WORD_ABILITY_ID);
        // 实际应调用 AIKIT_Init() 等
        return true; 
    }
    void UninitializeAIKitSDK() { 
        LogInfo("模拟: UninitializeAIKitSDK (应包含 AIKIT_UnInit)");
    }
    int CommandWordEngineInit() { 
        LogInfo("模拟: CommandWordEngineInit (应包含 AIKIT_EngineInit, AIKIT_LoadData for FSA, AIKIT_SpecifyDataSet for %s)", COMMAND_WORD_ABILITY_ID);
        // 实际应加载如 resource/cnenesr/fsa/cn_fsa.txt
        return 0; 
    }
    int CommandWordEngineUninit() { 
        LogInfo("模拟: CommandWordEngineUninit (应包含 AIKIT_UnLoadData, AIKIT_EngineUnInit for %s)", COMMAND_WORD_ABILITY_ID);
        return 0; 
    }

    // ESRImproved.cpp 中的函数也需要能链接到，或者将其声明和定义移到此处进行单文件测试
    // 这里假设它们在项目中其他地方定义，并通过 ESRImproved.h 链接
    // 为简单起见，若要独立运行，需要将 ESRImproved.cpp 的内容合并或链接。
    // 以下是 OnCommandWordDetected 的简单模拟，实际应来自 ESRImproved.cpp
    void OnCommandWordDetected(const std::string& plainText, const std::string& readableJson) {
        LogInfo("模拟 OnCommandWordDetected: 命令词: [%s], JSON: %s", plainText.c_str(), readableJson.c_str());
        if (plainText == "打开灯") {
            LogInfo("模拟执行: 打开灯");
        }
    }
     // ProcessESROutput 等其他函数也需要模拟或链接
    void ProcessESROutput(AIKIT_OutputData* output) {
        LogInfo("模拟 ProcessESROutput");
        if (output && output->node && output->node->key && strcmp(output->node->key, "plain") == 0 && output->node->value) {
            std::string plainText((char*)output->node->value, output->node->len);
            OnCommandWordDetected(plainText, "{}"); // 简化 JSON
        } else {
             OnCommandWordDetected("模拟未识别", "{}");
        }
    }
    int ESRGetResultImproved(AIKIT_HANDLE* handle, AIKIT::AIKIT_DataBuilder* dataBuilder) {
        LogInfo("模拟 ESRGetResultImproved");
        // 模拟一次写和一次读
        AIKIT_InputData* input_data = AIKIT::AIKIT_Builder::build(dataBuilder);
        if (!input_data) return -1;
        // AIKIT_Write(handle, input_data);

        // 模拟返回一个结果
        if (input_data->data && input_data->data->key && strcmp(input_data->data->key, "audio") == 0 && input_data->data->value) {
             AIKIT::AiAudio* audio = (AIKIT::AiAudio*) input_data->data->value;
             if (audio->status == AIKIT_DataEnd) {
                LogInfo("模拟 ESRGetResultImproved - 接收到DataEnd");
                // 这里可以模拟一个最终识别结果
                AIKIT_BaseData node;
                node.key = "plain";
                std::string res_str = "打开灯";
                node.value = (void*)res_str.c_str();
                node.len = res_str.length();
                node.type = AIKIT_DATA_TYPE_STRING;
                node.next = nullptr;
                AIKIT_OutputData out_data;
                out_data.node = &node;
                ProcessESROutput(&out_data);
             } else {
                LogInfo("模拟 ESRGetResultImproved - 接收到DataContinue");
             }
        }
        return 0;
    }


} // namespace AIKITDLL

int main(int argc, char* argv[]) {
    const char* audioFile = "./resource/cnenesr/testAudio/test_command.wav"; // 请替换为实际的测试音频路径
    if (argc > 1) {
        audioFile = argv[1];
    }
    std::cout << "将使用音频文件: " << audioFile << std::endl;
    std::cout << "确保该文件是16kHz单声道PCM或正确格式的WAV，并且命令词FSA资源已配置。" << std::endl;
    std::cout << "此main函数为独立测试提供模拟实现，实际项目请移除或调整。" << std::endl;
    
    // 创建一个假的资源文件，如果CommandWordEngineInit会检查的话
    std::ofstream outfile ("./resource/cnenesr/fsa/cn_fsa.txt");
    if(outfile.is_open()){
        outfile << "#FSA_BEGIN#\n";
        outfile << "!grammar commands\n";
        outfile << "!start <commands>\n";
        outfile << "<commands> = 打开灯 | 关闭灯;\n";
        outfile.close();
        AIKITDLL::LogInfo("创建了模拟的 cn_fsa.txt");
    } else {
        AIKITDLL::LogError("无法创建模拟的 cn_fsa.txt");
    }
     std::filesystem::create_directories("./resource/cnenesr/testAudio/"); // Ensure directory exists

    return RunCommandWordRecognitionExample(audioFile);
}

#endif // STANDALONE_EXAMPLE_MAIN

/*
关键依赖项和假设：
1.  头文件:
    *   `ESRImproved.h`: 必须提供 `ESRGetResultImproved` 和 `OnCommandWordDetected` 等函数的声明。其实现应在 `ESRImproved.cpp` 中。
    *   其他如 `AikitMain.h`, `CnenEsrWrapper.h`, `Common.h` 用于提供SDK初始化、引擎初始化、日志等辅助函数。
2.  能力ID: `COMMAND_WORD_ABILITY_ID` ("e75f07b62") 用于所有与命令词识别相关的SDK调用。
3.  初始化函数:
    *   `AIKITDLL::InitializeAIKitSDK()`: 全局初始化，应使用 `COMMAND_WORD_ABILITY_ID` 进行授权。
    *   `AIKITDLL::CommandWordEngineInit()`: 命令词引擎特定初始化，加载FSA语法文件（如 `resource/cnenesr/fsa/cn_fsa.txt`）。
4.  会话函数:
    *   `AIKITDLL::StartCommandWordSession()`: 封装 `AIKIT_Start`，启动一个命令词识别会话。
    *   `AIKITDLL::StopCommandWordSession()`: 封装 `AIKIT_End`。
5.  音频文件: 示例需要一个音频文件路径。音频格式应与 `StartCommandWordSession` 中设置的参数（如 `aue`, `sample_rate`）匹配。
    如果使用 "raw" 音频，请确保文件是纯PCM数据，没有头部。
6.  `ESRImproved.cpp` 中的逻辑:
    *   `ESRGetResultImproved` 正确处理音频数据的写入 (`AIKIT_Write`) 和结果的读取 (`AIKIT_Read`)。
    *   `ProcessESROutput` 正确解析结果，并在识别到命令词时调用 `OnCommandWordDetected`。
    *   `OnCommandWordDetected` 包含用户定义的命令处理逻辑。
7.  资源文件: 确保命令词的FSA语法文件 (例如 `cn_fsa.txt`) 存在于 `AIKITDLL::CommandWordEngineInit()` 期望加载的路径。
    `README.md` 提到路径: `C:\AIKIT_DEMO\AIKIT_WPF_DEMO\bin\x64\Debug\net8.0-windows\resource\cnenesr\fsa\cn_fsa.txt`
    SDK文档指引将 `resource` 文件夹复制到工作目录。
8.  错误处理和日志: 示例中包含了基本的错误检查和日志输出。
*/
