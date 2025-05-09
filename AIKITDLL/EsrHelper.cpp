#include "pch.h"
#include "EsrHelper.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

using namespace AIKIT;

#define ESR_DBGON 0
#if ESR_DBGON == 1
#define esr_dbg AIKITDLL::LogDebug
#else
#define esr_dbg
#endif

#define DEFAULT_FORMAT        \
{                             \
    WAVE_FORMAT_PCM,          \
    1,                        \
    16000,                    \
    32000,                    \
    2,                        \
    16,                       \
    sizeof(WAVEFORMATEX)      \
}

#define ESR_MALLOC malloc
#define ESR_MFREE  free
#define ESR_MEMSET memset

bool is_result = false;
int ESRGetRlt(AIKIT_HANDLE* handle, AIKIT_DataBuilder* dataBuilder)
{
    int ret = 0;
    AIKIT_OutputData* output = nullptr;
    AIKIT_InputData* input_data = AIKIT_Builder::build(dataBuilder);
    ret = AIKIT_Write(handle, input_data);
    if (ret != 0)
    {
        AIKITDLL::LogError("AIKIT_Write 失败，错误码: %d", ret);
        return ret;
    }
    ret = AIKIT_Read(handle, &output);
    if (ret != 0)
    {
        AIKITDLL::LogError("AIKIT_Read 失败，错误码: %d", ret);
        return ret;
    }

    if (output != nullptr)
    {
        FILE* fsaFile = nullptr;
               errno_t err = fopen_s(&fsaFile, "esr_result.txt", "ab");
               if (err != 0 || fsaFile == nullptr)
               {
                   AIKITDLL::LogError("文件打开失败!");
                   return -1;
               }
               AIKIT_BaseData* node = output->node;
               while (node != nullptr && node->value != nullptr)
               {
                   fwrite(node->key, sizeof(char), strlen(node->key), fsaFile);
                   fwrite(": ", sizeof(char), strlen(": "), fsaFile);
                   fwrite(node->value, sizeof(char), node->len, fsaFile);
                   fwrite("\n", sizeof(char), strlen("\n"), fsaFile);
                   AIKITDLL::LogInfo("key:%s\tvalue:%s", node->key, (char*)node->value);
                   // 打印识别结果中的每个字符到日志
                   if (node->value) {
                       char* text = (char*)node->value;
                       for (size_t i = 0; i < node->len && text[i] != '\0'; i++) {
                           AIKITDLL::LogInfo("识别字符: %c", text[i]);
                       }
                   }
                   if (node->status == 2)
                       is_result = true;
                   node = node->next;
               }
               fclose(fsaFile);
    }
    if (is_result)
    {
        is_result = false;
        return ESR_HAS_RESULT;
    }

    return ret;
}

static void end_esr(struct EsrRecognizer* esr)
{
    if (esr->aud_src == ESR_MIC)
        stop_record(esr->recorder);
    
    if (esr->handle) {
        AIKIT_End(esr->handle);
        esr->handle = NULL;
    }
    esr->state = ESR_STATE_INIT;
}

static void esr_cb(char* data, unsigned long len, void* user_para)
{
    int errcode;
    struct EsrRecognizer* esr;

    if (len == 0 || data == NULL)
        return;

    esr = (struct EsrRecognizer*)user_para;

    if (esr == NULL || esr->audio_status >= AIKIT_DataEnd)
        return;
    
    errcode = EsrWriteAudioData(esr, data, len);
    if (errcode) {
        end_esr(esr);
        return;
    }
}

