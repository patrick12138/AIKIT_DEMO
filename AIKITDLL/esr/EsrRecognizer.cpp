#include "EsrRecognizer.h"
#include "../Common.h"
#include "../ResultBufferManager.h"
#include <mutex>
#include <atomic>
#include <windows.h>
#include <string>
#include <cstring>
#include <cstdio>
#include <conio.h>
#include <process.h>
#include <errno.h>

using namespace std;

namespace esr {
// 事件类型常量
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

static HANDLE events[EVT_TOTAL] = { NULL, NULL, NULL };

int esr_microphone(const char* abilityID)
{
    Common::LogInfo("正在初始化麦克风语音识别...");

    int errcode;
    HANDLE helper_thread = NULL;
    DWORD waitres;
    char isquit = 0;
    struct EsrRecognizer esr;
    DWORD startTime = GetTickCount();
    const DWORD MAX_WAIT_TIME = 10000; // 10秒超时

    // 初始化语音识别器
    errcode = EsrInit(&esr, ESR_MIC, -1);
    if (errcode) {
        Common::LogError("语音识别器初始化失败，错误码: %d", errcode);
        return errcode;
    }

    // 创建事件句柄
    for (int i = 0; i < EVT_TOTAL; ++i) {
        events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (events[i] == NULL) {
            Common::LogError("创建事件失败，错误码: %d", GetLastError());
            EsrUninit(&esr);
            return -1;
        }
    }

    Common::LogInfo("开始监听语音...");
    errcode = EsrStartListening(&esr);
    if (errcode) {
        Common::LogError("开始监听失败，错误码: %d", errcode);
        isquit = 1; // 标记退出
    }
    
    char plainResultBuffer[8192]; // 用于接收 plain 结果的缓冲区
    bool hasNewResult = false;

    while (!isquit) {
        // 调用新的 GetPlainResult
        memset(plainResultBuffer, 0, sizeof(plainResultBuffer)); // 清空缓冲区
        hasNewResult = false; // 重置标志
        int resultLen = GetPlainResult(plainResultBuffer, sizeof(plainResultBuffer) -1, &hasNewResult);

        if (hasNewResult && resultLen > 0) {
            // 成功获取到新的 plain 结果
            // 注意：plainResultBuffer 现在包含的是 "plain: actual_result_json" 这样的格式
            // 你可能需要解析掉 "plain: " 前缀
            char* actualJson = strstr(plainResultBuffer, "plain: ");
            if (actualJson) {
                actualJson += strlen("plain: "); // 跳过 "plain: "
            } else {
                actualJson = plainResultBuffer; // 如果没有前缀，则直接使用
            }
            
            Common::LogInfo("获取到 new plain 结果: %s (长度: %d)", actualJson, resultLen);

            std::lock_guard<std::mutex> lock(ResultBufferManager::Instance().GetMutex());
            ResultBufferManager::Instance().SetKeywordResult(actualJson);
            ResultBufferManager::Instance().SetStatus(ESR_STATUS_SUCCESS);
            Common::LogInfo("已识别到命令词: %s，准备退出监听", actualJson);
            
            errcode = EsrStopListening(&esr);
            if (errcode) {
                Common::LogError("停止监听失败，错误码: %d", errcode);
            }
            isquit = 1;
            break; 
        } else {
            // Common::LogDebug("未获取到新的plain结果，或结果为空。hasNewResult: %s, resultLen: %d", hasNewResult ? "true" : "false", resultLen);
        }

        // 检查是否已经失败 (例如由其他逻辑设置)
        if (ResultBufferManager::Instance().GetStatus() == ESR_STATUS_FAILED) {
            Common::LogInfo("命令词识别已失败（由其他部分标记），准备退出监听");
            errcode = EsrStopListening(&esr);
            if (errcode) {
                Common::LogError("停止监听失败，错误码: %d", errcode);
            }
            isquit = 1;
            break;
        }

        // 检查是否超时
        if (GetTickCount() - startTime > MAX_WAIT_TIME) {
            Common::LogInfo("命令词识别超时，准备退出监听");
            errcode = EsrStopListening(&esr);
            if (errcode) {
                Common::LogError("停止监听失败，错误码: %d", errcode);
            }
            std::lock_guard<std::mutex> lock(ResultBufferManager::Instance().GetMutex());
            ResultBufferManager::Instance().SetStatus(ESR_STATUS_FAILED);
            ResultBufferManager::Instance().SetErrorInfo("识别超时");
            isquit = 1;
            break;
        }

        // 等待事件，设置较短的超时时间以便定期检查识别结果
        waitres = WaitForMultipleObjects(EVT_TOTAL, events, FALSE, 200); // 缩短等待时间，更频繁检查
        switch (waitres) {
        case WAIT_FAILED:
            Common::LogError("等待事件失败，错误码: %d", GetLastError());
            isquit = 1;
            break;
        case WAIT_TIMEOUT:
            // 超时意味着没有事件发生，循环将继续，再次尝试获取结果
            break;
        case WAIT_OBJECT_0 + EVT_STOP:
            Common::LogInfo("接收到停止事件，停止监听语音...");
            errcode = EsrStopListening(&esr);
            if (errcode) {
                Common::LogError("停止监听失败，错误码: %d", errcode);
            }
            isquit = 1; // 标记退出
            break;
        case WAIT_OBJECT_0 + EVT_QUIT:
            Common::LogInfo("接收到退出事件，正在退出...");
            EsrStopListening(&esr); // 尝试停止
            isquit = 1;
            break;
        default:
            // 其他事件
            break;
        }
    }

    // 清理资源
    if (helper_thread != NULL) {
        WaitForSingleObject(helper_thread, INFINITE);
        CloseHandle(helper_thread);
    }

    for (int i = 0; i < EVT_TOTAL; ++i) {
        if (events[i]) {
            CloseHandle(events[i]);
            events[i] = NULL; // 防止重复关闭
        }
    }

    EsrUninit(&esr);
    Common::LogInfo("麦克风语音识别已结束");
    return errcode; // 返回最后的错误码
}

void TestEsrMicrophone(const AIKIT_Callbacks& cbs)
{
    Common::LogInfo("======================= ESR 麦克风测试开始 ===========================");

    try {
        // 注册回调
        int ret = 0;

        // 设置状态为处理中
        ResultBufferManager::Instance().SetStatus(ESR_STATUS_PROCESSING);
        Common::LogInfo("正在注册ESR能力回调");

        ret = AIKIT::AIKIT_RegisterAbilityCallback(ESR_ABILITY, cbs);
        if (ret != 0) {
            Common::LogError("注册能力回调失败，错误码: %d", ret);
            ResultBufferManager::Instance().SetStatus(ESR_STATUS_FAILED);
            return;
        }
        Common::LogInfo("注册能力回调成功");

        // 初始化ESR
        ret = CnenEsrInit();
        if (ret != 0) {
            Common::LogError("ESR初始化失败，错误码: %d", ret);
            ResultBufferManager::Instance().SetStatus(ESR_STATUS_FAILED);
            goto exit;
        }
        // 从麦克风获取音频数据
        Common::LogInfo("开始从麦克风获取音频数据");
        ret = esr_microphone(ESR_ABILITY);
        if (ret != 0) {
            Common::LogError("麦克风处理失败，错误码: %d", ret);
            ResultBufferManager::Instance().SetStatus(ESR_STATUS_FAILED);
        }
        else
        {
            if (ResultBufferManager::Instance().GetStatus() != ESR_STATUS_SUCCESS) {
                // 如果状态仍然是处理中，但函数返回了，说明可能是超时或其他退出原因
                if (ResultBufferManager::Instance().GetStatus() == ESR_STATUS_PROCESSING) {
                    Common::LogInfo("麦克风处理已结束，但未找到明确结果");
                    ResultBufferManager::Instance().SetStatus(ESR_STATUS_FAILED);
                    ResultBufferManager::Instance().SetErrorInfo("未找到明确的命令词结果");
                }
            }
            else {
                Common::LogInfo("麦克风处理成功");
            }
        }
    }
    catch (const std::exception& e) {
        Common::LogError("发生异常: %s", e.what());
        ResultBufferManager::Instance().SetStatus(ESR_STATUS_FAILED);
    }
    catch (...) {
        Common::LogError("发生未知异常");
        ResultBufferManager::Instance().SetStatus(ESR_STATUS_FAILED);
    }

exit:
    // 释放资源
    CnenEsrUninit();

    Common::LogInfo("======================= ESR 麦克风测试结束 ===========================");
}

} // namespace esr
