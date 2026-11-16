// Ponte mínima para o ICU que vem com o Windows 10 (1903+) e 11 (icu.dll): só o que a libcdr usa.
// As funções são carregadas do icu.dll na primeira chamada; sem ele, falham com erro (a libcdr então ignora a
// conversão de codificação daquele texto) em vez de impedir o Vizu de abrir.
#pragma once
#include <stdint.h>
typedef int32_t UChar32;
typedef uint16_t UChar;
typedef int8_t UBool;
typedef enum UErrorCode { U_ZERO_ERROR = 0, U_ILLEGAL_ARGUMENT_ERROR = 1, U_MISSING_RESOURCE_ERROR = 2, U_UNSUPPORTED_ERROR = 16 } UErrorCode;
#define U_SUCCESS(x) ((x) <= U_ZERO_ERROR)
#define U_FAILURE(x) ((x) > U_ZERO_ERROR)
#define U_IS_UNICODE_NONCHAR(c) ((c) >= 0xfdd0 && ((c) <= 0xfdef || ((c) & 0xfffe) == 0xfffe) && (c) <= 0x10ffff)
#define U_IS_UNICODE_CHAR(c) ((uint32_t)(c) < 0xd800 || (0xdfff < (c) && (c) <= 0x10ffff && !U_IS_UNICODE_NONCHAR(c)))