int EsrInit(struct EsrRecognizer* esr, enum EsrAudioSource aud_src, int devid)
{
    int errcode;
    int index[] = { 0 };
    WAVEFORMATEX wavfmt = DEFAULT_FORMAT;

    if (aud_src == ESR_MIC && get_input_dev_num() == 0) {
        return E_SR_NOACTIVEDEVICE;
    }

    if (!esr)
        return E_SR_INVAL;

    ESR_MEMSET(esr, 0, sizeof(struct EsrRecognizer));
    esr->state = ESR_STATE_INIT;
    esr->aud_src = aud_src;
    esr->audio_status = AIKIT_DataBegin;
    esr->ABILITY = ESR_ABILITY;
    esr->dataBuilder = nullptr;
    esr->dataBuilder = AIKIT_DataBuilder::create();
    esr->paramBuilder = AIKIT_ParamBuilder::create();

    esr->paramBuilder->clear();
    esr->paramBuilder->param("languageType", 0);
    esr->paramBuilder->param("vadEndGap", 75);
    esr->paramBuilder->param("vadOn", true);
    esr->paramBuilder->param("beamThreshold", 20);
    esr->paramBuilder->param("hisGramThreshold", 3000);
    esr->paramBuilder->param("postprocOn", true);
    esr->paramBuilder->param("vadResponsetime", 1000);
    esr->paramBuilder->param("vadLinkOn", true);
    esr->paramBuilder->param("vadSpeechEnd", 80);
    
    if (aud_src == ESR_MIC) {
        errcode = create_recorder(&esr->recorder, esr_cb, (void*)esr);
        if (esr->recorder == NULL || errcode != 0) {
            esr_dbg("创建录音设备失败: %d", errcode);
            errcode = E_SR_RECORDFAIL;
            goto fail;
        }
        
        // 音频参数设置
        wavfmt.nSamplesPerSec = 16000;
        wavfmt.nAvgBytesPerSec = wavfmt.nBlockAlign * wavfmt.nSamplesPerSec;
    
        errcode = open_recorder(esr->recorder, devid, &wavfmt);
        if (errcode != 0) {
            esr_dbg("打开录音设备失败: %d", errcode);
            errcode = E_SR_RECORDFAIL;
            goto fail;
        }
    }

    return 0;

fail:
    if (esr->recorder) {
        destroy_recorder(esr->recorder);
        esr->recorder = NULL;
    }

    return errcode;
}

int EsrStartListening(struct EsrRecognizer* esr)
{
    int ret;
    int errcode = 0;
    int index[] = { 0 };

    if (esr->state >= ESR_STATE_STARTED) {
        esr_dbg("已经开始监听.");
        return E_SR_ALREADY;
    }
    index[0] = 0;
    errcode = AIKIT_SpecifyDataSet(esr->ABILITY, "FSA", index, sizeof(index) / sizeof(int));
    if (errcode != 0)
        return errcode;
    errcode = AIKIT_Start(esr->ABILITY, AIKIT_Builder::build(esr->paramBuilder), nullptr, &esr->handle);
    if (0 != errcode)
    {
        esr_dbg("AIKIT_Start 失败! 错误码:%d", errcode);
        return errcode;
    }
    esr->audio_status = AIKIT_DataBegin;

    if (esr->aud_src == ESR_MIC) {
        ret = start_record(esr->recorder);
        if (ret != 0) {
            esr_dbg("开始录音失败: %d", ret);
            ret = AIKIT_End(esr->handle);
            esr->handle = NULL;
            return E_SR_RECORDFAIL;
        }
    }

    esr->state = ESR_STATE_STARTED;

    AIKITDLL::LogInfo("开始监听...");
    return 0;
}

static void wait_for_rec_stop(struct recorder* rec, unsigned int timeout_ms)
{
    while (!is_record_stopped(rec)) {
        Sleep(1);
        if (timeout_ms != (unsigned int)-1)
            if (0 == timeout_ms--)
                break;
    }
}

int EsrStopListening(struct EsrRecognizer* esr)
{
    int ret = 0;
    AiAudio* aiAudio_raw = NULL;

    if (esr->state < ESR_STATE_STARTED) {
        esr_dbg("未开始或已停止.");
        return 0;
    }

    if (esr->aud_src == ESR_MIC) {
        ret = stop_record(esr->recorder);
        if (ret != 0) {
            esr_dbg("停止失败!");
            return E_SR_RECORDFAIL;
        }
        wait_for_rec_stop(esr->recorder, (unsigned int)-1);
    }
    esr->state = ESR_STATE_INIT;
    esr->dataBuilder->clear();
    aiAudio_raw = AiAudio::get("audio")->data(NULL, 0)->status(AIKIT_DataEnd)->valid();
    esr->dataBuilder->payload(aiAudio_raw);
    AIKITDLL::LogInfo("停止监听");
    ret = ESRGetRlt(esr->handle, esr->dataBuilder);
    if (ret != 0 && ret != ESR_HAS_RESULT) {
        esr_dbg("写入最后样本失败: %d", ret);
        AIKIT_End(esr->handle);
        return ret;
    }

    AIKIT_End(esr->handle);
    esr->handle = NULL;
    return 0;
}

