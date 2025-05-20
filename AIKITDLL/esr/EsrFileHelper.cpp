#include "EsrFileHelper.h"
#include "../Common.h"
#include "../ResultBufferManager.h"
#include <cstdio>
#include <mutex>
#include <string>

using namespace std;

namespace esr {

// ESR结果状态定义
enum ESR_STATUS {
    ESR_STATUS_NONE = 0,      // 无结果
    ESR_STATUS_SUCCESS = 1,   // 识别成功
    ESR_STATUS_FAILED = 2,    // 识别失败
    ESR_STATUS_PROCESSING = 3 // 正在处理
};

int EsrFromFile(const char* audioFilePath)
{
    Common::LogInfo("开始文件语音识别测试: %s", audioFilePath);

    if (!audioFilePath) {
        Common::LogError("音频文件路径为空");
        return -1;
    }

    // 检查文件是否存在
    FILE* audioFile = nullptr;
    errno_t err = fopen_s(&audioFile, audioFilePath, "rb");
    if (err != 0 || audioFile == nullptr) {
        Common::LogError("音频文件不存在或无法打开: %s, 错误码: %d", audioFilePath, err);
        return -1;
    }
    fclose(audioFile);

    // 处理文件识别
    long readLen = 0;
    int ret = ::EsrFromFile(ESR_ABILITY, audioFilePath, 1, &readLen);

    Common::LogInfo("文件语音识别测试结束，返回值: %d", ret);
    return ret;
}

void TestEsr(const AIKIT_Callbacks& cbs)
{
    Common::LogInfo("======================= ESR 测试开始 ===========================");

    try {
        int ret = 0;

        // 设置状态为处理中
        ResultBufferManager::Instance().SetStatus(ESR_STATUS_PROCESSING);

        Common::LogInfo("正在注册ESR能力回调...");
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

        // 直接从音频文件读取数据
        const char* audioFilePath = ".\\resource\\cnenesr\\testAudio\\cn_test.pcm";
        Common::LogInfo("开始从文件获取音频数据: %s", audioFilePath);
        ret = EsrFromFile(audioFilePath);
        if (ret != 0 /*&& ret != ESR_HAS_RESULT*/) {
            Common::LogError("文件处理失败，错误码: %d", ret);
            ResultBufferManager::Instance().SetStatus(ESR_STATUS_FAILED);
        }
        else
        {
            Common::LogInfo("文件处理成功");
            ResultBufferManager::Instance().SetStatus(ESR_STATUS_SUCCESS);
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

    Common::LogInfo("======================= ESR 测试结束 ===========================");
}

} // namespace esr
