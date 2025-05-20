#include "EsrAudioCallback.h"
#include "../Common.h"
void esr_cb(char* data, unsigned long len, void* user_para) {
    // 这里只做空实现，后续可迁移原有回调逻辑
}
void end_esr(struct EsrRecognizer* esr) {
    // 这里只做空实现，后续可迁移原有end_esr逻辑
}