int EsrWriteAudioData(struct EsrRecognizer* esr, char* data, unsigned int len)
{
    AiAudio* aiAudio_raw = NULL;
    int ret = 0;
    if (!esr)
        return E_SR_INVAL;
    if (!data || !len)
        return 0;

    esr->dataBuilder->clear();
    aiAudio_raw = AiAudio::get("audio")->data(data, len)->status(esr->audio_status)->valid();
    esr->dataBuilder->payload(aiAudio_raw);

    AIKITDLL::LogDebug("写入音频数据");
    ret = ESRGetRlt(esr->handle, esr->dataBuilder);
    if (ret) {
        esr->audio_status = AIKIT_DataEnd;
        end_esr(esr);
        return ret;
    }
    esr->audio_status = AIKIT_DataContinue;

    return 0;
}

void EsrUninit(struct EsrRecognizer* esr)
{
    if (esr->recorder) {
        if (!is_record_stopped(esr->recorder))
            stop_record(esr->recorder);
        close_recorder(esr->recorder);
        destroy_recorder(esr->recorder);
        esr->recorder = NULL;
    }

    if (esr->dataBuilder != nullptr) {
        delete esr->dataBuilder;
        esr->dataBuilder = nullptr;
    }
    if (esr->paramBuilder != nullptr)
    {
        delete esr->paramBuilder;
        esr->paramBuilder = nullptr;
    }
}

