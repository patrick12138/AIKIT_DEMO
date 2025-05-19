#ifndef SAMPLE_COMMON_H
#define SAMPLE_COMMON_H

#include <fstream>
#include <assert.h>
#include <cstring>
#include <atomic>

#include<Windows.h>


#include "aikit_biz_api.h"
#include "aikit_constant.h"
#include "aikit_biz_config.h"

#pragma comment(lib, "winmm.lib")  

#define FRAME_LEN	640 //16k采样率的16bit音频，一帧的大小为640B, 时长20ms

using namespace std;
using namespace AIKIT;

static const char* XTTS_ABILITY = "e2e44feff";
static const char* IVW_ABILITY = "e867a88f2";
static const char* CNENIVW_ABILITY = "e3c82b5c3";
static const char* ESR_ABILITY = "e75f07b62";
extern FILE* fin;
extern std::atomic_bool ttsFinished;
extern int wakeupFlage;

int Ivw70Init();
int Ivw70Uninit();

void TestIvw70(const AIKIT_Callbacks& cbs);

void TestESR(const AIKIT_Callbacks& cbs);

void TestXtts(const AIKIT_Callbacks& cbs);


#endif
