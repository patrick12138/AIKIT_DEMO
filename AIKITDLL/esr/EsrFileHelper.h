#pragma once

namespace esr {
    // 文件识别相关声明
    int EsrFromFile(const char* abilityID, const char* audio_path, int fsa_count, long* readLen);
    void TestEsr(const AIKIT_Callbacks& cbs);
}
