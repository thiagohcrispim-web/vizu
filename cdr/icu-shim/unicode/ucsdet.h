#pragma once
#include "utypes.h"
typedef struct UCharsetDetector UCharsetDetector;
typedef struct UCharsetMatch UCharsetMatch;
#ifdef __cplusplus
extern "C" {
#endif
UCharsetDetector* ucsdet_open(UErrorCode* status);
void ucsdet_close(UCharsetDetector* ucsd);
void ucsdet_setText(UCharsetDetector* ucsd, const char* textIn, int32_t len, UErrorCode* status);
const UCharsetMatch* ucsdet_detect(UCharsetDetector* ucsd, UErrorCode* status);
const char* ucsdet_getName(const UCharsetMatch* ucsm, UErrorCode* status);
int32_t ucsdet_getConfidence(const UCharsetMatch* ucsm, UErrorCode* status);
UBool ucsdet_enableInputFilter(UCharsetDetector* ucsd, UBool filter);
#ifdef __cplusplus
}
#endif
