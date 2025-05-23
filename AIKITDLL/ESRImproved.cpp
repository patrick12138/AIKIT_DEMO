#include "pch.h"
#include "Common.h"
#include "aikit_biz_api.h"
#include "aikit_biz_builder.h"
#include <string>

namespace AIKITDLL {

    /// <summary>
    /// 改进版的ESR结果获取函数
    /// 根据官方文档规范，完善结果处理逻辑
    /// </summary>
    /// <param name="handle">AIKIT会话句柄</param>
    /// <param name="dataBuilder">数据构造器</param>
    /// <returns>0=成功，其他=错误码</returns>
    int ESRGetResultImproved(AIKIT_HANDLE* handle, AIKIT::AIKIT_DataBuilder* dataBuilder)
    {
        if (!handle || !dataBuilder) {
            LogError("ESRGetResultImproved: 输入参数为空");
            return -1;
        }

        // 1. 构建输入数据
        AIKIT_InputData* input_data = AIKIT::AIKIT_Builder::build(dataBuilder);
        if (!input_data) {
            LogError("ESRGetResultImproved: 构建输入数据失败");
            return -2;
        }

        // 2. 写入数据到AIKIT引擎
        int ret = AIKIT::AIKIT_Write(handle, input_data);
        if (ret != 0) {
            LogError("ESRGetResultImproved: AIKIT_Write失败，错误码: %d", ret);
            return ret;
        }

        // 3. 读取识别结果
        AIKIT_OutputData* output = nullptr;
        ret = AIKIT::AIKIT_Read(handle, &output);
        if (ret != 0) {
            LogError("ESRGetResultImproved: AIKIT_Read失败，错误码: %d", ret);
            return ret;
        }

        // 4. 处理识别结果
        if (output != nullptr) {
            ProcessESROutput(output);
        }
        else {
            LogWarning("ESRGetResultImproved: 未获取到识别结果");
        }

        return 0;
    }

    /// <summary>
    /// 处理ESR识别输出结果
    /// 按照官方文档的结果格式进行解析
    /// </summary>
    /// <param name="output">AIKIT输出数据</param>
    void ProcessESROutput(AIKIT_OutputData* output)
    {
        if (!output || !output->node) {
            LogWarning("ProcessESROutput: 输出数据为空");
            return;
        }

        AIKIT_BaseData* node = output->node;
        std::string plainResult = "";
        std::string readableResult = "";
        bool hasValidResult = false;

        LogInfo("ESR识别结果解析开始...");

        while (node != nullptr) {
            if (node->key) {
                std::string keyStr = std::string(node->key);
                
                if (node->value && node->len > 0) {
                    std::string valueStr((char*)node->value, node->len);
                    
                    // 根据结果类型进行处理
                    if (keyStr == "plain") {
                        // plain: 最终完整识别结果
                        plainResult = valueStr;
                        hasValidResult = true;
                        LogInfo("ESR [PLAIN] 最终结果: %s", valueStr.c_str());
                    }
                    else if (keyStr == "pgs") {
                        // pgs: 渐进式结果（实时刷屏）
                        LogInfo("ESR [PGS] 渐进式结果: %s", valueStr.c_str());
                    }
                    else if (keyStr == "htk") {
                        // htk: 带分词信息的结果
                        LogInfo("ESR [HTK] 分词结果: %s", valueStr.c_str());
                    }
                    else if (keyStr == "vad") {
                        // vad: 语音端点检测结果
                        LogInfo("ESR [VAD] 端点检测: %s", valueStr.c_str());
                        ProcessVADResult(valueStr);
                    }
                    else if (keyStr == "readable") {
                        // readable: JSON格式结果，包含详细信息
                        readableResult = valueStr;
                        LogInfo("ESR [READABLE] JSON结果: %s", valueStr.c_str());
                        ProcessReadableResult(valueStr);
                    }
                    else {
                        // 其他未知类型
                        LogInfo("ESR [%s]: %s", keyStr.c_str(), valueStr.c_str());
                    }
                }
                else {
                    LogWarning("ESR结果类型 [%s] 无数据", keyStr.c_str());
                }
            }
            node = node->next;
        }

        // 检查是否获得有效的命令词识别结果
        if (hasValidResult && !plainResult.empty()) {
            OnCommandWordDetected(plainResult, readableResult);
        }
        else {
            LogInfo("ESR: 本次识别未获得有效命令词结果");
        }

        LogInfo("ESR识别结果解析完成");
    }

    /// <summary>
    /// 处理VAD（语音端点检测）结果
    /// </summary>
    /// <param name="vadJson">VAD的JSON结果</param>
    void ProcessVADResult(const std::string& vadJson)
    {
        LogInfo("VAD结果处理: %s", vadJson.c_str());
        
        // 检查是否包含语音结束标志
        if (vadJson.find("SpeechAutoFinish") != std::string::npos) {
            LogInfo("VAD: 检测到语音自动结束");
            // 在这里可以触发音频状态切换到AIKIT_DataEnd
        }
        else if (vadJson.find("SpeechNormal") != std::string::npos) {
            LogInfo("VAD: 检测到正常语音段");
        }
        
        // 可以进一步解析bg（前端点）和ed（后端点）信息
        // 例如使用JSON解析库提取具体的时间戳
    }

    /// <summary>
    /// 处理Readable格式的JSON结果
    /// </summary>
    /// <param name="readableJson">JSON格式的详细识别结果</param>
    void ProcessReadableResult(const std::string& readableJson)
    {
        LogInfo("Readable结果解析: %s", readableJson.c_str());
        
        // 这里可以解析JSON获取更详细信息：
        // - sc: 置信度
        // - mode: 识别模式（如"fsa"表示命令词模式）
        // - ismandarin: 是否为普通话
        // - ws: 分词结果数组
        //   - w: 识别的词
        //   - slot: 命中的槽名
        //   - pinyin: 拼音
        //   - boundary: 边界信息
        
        // TODO: 可以使用nlohmann/json或其他JSON库进行详细解析
        // 提取置信度、槽名等关键信息用于业务逻辑判断
    }

    /// <summary>
    /// 命令词检测回调
    /// 当检测到有效命令词时触发
    /// </summary>
    /// <param name="plainText">简洁的识别文本</param>
    /// <param name="readableJson">详细的JSON结果</param>
    void OnCommandWordDetected(const std::string& plainText, const std::string& readableJson)
    {
        LogInfo("检测到命令词: [%s]", plainText.c_str());
        
        // 在这里添加具体的命令词处理逻辑
        // 例如：
        // 1. 根据命令词内容执行相应操作
        // 2. 通知上层应用（WPF界面）
        // 3. 触发语音合成回复
        // 4. 记录命令词历史等
        
        // 示例：简单的命令词匹配
        if (plainText.find("播放音乐") != std::string::npos) {
            LogInfo("执行命令: 播放音乐");
            // TODO: 调用音乐播放功能
        }
        else if (plainText.find("关闭应用") != std::string::npos) {
            LogInfo("执行命令: 关闭应用");
            // TODO: 触发应用退出流程
        }
        else {
            LogInfo("未识别的命令词，忽略处理");
        }
    }

} // namespace AIKITDLL
