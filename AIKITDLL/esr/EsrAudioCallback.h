#pragma once
#include "esr/EsrRecognizer.h"
// 音频流回调与录音相关处理声明
void esr_cb(char* data, unsigned long len, void* user_para);
void end_esr(struct EsrRecognizer* esr);
