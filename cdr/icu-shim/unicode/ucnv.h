#pragma once
#include "utypes.h"
typedef struct UConverter UConverter;
#ifdef __cplusplus
extern "C" {
#endif
UConverter* ucnv_open(const char* name, UErrorCode* err);
void ucnv_close(UConverter* cnv);
UChar32 ucnv_getNextUChar(UConverter* cnv, const char** source, const char* sourceLimit, UErrorCode* err);
#ifdef __cplusplus
}
#endif
