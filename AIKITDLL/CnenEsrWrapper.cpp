#include "pch.h"
#include "CnenEsrWrapper.h"
#include "EsrHelper.h"
#include <atomic>
#include <process.h>
#include <conio.h>
#include <errno.h>
#include <mutex> // 添加互斥锁

// 定义事件类型常量
enum {
    EVT_START = 0,
    EVT_STOP,
    EVT_QUIT,
    EVT_TOTAL
};

// ESR结果状态定义
enum ESR_STATUS {
    ESR_STATUS_NONE = 0,      // 无结果
    ESR_STATUS_SUCCESS = 1,   // 识别成功
    ESR_STATUS_FAILED = 2,    // 识别失败
    ESR_STATUS_PROCESSING = 3 // 正在处理
};

// 事件句柄
static HANDLE events[EVT_TOTAL] = { NULL, NULL, NULL };

// ESR能力结果标识
namespace AIKITDLL {
    std::atomic<int> esrResultFlag(0);
    std::string lastEsrResult;
    std::atomic<int> esrStatus(ESR_STATUS_NONE); // 添加ESR状态
    std::string lastEsrKeywordResult;            // 识别到的命令词结果
    std::string lastEsrErrorInfo;                // 错误信息
    std::mutex esrResultMutex;                   // 结果保护互斥锁
}

// 显示操作提示
static void show_key_hints(void)
{
    AIKITDLL::LogInfo("----------------------------");
    AIKITDLL::LogInfo("按r键开始说话");
    AIKITDLL::LogInfo("按s键结束说话");
    AIKITDLL::LogInfo("按q键退出");
    AIKITDLL::LogInfo("----------------------------");
}

// 辅助线程：监听键盘输入
static unsigned int __stdcall helper_thread_proc(void* para)
{
    int key;
    int quit = 0;

    do {
        key = _getch();
        switch (key) {
        case 'r':
        case 'R':
            SetEvent(events[EVT_START]);
            break;
        case 's':
        case 'S':
            SetEvent(events[EVT_STOP]);
            break;
        case 'q':
        case 'Q':
            quit = 1;
            SetEvent(events[EVT_QUIT]);
            PostQuitMessage(0);
            break;
        default:
            break;
        }

        if (quit)
            break;
    } while (1);

    return 0;
}

// 启动辅助线程
static HANDLE start_helper_thread()
{
    HANDLE hdl;
    hdl = (HANDLE)_beginthreadex(NULL, 0, helper_thread_proc, NULL, 0, NULL);
    return hdl;
}

// 解析ESR回调结果中的命令词
static void parseEsrKeyword(const char* jsonResult) 
{
    if (!jsonResult || strlen(jsonResult) == 0) {
        return;
    }

    // 简单示例：提取命令词文本，实际应用中建议使用JSON解析库
    std::string result(jsonResult);
    
    // 检查是否包含"w":"后面的内容
    size_t wPos = result.find("\"w\":\"");
    if (wPos != std::string::npos) {
        wPos += 5; // 跳过"w":"
        size_t endPos = result.find("\"", wPos);
        if (endPos != std::string::npos) {
            std::string keyword = result.substr(wPos, endPos - wPos);
            
            // 更新结果
            std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
            AIKITDLL::lastEsrKeywordResult = keyword;
            AIKITDLL::esrStatus = ESR_STATUS_SUCCESS;
            AIKITDLL::LogInfo("提取到命令词: %s", keyword.c_str());
        }
    }
}

// ESR回调结果处理函数
void processEsrCallback(const char* key, const char* value) 
{
    if (!key || !value) {
        return;
    }

    AIKITDLL::LogInfo("ESR回调: key:%s\tvalue:%s", key, value);

    // 处理不同类型的回调
    if (strcmp(key, "readable") == 0) {
        // 处理可读结果
        parseEsrKeyword(value);
    }
    else if (strcmp(key, "plain") == 0) {
        // 直接文本结果
        std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
        AIKITDLL::lastEsrKeywordResult = value;
        AIKITDLL::esrStatus = ESR_STATUS_SUCCESS;
    }
    else if (strcmp(key, "error") == 0) {
        // 错误信息
        std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
        AIKITDLL::lastEsrErrorInfo = value;
        AIKITDLL::esrStatus = ESR_STATUS_FAILED;
    }
}

