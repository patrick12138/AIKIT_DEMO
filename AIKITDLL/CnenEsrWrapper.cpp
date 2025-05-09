#include "pch.h"
#include "CnenEsrWrapper.h"
#include "EsrHelper.h"
#include <atomic>
#include <process.h>
#include <conio.h>
#include <errno.h>

// 定义事件类型常量
enum {
    EVT_START = 0,
    EVT_STOP,
    EVT_QUIT,
    EVT_TOTAL
};

// 事件句柄
static HANDLE events[EVT_TOTAL] = { NULL, NULL, NULL };

// ESR能力结果标识
namespace AIKITDLL {
    std::atomic<int> esrResultFlag(0);
    std::string lastEsrResult;
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
    
    // 检查FSA文件是否存在
    FILE* fsaFile = nullptr;
    errno_t err = fopen_s(&fsaFile, ".\\resource\\cnenesr\\fsa\\cn_fsa.txt", "r");
    if (err != 0 || fsaFile == nullptr) {
        AIKITDLL::LogWarning("没有找到中文FSA文件: .\\resource\\cnenesr\\fsa\\cn_fsa.txt");
        
        // 尝试查找英文FSA文件
        err = fopen_s(&fsaFile, ".\\resource\\cnenesr\\fsa\\en_fsa.txt", "r");
        if (err != 0 || fsaFile == nullptr) {
            AIKITDLL::LogError("无法找到任何FSA文件，请检查资源路径");
            return -1;
        }
        AIKITDLL::LogInfo("找到英文FSA文件");
    } else {
        AIKITDLL::LogInfo("找到中文FSA文件");
    }
    
    if (fsaFile) {
        fclose(fsaFile);
    }
    
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
        AIKITDLL::LogInfo("正在注册ESR能力回调...");
        ret = AIKIT::AIKIT_RegisterAbilityCallback(ESR_ABILITY, cbs);
        if (ret != 0) {
            AIKITDLL::LogError("注册能力回调失败，错误码: %d", ret);
            return;
        }
        AIKITDLL::LogInfo("注册能力回调成功");
        // 初始化ESR
         ret = CnenEsrInit();
        if (ret != 0) {
            AIKITDLL::LogError("ESR初始化失败，错误码: %d", ret);
            goto exit;
        }
        
        // 直接从音频文件读取数据
        const char* audioFilePath = ".\\resource\\cnenesr\\testAudio\\cn_test.pcm";
        AIKITDLL::LogInfo("开始从文件获取音频数据: %s", audioFilePath);
        ret = EsrFromFile(audioFilePath);
        if (ret != 0 && ret != ESR_HAS_RESULT) {
            AIKITDLL::LogError("文件处理失败，错误码: %d", ret);
        }
    }
    catch (const std::exception& e) {
        AIKITDLL::LogError("发生异常: %s", e.what());
    }
    catch (...) {
        AIKITDLL::LogError("发生未知异常");
    }

exit:
    // 释放资源
    CnenEsrUninit();
    
    AIKITDLL::LogInfo("======================= ESR 测试结束 ===========================");
}