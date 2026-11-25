// Config.cpp - Implementação do gerenciamento de configurações
#include "Config.h"
#include "VectorConvert.h"
#include "Ui.h"

namespace cfg {

Config g_cfg;
bool g_lite = false;

DWORD ReadDword(const wchar_t* name, DWORD def) {
    DWORD v = def, sz = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, kCfgKey, name, RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v;
}

void WriteDword(const wchar_t* name, DWORD v) {
    RegSetKeyValueW(HKEY_CURRENT_USER, kCfgKey, name, REG_DWORD, &v, sizeof(v));
}

void LoadConfig() {
    g_cfg.space = ReadDword(L"SpacePreview", 1) != 0;
    g_lite = vec::Edition() == vec::kLite;
    g_cfg.alt = !g_lite && ReadDword(L"AltButtons", 1) != 0;
    g_cfg.altDelay = max(150ul, min(3000ul, (unsigned long)ReadDword(L"AltDelay", 500)));
    g_cfg.quality = max(50ul, min(100ul, (unsigned long)ReadDword(L"JpgQuality", 75)));
    g_cfg.dpi300 = ReadDword(L"ConvertDpi", 72) == 300;
    g_cfg.picker = ReadDword(L"ColorPicker", 1) != 0;
    { // v9.2: novos temas (Minimalista Claro/Escuro, Padrão Windows, Crispy); migra o valor antigo
        DWORD old = ReadDword(L"Theme", 0), mig[4] = {ui::kThemeWindows, ui::kThemeLight, ui::kThemeDark, ui::kThemeCrispy};
        g_cfg.themeMode = min(4ul, (unsigned long)ReadDword(L"Theme2", mig[min(3ul, (unsigned long)old)]));
    }
    g_cfg.webpQuality = max(30ul, min(100ul, (unsigned long)ReadDword(L"WebpQuality", 75)));
    g_cfg.tray = ReadDword(L"TrayIcon", 1) != 0;
    g_cfg.autoUpdate = ReadDword(L"AutoUpdate", 1) != 0;
    g_cfg.pdfRaw = ReadDword(L"PdfUncompressed", 0) != 0;
    g_cfg.vectorTextPaths = ReadDword(L"VectorTextPaths", 0) != 0;
    g_cfg.tempPermanent = ReadDword(L"TempPermanent", 0) != 0;
    g_cfg.neon = ReadDword(L"NeonColor", 0xFF6633) & 0xFFFFFF;
    ui::NeonColor() = g_cfg.neon;

    // Chuva digital (tema Classic) - PLANO-CHUVA-DIGITAL
    g_cfg.rainEnabled = ReadDword(L"RainEnabled", 0) != 0;
    g_cfg.rainColor = ReadDword(L"RainColor", 0x0E1014) & 0xFFFFFF;
    g_cfg.rainOpacity = max(5ul, min(60ul, (unsigned long)ReadDword(L"RainOpacity", 22)));
    g_cfg.rainCycleMs = max(10000ul, min(40000ul, (unsigned long)ReadDword(L"RainCycleMs", 20000)));
    // Garantir passo de 5000
    g_cfg.rainCycleMs = ((g_cfg.rainCycleMs + 2500) / 5000) * 5000;
    g_cfg.rainHeadRatio = min(60ul, (unsigned long)ReadDword(L"RainHeadRatio", 15));
    g_cfg.rainCharset = min(2ul, (unsigned long)ReadDword(L"RainCharset", 0));
    g_cfg.rainFps = ReadDword(L"RainFps", 24);
    if (g_cfg.rainFps != 20 && g_cfg.rainFps != 24 && g_cfg.rainFps != 30) g_cfg.rainFps = 24;
    g_cfg.rainCustomChars = ReadStr(L"RainCustomChars", L"");
}

void SaveConfig() {
    WriteDword(L"SpacePreview", g_cfg.space);
    WriteDword(L"AltButtons", g_cfg.alt);
    WriteDword(L"AltDelay", g_cfg.altDelay);
    WriteDword(L"JpgQuality", g_cfg.quality);
    WriteDword(L"ConvertDpi", g_cfg.dpi300 ? 300 : 72);
    WriteDword(L"ColorPicker", g_cfg.picker);
    WriteDword(L"Theme2", g_cfg.themeMode);
    WriteDword(L"WebpQuality", g_cfg.webpQuality);
    WriteDword(L"TrayIcon", g_cfg.tray);
    WriteDword(L"AutoUpdate", g_cfg.autoUpdate);
    WriteDword(L"PdfUncompressed", g_cfg.pdfRaw);
    WriteDword(L"VectorTextPaths", g_cfg.vectorTextPaths);
    WriteDword(L"TempPermanent", g_cfg.tempPermanent);
    WriteDword(L"NeonColor", g_cfg.neon);

    // Chuva digital (tema Classic) - PLANO-CHUVA-DIGITAL
    WriteDword(L"RainEnabled", g_cfg.rainEnabled);
    WriteDword(L"RainColor", g_cfg.rainColor);
    WriteDword(L"RainOpacity", g_cfg.rainOpacity);
    WriteDword(L"RainCycleMs", g_cfg.rainCycleMs);
    WriteDword(L"RainHeadRatio", g_cfg.rainHeadRatio);
    WriteDword(L"RainCharset", g_cfg.rainCharset);
    WriteDword(L"RainFps", g_cfg.rainFps);
    WriteStr(L"RainCustomChars", g_cfg.rainCustomChars);
}

} // namespace cfg