// 从麦克风获取ESR结果的实现
namespace AIKITDLL {
    int esr_microphone(const char* abilityID)
    {
        AIKITDLL::LogInfo("正在初始化麦克风语音识别...");
        
        int errcode;
        HANDLE helper_thread = NULL;
        DWORD waitres;
        char isquit = 0;
        struct EsrRecognizer esr;

        // 初始化语音识别器
        errcode = EsrInit(&esr, ESR_MIC, 1);
        if (errcode) {
            AIKITDLL::LogError("语音识别器初始化失败，错误码: %d", errcode);
            return errcode;
        }

        // 创建事件句柄
        for (int i = 0; i < EVT_TOTAL; ++i) {
            events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (events[i] == NULL) {
                AIKITDLL::LogError("创建事件失败，错误码: %d", GetLastError());
                EsrUninit(&esr);
                return -1;
            }
        }

        // 启动辅助线程
        helper_thread = start_helper_thread();
        if (helper_thread == NULL) {
            AIKITDLL::LogError("创建辅助线程失败");
            
            // 清理资源
            for (int i = 0; i < EVT_TOTAL; ++i) {
                if (events[i]) CloseHandle(events[i]);
            }
            EsrUninit(&esr);
            return -1;
        }

        // 显示操作提示
        show_key_hints();

        // 主循环处理事件
        while (1) {
            waitres = WaitForMultipleObjects(EVT_TOTAL, events, FALSE, INFINITE);
            switch (waitres) {
            case WAIT_FAILED:
                AIKITDLL::LogError("等待事件失败，错误码: %d", GetLastError());
                isquit = 1;
                break;
            case WAIT_TIMEOUT:
                AIKITDLL::LogWarning("等待超时");
                break;
            case WAIT_OBJECT_0 + EVT_START:
                AIKITDLL::LogInfo("开始监听语音...");
                errcode = EsrStartListening(&esr);
                if (errcode) {
                    AIKITDLL::LogError("开始监听失败，错误码: %d", errcode);
                    isquit = 1;
                }
                break;
            case WAIT_OBJECT_0 + EVT_STOP:
                AIKITDLL::LogInfo("停止监听语音...");
                errcode = EsrStopListening(&esr);
                if (errcode) {
                    AIKITDLL::LogError("停止监听失败，错误码: %d", errcode);
                    isquit = 1;
                }
                break;
            case WAIT_OBJECT_0 + EVT_QUIT:
                AIKITDLL::LogInfo("正在退出...");
                EsrStopListening(&esr);
                isquit = 1;
                break;
            default:
                break;
            }
            
            if (isquit)
                break;
        }

        // 清理资源
        if (helper_thread != NULL) {
            WaitForSingleObject(helper_thread, INFINITE);
            CloseHandle(helper_thread);
        }

        for (int i = 0; i < EVT_TOTAL; ++i) {
            if (events[i])
                CloseHandle(events[i]);
        }

        EsrUninit(&esr);
        AIKITDLL::LogInfo("麦克风语音识别已结束");
        return 0;
    }
}

// ESR初始化函数
int CnenEsrInit()
{
    AIKITDLL::LogInfo("正在初始化ESR能力...");
    
    // 检查工作目录
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    AIKITDLL::LogInfo("当前工作目录: %s", currentDir);
    
    // 初始化引擎
    AIKIT::AIKIT_ParamBuilder* engine_paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
    if (engine_paramBuilder == nullptr) {
        AIKITDLL::LogError("创建ParamBuilder失败");
        return -1;
    }
    
    engine_paramBuilder->clear();
    engine_paramBuilder->param("decNetType", "fsa", strlen("fsa"));
    engine_paramBuilder->param("punishCoefficient", 0.0);
    engine_paramBuilder->param("wfst_addType", 0);        // 0-中文，1-英文
    
    AIKITDLL::LogInfo("正在初始化ESR引擎...");
    int ret = AIKIT::AIKIT_EngineInit(ESR_ABILITY, AIKIT::AIKIT_Builder::build(engine_paramBuilder));
    if (ret != 0) {
        AIKITDLL::LogError("AIKIT_EngineInit 失败，错误码: %d", ret);
        delete engine_paramBuilder;
        return ret;
    }
    AIKITDLL::LogInfo("ESR引擎初始化成功");
    
    // 加载词汇表数据
    AIKIT::AIKIT_CustomBuilder* customBuilder = AIKIT::AIKIT_CustomBuilder::create();
    if (customBuilder == nullptr) {
        AIKITDLL::LogError("创建CustomBuilder失败");
        delete engine_paramBuilder;
        return -1;
    }
    
    customBuilder->clear();
    customBuilder->textPath("FSA", ".\\resource\\cnenesr\\fsa\\cn_fsa.txt", 0);
    
    AIKITDLL::LogInfo("正在加载FSA数据...");
    ret = AIKIT::AIKIT_LoadData(ESR_ABILITY, AIKIT::AIKIT_Builder::build(customBuilder));
    if (ret != 0) {
        AIKITDLL::LogError("AIKIT_LoadData 失败，错误码: %d", ret);
        delete engine_paramBuilder;
        delete customBuilder;
        return ret;
    }
    AIKITDLL::LogInfo("FSA数据加载成功");
    
    // 清理资源
    delete engine_paramBuilder;
    delete customBuilder;
    
    return 0;
}

// ESR资源释放
int CnenEsrUninit()
{
    AIKITDLL::LogInfo("正在释放ESR资源...");
    
    // 卸载FSA数据
    int ret = AIKIT::AIKIT_UnLoadData(ESR_ABILITY, "FSA", 0);
    if (ret != 0) {
        AIKITDLL::LogWarning("卸载FSA数据时出现警告，错误码: %d", ret);
    }
    
    AIKITDLL::LogInfo("ESR资源释放完成");
    return 0;
}

