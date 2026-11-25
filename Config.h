// Config.h - Gerenciamento de configurações do Vizu (HKCU\Software\DesignThumbs)
#pragma once
#include <windows.h>

namespace cfg {

struct Config {
    DWORD themeMode = 0;          // 0=Windows, 1=Light, 2=Dark, 3=Crispy, 4=Classic
    DWORD webpQuality = 75;
    DWORD neon = 0xFF6633;
    bool tray = true;
    bool space = true;
    bool alt = true;
    bool picker = true;
    bool autoUpdate = true;
    bool dpi300 = false;
    bool pdfRaw = false;
    bool vectorTextPaths = false;
    bool tempPermanent = false;
    DWORD altDelay = 500;
    DWORD quality = 75;

    // Chuva digital (tema Classic) - PLANO-CHUVA-DIGITAL
    bool rainEnabled = false;
    DWORD rainColor = 0x0E1014;   // Grafite padrão
    DWORD rainOpacity = 22;       // 5-60%, padrão 22%
    DWORD rainCycleMs = 20000;    // 10000-40000ms, passos de 5000
    DWORD rainHeadRatio = 15;     // 0-60%, padrão 15%
    DWORD rainCharset = 0;        // 0=Katakana, 1=Latin, 2=Custom
    DWORD rainFps = 24;           // 20/24/30
    std::wstring rainCustomChars;
};

extern Config g_cfg;
extern bool g_lite;

static const wchar_t* kCfgKey = L"Software\\DesignThumbs";

void LoadConfig();
void SaveConfig();
DWORD ReadDword(const wchar_t* name, DWORD def);
void WriteDword(const wchar_t* name, DWORD v);

} // namespace cfg