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
};

extern Config g_cfg;
extern bool g_lite;

static const wchar_t* kCfgKey = L"Software\\DesignThumbs";

void LoadConfig();
void SaveConfig();
DWORD ReadDword(const wchar_t* name, DWORD def);
void WriteDword(const wchar_t* name, DWORD v);

} // namespace cfg