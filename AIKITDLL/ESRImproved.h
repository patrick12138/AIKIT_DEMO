#ifndef ESR_IMPROVED_H
#define ESR_IMPROVED_H

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
    int ESRGetResultImproved(AIKIT_HANDLE* handle, AIKIT::AIKIT_DataBuilder* dataBuilder);

    /// <summary>
    /// 处理ESR识别输出结果
    /// 按照官方文档的结果格式进行解析
    /// </summary>
    /// <param name="output">AIKIT输出数据</param>
    void ProcessESROutput(AIKIT_OutputData* output);

    /// <summary>
    /// 处理VAD（语音端点检测）结果
    /// </summary>
    /// <param name="vadJson">VAD的JSON结果</param>
    void ProcessVADResult(const std::string& vadJson);

    /// <summary>
    /// 处理Readable格式的JSON结果
    /// </summary>
    /// <param name="readableJson">JSON格式的详细识别结果</param>
    void ProcessReadableResult(const std::string& readableJson);

    /// <summary>
    /// 命令词检测回调
    /// 当检测到有效命令词时触发
    /// </summary>
    /// <param name="plainText">简洁的识别文本</param>
    /// <param name="readableJson">详细的JSON结果</param>
    void OnCommandWordDetected(const std::string& plainText, const std::string& readableJson);

} // namespace AIKITDLL

#endif // ESR_IMPROVED_H
