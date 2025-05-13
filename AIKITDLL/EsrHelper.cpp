#include "pch.h"
#include "EsrHelper.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string>
#include <unordered_map>
#include <process.h>

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

// 添加全局变量存储识别结果
extern "C" {
	// 不同类型结果的缓冲区
	char g_pgsResultBuffer[8192] = { 0 };      // pgs格式结果缓冲区
	char g_htkResultBuffer[8192] = { 0 };      // htk格式结果缓冲区
	char g_plainResultBuffer[8192] = { 0 };    // plain格式结果缓冲区
	char g_vadResultBuffer[8192] = { 0 };      // vad格式结果缓冲区
	char g_readableResultBuffer[8192] = { 0 }; // readable格式结果缓冲区

	// 是否有新结果的标志
	bool g_hasNewPgsResult = false;
	bool g_hasNewHtkResult = false;
	bool g_hasNewPlainResult = false;
	bool g_hasNewVadResult = false;
	bool g_hasNewReadableResult = false;

	// 互斥锁保护共享数据
	CRITICAL_SECTION g_resultLock;
}

// 初始化互斥锁
void InitResultLock() {
	static bool initialized = false;
	if (!initialized) {
		InitializeCriticalSection(&g_resultLock);
		initialized = true;
	}
}

// 添加结果到对应缓冲区
void AddResult(const char* type, const char* result) {
	if (!type || !result || strlen(result) == 0) return;

	EnterCriticalSection(&g_resultLock);

	if (strcmp(type, "pgs") == 0) {
		if (strlen(g_pgsResultBuffer) > 7800) {
			// 缓冲区快满，只保留最后1000个字符
			char temp[1000] = { 0 };
			size_t len = strlen(g_pgsResultBuffer);
			if (len > 1000) {
				strcpy_s(temp, sizeof(temp), g_pgsResultBuffer + len - 1000);
				memset(g_pgsResultBuffer, 0, sizeof(g_pgsResultBuffer));
				strcpy_s(g_pgsResultBuffer, sizeof(g_pgsResultBuffer), temp);
			}
		}

		// 添加新结果，用换行分隔
		if (strlen(g_pgsResultBuffer) > 0) {
			strcat_s(g_pgsResultBuffer, sizeof(g_pgsResultBuffer), "\n");
		}
		strcat_s(g_pgsResultBuffer, sizeof(g_pgsResultBuffer), "pgs:");
		strcat_s(g_pgsResultBuffer, sizeof(g_pgsResultBuffer), result);
		g_hasNewPgsResult = true;
	}
	else if (strcmp(type, "htk") == 0) {
		if (strlen(g_htkResultBuffer) > 7800) {
			// 缓冲区清理
			memset(g_htkResultBuffer, 0, sizeof(g_htkResultBuffer));
		}

		if (strlen(g_htkResultBuffer) > 0) {
			strcat_s(g_htkResultBuffer, sizeof(g_htkResultBuffer), "\n");
		}
		strcat_s(g_htkResultBuffer, sizeof(g_htkResultBuffer), "htk:");
		strcat_s(g_htkResultBuffer, sizeof(g_htkResultBuffer), result);
		g_hasNewHtkResult = true;
	}
	else if (strcmp(type, "plain") == 0) {
		if (strlen(g_plainResultBuffer) > 7800) {
			memset(g_plainResultBuffer, 0, sizeof(g_plainResultBuffer));
		}

		if (strlen(g_plainResultBuffer) > 0) {
			strcat_s(g_plainResultBuffer, sizeof(g_plainResultBuffer), "\n");
		}
		strcat_s(g_plainResultBuffer, sizeof(g_plainResultBuffer), "plain: ");
		strcat_s(g_plainResultBuffer, sizeof(g_plainResultBuffer), result);
		g_hasNewPlainResult = true;
	}
	else if (strcmp(type, "vad") == 0) {
		if (strlen(g_vadResultBuffer) > 7800) {
			memset(g_vadResultBuffer, 0, sizeof(g_vadResultBuffer));
		}

		if (strlen(g_vadResultBuffer) > 0) {
			strcat_s(g_vadResultBuffer, sizeof(g_vadResultBuffer), "\n");
		}
		strcat_s(g_vadResultBuffer, sizeof(g_vadResultBuffer), "vad: ");
		strcat_s(g_vadResultBuffer, sizeof(g_vadResultBuffer), result);
		g_hasNewVadResult = true;
	}
	else if (strcmp(type, "readable") == 0) {
		if (strlen(g_readableResultBuffer) > 7800) {
			memset(g_readableResultBuffer, 0, sizeof(g_readableResultBuffer));
		}

		if (strlen(g_readableResultBuffer) > 0) {
			strcat_s(g_readableResultBuffer, sizeof(g_readableResultBuffer), "\n");
		}
		strcat_s(g_readableResultBuffer, sizeof(g_readableResultBuffer), "readable: ");
		strcat_s(g_readableResultBuffer, sizeof(g_readableResultBuffer), result);
		g_hasNewReadableResult = true;
	}

	LeaveCriticalSection(&g_resultLock);
}

