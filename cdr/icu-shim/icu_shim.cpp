// Implementa a ponte de icu-shim/unicode/*.h chamando o icu.dll do Windows (carregado na primeira chamada).
#include <windows.h>
#include "unicode/ucnv.h"
#include "unicode/ucsdet.h"

namespace {
HMODULE Icu() {
    static HMODULE m = LoadLibraryExW(L"icu.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return m;
}
template <class F> F Fn(const char* name) { HMODULE m = Icu(); return m ? (F)(void*)GetProcAddress(m, name) : nullptr; }
#define ICU_CALL(name, type, fail, ...) \
    static auto f = Fn<type>(#name); \
    if (!f) { if (status) *status = U_UNSUPPORTED_ERROR; return fail; } \
    return f(__VA_ARGS__)
}

extern "C" {
UConverter* ucnv_open(const char* name, UErrorCode* status) { ICU_CALL(ucnv_open, UConverter* (*)(const char*, UErrorCode*), nullptr, name, status); }
void ucnv_close(UConverter* c) { static auto f = Fn<void (*)(UConverter*)>("ucnv_close"); if (f && c) f(c); }
UChar32 ucnv_getNextUChar(UConverter* c, const char** s, const char* e, UErrorCode* status) {
    ICU_CALL(ucnv_getNextUChar, UChar32 (*)(UConverter*, const char**, const char*, UErrorCode*), 0xfffd, c, s, e, status);
}
UCharsetDetector* ucsdet_open(UErrorCode* status) { ICU_CALL(ucsdet_open, UCharsetDetector* (*)(UErrorCode*), nullptr, status); }
void ucsdet_close(UCharsetDetector* d) { static auto f = Fn<void (*)(UCharsetDetector*)>("ucsdet_close"); if (f && d) f(d); }
void ucsdet_setText(UCharsetDetector* d, const char* t, int32_t n, UErrorCode* status) {
    static auto f = Fn<void (*)(UCharsetDetector*, const char*, int32_t, UErrorCode*)>("ucsdet_setText");
    if (!f) { *status = U_UNSUPPORTED_ERROR; return; }
    f(d, t, n, status);
}
const UCharsetMatch* ucsdet_detect(UCharsetDetector* d, UErrorCode* status) { ICU_CALL(ucsdet_detect, const UCharsetMatch* (*)(UCharsetDetector*, UErrorCode*), nullptr, d, status); }
const char* ucsdet_getName(const UCharsetMatch* m, UErrorCode* status) { ICU_CALL(ucsdet_getName, const char* (*)(const UCharsetMatch*, UErrorCode*), nullptr, m, status); }
int32_t ucsdet_getConfidence(const UCharsetMatch* m, UErrorCode* status) { ICU_CALL(ucsdet_getConfidence, int32_t (*)(const UCharsetMatch*, UErrorCode*), 0, m, status); }
UBool ucsdet_enableInputFilter(UCharsetDetector* d, UBool on) { static auto f = Fn<UBool (*)(UCharsetDetector*, UBool)>("ucsdet_enableInputFilter"); return f ? f(d, on) : 0; }
}