int EsrFromFile(const char* abilityID, const char* audio_path, int fsa_count, long* readLen)
{
    AIKITDLL::LogInfo("开始处理音频文件识别...");

    int ret = 0;
    FILE* file = nullptr;
    long fileSize = 0;
    long curLen = 0;
    char data[FRAME_LEN_ESR] = { 0 };
    int* index = nullptr;
    
    AIKIT_DataStatus status = AIKIT_DataBegin;
    AIKIT::AIKIT_DataBuilder* dataBuilder = nullptr;
    AIKIT_HANDLE* handle = nullptr;
    AIKIT::AiAudio* aiAudio_raw = nullptr;

    // 防止内存分配失败
    index = (int*)malloc(fsa_count * sizeof(int));
    if (index == nullptr) {
        AIKITDLL::LogError("内存分配失败");
        return -1;
    }

    for (int i = 0; i < fsa_count; ++i)
    {
        index[i] = i;
    }
    
    AIKITDLL::LogInfo("正在指定数据集...");
    ret = AIKIT::AIKIT_SpecifyDataSet(abilityID, "FSA", index, fsa_count);
    if (ret != 0)
    {
        AIKITDLL::LogError("AIKIT_SpecifyDataSet 失败，错误码: %d", ret);
    }
    AIKITDLL::LogInfo("数据集指定成功");

    // 创建参数构建器
    AIKIT::AIKIT_ParamBuilder* paramBuilder = AIKIT::AIKIT_ParamBuilder::create();
    if (paramBuilder == nullptr) {
        AIKITDLL::LogError("创建 ParamBuilder 失败");
        ret = -1;
    }
    
    // 设置参数
    paramBuilder->clear();
    paramBuilder->param("languageType", 0);    // 0-中文 1-英文
    paramBuilder->param("vadEndGap", 60);
    paramBuilder->param("vadOn", true);
    paramBuilder->param("beamThreshold", 20);
    paramBuilder->param("hisGramThreshold", 3000);
    paramBuilder->param("postprocOn", false);
    paramBuilder->param("vadResponsetime", 1000);
    paramBuilder->param("vadLinkOn", true);
    paramBuilder->param("vadSpeechEnd", 80);
    
    // 启动能力
    AIKITDLL::LogInfo("正在启动语音识别能力...");
    ret = AIKIT::AIKIT_Start(abilityID, AIKIT::AIKIT_Builder::build(paramBuilder), nullptr, &handle);
    if (ret != 0)
    {
        AIKITDLL::LogError("AIKIT_Start 失败，错误码: %d", ret);
        if (paramBuilder) delete paramBuilder;
    }
    AIKITDLL::LogInfo("语音识别能力启动成功");

    // 检查文件路径
    if (audio_path == nullptr) {
        AIKITDLL::LogError("音频文件路径为空");
        ret = -1;
        if (paramBuilder) delete paramBuilder;
    }

    // 打开音频文件
    AIKITDLL::LogInfo("正在打开音频文件: %s", audio_path);
    errno_t err = fopen_s(&file, audio_path, "rb");
    if (err != 0 || file == nullptr)
    {
        AIKITDLL::LogError("打开音频文件失败: %s，错误码: %d", audio_path, err);
        ret = -1;
        if (paramBuilder) delete paramBuilder;
        goto exit;
    }
    AIKITDLL::LogInfo("音频文件打开成功");

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    AIKITDLL::LogInfo("音频文件大小: %ld 字节", fileSize);
    
    // 创建数据构建器
    dataBuilder = AIKIT::AIKIT_DataBuilder::create();
    if (dataBuilder == nullptr) {
        AIKITDLL::LogError("创建 DataBuilder 失败");
        ret = -1;
        if (paramBuilder) delete paramBuilder;
        goto exit;
    }

    // 处理音频数据
    while (fileSize > *readLen) {
        curLen = fread(data, 1, sizeof(data), file);
        *readLen += curLen;
        dataBuilder->clear();
        
        if (*readLen == FRAME_LEN_ESR) {
            status = AIKIT_DataBegin;
        }
        else {
            status = AIKIT_DataContinue;
        }

        // 构建音频数据
        aiAudio_raw = AIKIT::AiAudio::get("audio")->data(data, curLen)->status(status)->valid();
        dataBuilder->payload(aiAudio_raw);

        // 获取识别结果
        AIKITDLL::LogDebug("正在处理音频数据片段，当前位置: %ld 字节", *readLen);
        ret = ESRGetRlt(handle, dataBuilder);
        if (ret != 0 && ret != ESR_HAS_RESULT) {
            AIKITDLL::LogError("处理音频数据失败，错误码: %d", ret);
            goto exit;
        }

        // 等待一小段时间，避免CPU占用过高
        Sleep(10);
    }

    // 发送结束标记
    *readLen = -1;
    dataBuilder->clear();
    status = AIKIT_DataEnd;
    aiAudio_raw = AIKIT::AiAudio::get("audio")->data(data, 0)->status(status)->valid();
    dataBuilder->payload(aiAudio_raw);

    AIKITDLL::LogInfo("发送音频数据结束标记");
    ret = ESRGetRlt(handle, dataBuilder);
    if (ret != 0 && ret != ESR_HAS_RESULT) {
        AIKITDLL::LogError("发送结束标记失败，错误码: %d", ret);
        goto exit;
    }

    AIKITDLL::LogInfo("正在结束语音识别能力...");
    ret = AIKIT::AIKIT_End(handle);
    if (ret != 0)
    {
        AIKITDLL::LogError("AIKIT_End 失败，错误码: %d", ret);
    }

exit:
    // 确保所有资源正确释放
    if (handle != nullptr) {
        AIKIT::AIKIT_End(handle);
        handle = nullptr;
    }

    if (dataBuilder != nullptr) {
        delete dataBuilder;
        dataBuilder = nullptr;
    }
    
    if (paramBuilder != nullptr) {
        delete paramBuilder;
        paramBuilder = nullptr;
    }
    
    if (file != nullptr) {
        fclose(file);
        file = nullptr;
    }
    
    if (index != nullptr) {
        free(index);
        index = nullptr;
    }

    return ret;
}