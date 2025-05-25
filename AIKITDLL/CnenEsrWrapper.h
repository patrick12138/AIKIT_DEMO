#pragma once

#include "Common.h"
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define FRAME_LEN_ESR 320 // 处理ESR数据时使用的帧长
#ifdef __cplusplus
extern "C" {
#endif

	// 导出定义
#ifdef _WIN32
#ifdef AIKITDLL_EXPORTS
#define AIKITDLL_API __declspec(dllexport)
#else
#define AIKITDLL_API __declspec(dllimport)
#endif
#else
#define AIKITDLL_API
#endif

	// CNEN-ESR相关常量
#define ESR_ABILITY "e75f07b62" // 离线命令词识别能力ID

// ESR结果回调相关错误码
#define ESR_HAS_RESULT 6001

	// ESR能力初始化
	AIKITDLL_API int CnenEsrInit();

	// ESR资源释放
	AIKITDLL_API int CnenEsrUninit();

	// ESR测试函数 - 从麦克风输入
	AIKITDLL_API int EsrFromMicrophone();

	//// ESR测试函数 - 从文件输入
	//AIKITDLL_API int EsrFromFile(const char* audioFilePath);

	// 主测试函数
	AIKITDLL_API void TestEsr(const AIKIT_Callbacks& cbs);

	AIKITDLL_API void TestEsrMicrophone(const AIKIT_Callbacks& cbs);

#ifdef __cplusplus
}
#endif

// 在C++环境中，于AIKITDLL命名空间内声明相关变量
#ifdef __cplusplus
#include <string> // For std::string
#include <atomic> // For std::atomic
#include <mutex>  // For std::mutex

namespace AIKITDLL {
    // ESR能力结果标识
    extern const char ESR_ABILITY_ID[]; // 将宏定义改为常量声明

    // 定义ESR状态枚举 (与CnenEsrWrapper.cpp中保持一致)
    enum EsrStatusVals {
        ESR_STATUS_NONE_INTERNAL = 0,
        ESR_STATUS_PROCESSING_INTERNAL,
        ESR_STATUS_SUCCESS_INTERNAL,
        ESR_STATUS_FAILED_INTERNAL,
        ESR_STATUS_NO_MATCH_INTERNAL // 新增一个未匹配状态
    };    extern std::atomic<int> esrStatus;
    extern std::string lastEsrKeywordResult;
    extern std::string lastEsrErrorInfo;
    extern std::atomic<int> esrResultFlag; // 如果Common.cpp或其他地方需要访问，也应声明
    extern std::mutex esrResultMutex; // 如果需要在命名空间外访问，也应声明
}
#endif

// 内部使用的函数
namespace AIKITDLL {
	// 麦克风输入的ESR处理函数
	int esr_microphone(const char* abilityID);

	// 从文件进行ESR处理的内部实现
	int esr_file(const char* abilityID, const char* audioFilePath, int fsa_count, long* readLen);
}