// 从麦克风输入获取ESR结果
int EsrFromMicrophone()
{
    AIKITDLL::LogInfo("开始麦克风语音识别测试");
    int ret = AIKITDLL::esr_microphone(ESR_ABILITY);
    AIKITDLL::LogInfo("麦克风语音识别测试结束");
    return ret;
}

// 从文件输入获取ESR结果
int EsrFromFile(const char* audioFilePath)
{
    AIKITDLL::LogInfo("开始文件语音识别测试: %s", audioFilePath);
    
    if (!audioFilePath) {
        AIKITDLL::LogError("音频文件路径为空");
        return -1;
    }
    
    // 检查文件是否存在
    FILE* audioFile = nullptr;
    errno_t err = fopen_s(&audioFile, audioFilePath, "rb");
    if (err != 0 || audioFile == nullptr) {
        AIKITDLL::LogError("音频文件不存在或无法打开: %s, 错误码: %d", audioFilePath, err);
        return -1;
    }
    fclose(audioFile);
    
    // 处理文件识别
    long readLen = 0;
    int ret = ::EsrFromFile(ESR_ABILITY, audioFilePath, 1, &readLen);
    
    AIKITDLL::LogInfo("文件语音识别测试结束，返回值: %d", ret);
    return ret;
}

// 主测试函数
void TestEsr(const AIKIT_Callbacks& cbs)
{
    AIKITDLL::LogInfo("======================= ESR 测试开始 ===========================");

    try {
        // 注册回调
        int ret = 0;
        
        // 设置状态为处理中
        AIKITDLL::esrStatus = ESR_STATUS_PROCESSING;
        
        AIKITDLL::LogInfo("正在注册ESR能力回调...");
        ret = AIKIT::AIKIT_RegisterAbilityCallback(ESR_ABILITY, cbs);
        if (ret != 0) {
            AIKITDLL::LogError("注册能力回调失败，错误码: %d", ret);
            AIKITDLL::esrStatus = ESR_STATUS_FAILED;
            return;
        }
        AIKITDLL::LogInfo("注册能力回调成功");
        // 初始化ESR
         ret = CnenEsrInit();
        if (ret != 0) {
            AIKITDLL::LogError("ESR初始化失败，错误码: %d", ret);
            AIKITDLL::esrStatus = ESR_STATUS_FAILED;
            goto exit;
        }
        
        // 直接从音频文件读取数据
        const char* audioFilePath = ".\\resource\\cnenesr\\testAudio\\cn_test.pcm";
        AIKITDLL::LogInfo("开始从文件获取音频数据: %s", audioFilePath);
        ret = EsrFromFile(audioFilePath);
        if (ret != 0 && ret != ESR_HAS_RESULT) {
            AIKITDLL::LogError("文件处理失败，错误码: %d", ret);
            AIKITDLL::esrStatus = ESR_STATUS_FAILED;
        }
    }
    catch (const std::exception& e) {
        AIKITDLL::LogError("发生异常: %s", e.what());
        AIKITDLL::esrStatus = ESR_STATUS_FAILED;
    }
    catch (...) {
        AIKITDLL::LogError("发生未知异常");
        AIKITDLL::esrStatus = ESR_STATUS_FAILED;
    }

exit:
    // 释放资源
    CnenEsrUninit();
    
    AIKITDLL::LogInfo("======================= ESR 测试结束 ===========================");
}

// 获取ESR状态
extern "C" __declspec(dllexport) int GetEsrStatus()
{
    return AIKITDLL::esrStatus.load();
}

// 重置ESR状态
extern "C" __declspec(dllexport) void ResetEsrStatus()
{
    std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
    AIKITDLL::esrStatus = ESR_STATUS_NONE;
    AIKITDLL::lastEsrKeywordResult.clear();
    AIKITDLL::lastEsrErrorInfo.clear();
}

// 获取ESR命令词结果
extern "C" __declspec(dllexport) const char* GetEsrKeywordResult()
{
    std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
    // 返回静态缓冲区，确保字符串不会在函数返回后被销毁
    static char resultBuffer[1024] = { 0 };
    memset(resultBuffer, 0, sizeof(resultBuffer));
    strcpy_s(resultBuffer, sizeof(resultBuffer), AIKITDLL::lastEsrKeywordResult.c_str());
    return resultBuffer;
}

// 获取ESR错误信息
extern "C" __declspec(dllexport) const char* GetEsrErrorInfo()
{
    std::lock_guard<std::mutex> lock(AIKITDLL::esrResultMutex);
    static char errorBuffer[1024] = { 0 };
    memset(errorBuffer, 0, sizeof(errorBuffer));
    strcpy_s(errorBuffer, sizeof(errorBuffer), AIKITDLL::lastEsrErrorInfo.c_str());
    return errorBuffer;
}