// 判断结果类型并存储
void ProcessRecognitionResult(const char* key, const char* value) {
	if (!key || !value) return;

	// 存储到对应类型的缓冲区
	if (strcmp(key, "pgs") == 0) {
		AddResult("pgs", value);
	}
	else if (strcmp(key, "htk") == 0) {
		AddResult("htk", value);
	}
	else if (strcmp(key, "plain") == 0) {
		AddResult("plain", value);
	}
	else if (strcmp(key, "vad") == 0) {
		AddResult("vad", value);
	}
	else if (strcmp(key, "readable") == 0) {
		AddResult("readable", value);
	}

	// 日志记录
	AIKITDLL::LogInfo("识别结果: %s: %s", key, value);
}

bool is_result = false;
int ESRGetRlt(AIKIT_HANDLE* handle, AIKIT_DataBuilder* dataBuilder)
{
	int ret = 0;
	AIKIT_OutputData* output = nullptr;
	AIKIT_InputData* input_data = AIKIT_Builder::build(dataBuilder);

	// 确保互斥锁已初始化
	InitResultLock();

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

			// 处理识别结果
			ProcessRecognitionResult(node->key, (char*)node->value);

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

	AIKITDLL::LogInfo("状态(state): %d", esr->state);
	AIKITDLL::LogInfo("音频来源(aud_src): %d", esr->aud_src);
	AIKITDLL::LogInfo("音频状态(audio_status): %d", esr->audio_status);
	AIKITDLL::LogInfo("能力ID(ABILITY): %s", esr->ABILITY);
	AIKITDLL::LogInfo("句柄(handle): %p", esr->handle);
	AIKITDLL::LogInfo("数据构建器(dataBuilder): %p", esr->dataBuilder);
	AIKITDLL::LogInfo("参数构建器(paramBuilder): %p", esr->paramBuilder);
	AIKITDLL::LogInfo("录音设备句柄(recorder): %p", esr->recorder);
	AIKITDLL::LogInfo("当前状态: %d", esr->state);

	// 如果已经有handle，输出其详细信息
	if (esr->handle) {
		AIKITDLL::LogInfo("句柄详细信息:");
		AIKITDLL::LogInfo("  用户上下文: %p", esr->handle->usrContext);
		AIKITDLL::LogInfo("  能力ID: %s", esr->handle->abilityID ? esr->handle->abilityID : "空");
		AIKITDLL::LogInfo("  句柄ID: %zu", esr->handle->handleID);
	}
	
	if (esr->state >= ESR_STATE_STARTED) {
		AIKITDLL::LogDebug("识别器已在运行状态,无需重复启动");
		esr_dbg("已经开始监听.");
		return E_SR_ALREADY;
	}

	AIKITDLL::LogDebug("开始指定数据集...");
	index[0] = 0;
	errcode = AIKIT_SpecifyDataSet(esr->ABILITY, "FSA", index, sizeof(index) / sizeof(int));
	if (errcode != 0) {
		AIKITDLL::LogDebug("指定数据集失败,错误码: %d", errcode);
		return errcode;
	}
	AIKITDLL::LogDebug("数据集指定成功");

	AIKITDLL::LogDebug("正在启动AIKIT服务...");
	errcode = AIKIT_Start(esr->ABILITY, AIKIT_Builder::build(esr->paramBuilder), nullptr, &esr->handle);
	if (0 != errcode)
	{
		AIKITDLL::LogDebug("AIKIT_Start 启动失败,错误码: %d", errcode);
		esr_dbg("AIKIT_Start 失败! 错误码:%d", errcode);
		return errcode;
	}
	AIKITDLL::LogDebug("AIKIT服务启动成功");

	esr->audio_status = AIKIT_DataBegin;

	if (esr->aud_src == ESR_MIC) {
		AIKITDLL::LogDebug("正在启动麦克风录音...");
		ret = start_record(esr->recorder);
		if (ret != 0) {
			AIKITDLL::LogDebug("启动录音失败,错误码: %d", ret);
			esr_dbg("开始录音失败: %d", ret);
			ret = AIKIT_End(esr->handle);
			esr->handle = NULL;
			return E_SR_RECORDFAIL;
		}
		AIKITDLL::LogDebug("麦克风录音启动成功");
	}

	esr->state = ESR_STATE_STARTED;

	AIKITDLL::LogInfo("语音识别监听已成功启动");
	AIKITDLL::LogDebug("开始监听语音输入...");
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

// 为WPF应用提供的接口函数 - 获取各种格式的结果
extern "C" __declspec(dllexport) const char* GetPgsResult()
{
	static char buffer[8192] = { 0 };
	EnterCriticalSection(&g_resultLock);
	strcpy_s(buffer, sizeof(buffer), g_pgsResultBuffer);
	LeaveCriticalSection(&g_resultLock);
	return buffer;
}

extern "C" __declspec(dllexport) const char* GetHtkResult()
{
	static char buffer[8192] = { 0 };
	EnterCriticalSection(&g_resultLock);
	strcpy_s(buffer, sizeof(buffer), g_htkResultBuffer);
	LeaveCriticalSection(&g_resultLock);
	return buffer;
}

extern "C" __declspec(dllexport) const char* GetPlainResult()
{
	static char buffer[8192] = { 0 };
	EnterCriticalSection(&g_resultLock);
	strcpy_s(buffer, sizeof(buffer), g_plainResultBuffer);
	LeaveCriticalSection(&g_resultLock);
	return buffer;
}

extern "C" __declspec(dllexport) const char* GetVadResult()
{
	static char buffer[8192] = { 0 };
	EnterCriticalSection(&g_resultLock);
	strcpy_s(buffer, sizeof(buffer), g_vadResultBuffer);
	LeaveCriticalSection(&g_resultLock);
	return buffer;
}

extern "C" __declspec(dllexport) const char* GetReadableResult()
{
	static char buffer[8192] = { 0 };
	EnterCriticalSection(&g_resultLock);
	strcpy_s(buffer, sizeof(buffer), g_readableResultBuffer);
	LeaveCriticalSection(&g_resultLock);
	return buffer;
}

// 检查是否有新结果
extern "C" __declspec(dllexport) bool HasNewPgsResult()
{
	bool result = false;
	EnterCriticalSection(&g_resultLock);
	result = g_hasNewPgsResult;
	g_hasNewPgsResult = false;
	LeaveCriticalSection(&g_resultLock);
	return result;
}

extern "C" __declspec(dllexport) bool HasNewHtkResult()
{
	bool result = false;
	EnterCriticalSection(&g_resultLock);
	result = g_hasNewHtkResult;
	g_hasNewHtkResult = false;
	LeaveCriticalSection(&g_resultLock);
	return result;
}

extern "C" __declspec(dllexport) bool HasNewPlainResult()
{
	bool result = false;
	EnterCriticalSection(&g_resultLock);
	result = g_hasNewPlainResult;
	g_hasNewPlainResult = false;
	LeaveCriticalSection(&g_resultLock);
	return result;
}

extern "C" __declspec(dllexport) bool HasNewVadResult()
{
	bool result = false;
	EnterCriticalSection(&g_resultLock);
	result = g_hasNewVadResult;
	g_hasNewVadResult = false;
	LeaveCriticalSection(&g_resultLock);
	return result;
}

extern "C" __declspec(dllexport) bool HasNewReadableResult()
{
	bool result = false;
	EnterCriticalSection(&g_resultLock);
	result = g_hasNewReadableResult;
	g_hasNewReadableResult = false;
	LeaveCriticalSection(&g_resultLock);
	return result;
}

// 清空所有结果缓冲区
extern "C" __declspec(dllexport) void ClearAllResultBuffers()
{
	EnterCriticalSection(&g_resultLock);
	memset(g_pgsResultBuffer, 0, sizeof(g_pgsResultBuffer));
	memset(g_htkResultBuffer, 0, sizeof(g_htkResultBuffer));
	memset(g_plainResultBuffer, 0, sizeof(g_plainResultBuffer));
	memset(g_vadResultBuffer, 0, sizeof(g_vadResultBuffer));
	memset(g_readableResultBuffer, 0, sizeof(g_readableResultBuffer));
	g_hasNewPgsResult = false;
	g_hasNewHtkResult = false;
	g_hasNewPlainResult = false;
	g_hasNewVadResult = false;
	g_hasNewReadableResult = false;
	LeaveCriticalSection(&g_resultLock);
}