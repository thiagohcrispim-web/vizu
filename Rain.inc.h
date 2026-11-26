// Rain.inc.h - Chuva digital para o tema Vizu Classic
// WorkerW layer entre papel de parede e ícones, loop perfeito, hash determinístico
#pragma once
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <wincodec.h>
#include "Ui.h"

namespace rain {

// Configurações da chuva (seção Classic do GUIA-DESIGN)
struct Config {
    bool enabled = false;           // Toggle: chuva no fundo
    UINT color = 0x0E1014;          // Cor (5 prontas + HEX livre), padrão grafite
    float opacity = 0.22f;          // Opacidade 5-60%, padrão 22%
    int cycleMs = 20000;            // Ciclo do loop 10-40s (passos 5), padrão 20s
    float headRatio = 0.15f;        // Cabeça 0-60%, padrão 15%
    int charset = 0;                // Conjunto de caracteres (0=Katakana espelhado, 1=Latin, 2=Custom)
    int fps = 24;                   // FPS 20/24/30, padrão 24
    std::wstring customChars;       // Caracteres personalizados se charset=2
};

inline Config g_cfg;
inline bool g_running = false;
inline HWND g_workerW = nullptr;
inline HWND g_rainWnd = nullptr;
inline ID2D1Factory* g_d2d = nullptr;
inline IDWriteFactory* g_dw = nullptr;
inline ID2D1HwndRenderTarget* g_rt = nullptr;
inline IDWriteTextFormat* g_font = nullptr;
inline std::vector<UINT> g_chars;   // Caracteres como codepoints
inline std::vector<float> g_colSpeeds;
inline std::vector<float> g_colOffsets;
inline std::vector<int> g_colLengths;
inline int g_cols = 0;
inline int g_rows = 0;
inline float g_charW = 0, g_charH = 0;
inline LARGE_INTEGER g_freq = {};
inline LARGE_INTEGER g_startTime = {};
inline UINT_PTR g_timerId = 0;

// Hash determinístico (sem RNG com estado) - xorshift32
inline uint32_t g_seed = 0xC0FFEE;
inline uint32_t Rand() {
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return g_seed;
}

// Inicializa conjunto de caracteres
inline void InitCharset() {
    g_chars.clear();
    if (g_cfg.charset == 0) { // Katakana espelhado (0x30A0-0x30FF)
        for (uint32_t c = 0x30A0; c <= 0x30FF; ++c) g_chars.push_back(c);
    } else if (g_cfg.charset == 1) { // Latin alfanumérico
        for (uint32_t c = '0'; c <= '9'; ++c) g_chars.push_back(c);
        for (uint32_t c = 'A'; c <= 'Z'; ++c) g_chars.push_back(c);
        for (uint32_t c = 'a'; c <= 'z'; ++c) g_chars.push_back(c);
    } else if (g_cfg.charset == 2 && !g_cfg.customChars.empty()) {
        for (wchar_t c : g_cfg.customChars) g_chars.push_back((UINT)c);
    }
    if (g_chars.empty()) { // fallback
        for (uint32_t c = 0x30A0; c <= 0x30FF; ++c) g_chars.push_back(c);
    }
}

inline ID2D1Bitmap* g_wall = nullptr;   // papel de parede desenhado por baixo da chuva
inline HWND g_insertAfter = nullptr;    // 24H2: fica logo abaixo dos ícones (SHELLDLL_DefView)
inline RECT g_area = {};                // área da janela (coordenadas da tela)

// Onde fica a camada do papel de parede (atrás dos ícones):
//  - Windows 11 24H2+: Progman tem como filhas SHELLDLL_DefView (ícones) e WorkerW (papel de parede).
//    A chuva vira filha do Progman, logo abaixo dos ícones.
//  - Windows 10 / 11 até 23H2: a WorkerW de nível superior com SHELLDLL_DefView guarda os ícones e a
//    WorkerW seguinte (na ordem das janelas) é a do papel de parede. A chuva vira filha dela.
inline HWND FindWallpaperParent(HWND* insertAfter) {
    *insertAfter = nullptr;
    HWND progman = FindWindowW(L"Progman", nullptr);
    if (!progman) return nullptr;
    // pede ao Explorer para criar a WorkerW (mensagem não documentada usada por papéis de parede animados)
    SendMessageTimeoutW(progman, 0x052C, 0xD, 0x1, SMTO_NORMAL, 1000, nullptr);
    SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
    HWND defView = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defView && FindWindowExW(progman, nullptr, L"WorkerW", nullptr)) { *insertAfter = defView; return progman; } // 24H2+
    HWND iconsWorker = nullptr;
    for (HWND w = FindWindowExW(nullptr, nullptr, L"WorkerW", nullptr); w; w = FindWindowExW(nullptr, w, L"WorkerW", nullptr))
        if (FindWindowExW(w, nullptr, L"SHELLDLL_DefView", nullptr)) { iconsWorker = w; break; }
    if (iconsWorker) {
        if (HWND wall = FindWindowExW(nullptr, iconsWorker, L"WorkerW", nullptr)) return wall;
    }
    if (defView) { *insertAfter = defView; return progman; } // sem WorkerW: desenha no Progman, abaixo dos ícones
    return nullptr;
}

// Carrega o papel de parede atual como bitmap do Direct2D (nulo se não houver imagem).
inline void LoadWallpaper() {
    if (g_wall) { g_wall->Release(); g_wall = nullptr; }
    wchar_t path[MAX_PATH] = {};
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0) || !path[0] || !g_rt) return;
    IWICImagingFactory* wic = nullptr; IWICBitmapDecoder* dec = nullptr; IWICBitmapFrameDecode* fr = nullptr; IWICFormatConverter* cv = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) &&
        SUCCEEDED(wic->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &fr)) && SUCCEEDED(wic->CreateFormatConverter(&cv)) &&
        SUCCEEDED(cv->Initialize(fr, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut)))
        g_rt->CreateBitmapFromWicBitmap(cv, nullptr, &g_wall);
    if (cv) cv->Release(); if (fr) fr->Release(); if (dec) dec->Release(); if (wic) wic->Release();
}

// Pausar quando não aparece ou não convém (janela em tela cheia/maximizada, bateria baixa, animações desligadas)
inline bool ShouldPause() {
    HWND fg = GetForegroundWindow();
    if (!fg) return true; // tela bloqueada
    wchar_t cls[64] = {}; GetClassNameW(fg, cls, 64);
    if (wcscmp(cls, L"Progman") && wcscmp(cls, L"WorkerW")) { // a própria área de trabalho em foco não conta
        RECT rc; GetWindowRect(fg, &rc);
        MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST), &mi);
        if (rc.left <= mi.rcWork.left && rc.right >= mi.rcWork.right && rc.top <= mi.rcWork.top && rc.bottom >= mi.rcWork.bottom) return true;
    }
    SYSTEM_POWER_STATUS ps;
    if (GetSystemPowerStatus(&ps) && ps.ACLineStatus == 0 && (ps.BatteryFlag & (2 | 4))) return true; // na bateria e baixa
    BOOL animOn = TRUE; SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animOn, 0);
    return !animOn;
}

// Desenha um quadro: papel de parede (em cada monitor, preenchendo) e a chuva por cima.
inline void DrawFrame() {
    if (!g_running || !g_rainWnd) return;
    if (!g_rt) {
        RECT rc; GetClientRect(g_rainWnd, &rc);
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96, 96);
        if (FAILED(g_d2d->CreateHwndRenderTarget(props, D2D1::HwndRenderTargetProperties(g_rainWnd, D2D1::SizeU(rc.right, rc.bottom)), &g_rt))) return;
        g_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        LoadWallpaper();
    }
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float elapsedMs = (float)((now.QuadPart - g_startTime.QuadPart) * 1000.0 / g_freq.QuadPart);
    float cycleProgress = fmodf(elapsedMs, (float)g_cfg.cycleMs) / g_cfg.cycleMs; // loop perfeito

    g_rt->BeginDraw();
    COLORREF bg = GetSysColor(COLOR_DESKTOP);
    g_rt->Clear(D2D1::ColorF(GetRValue(bg) / 255.f, GetGValue(bg) / 255.f, GetBValue(bg) / 255.f));
    if (g_wall) { // "preencher" em cada monitor, como o Windows faz por padrão
        D2D1_SIZE_F ws = g_wall->GetSize();
        EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR, HDC, LPRECT m, LPARAM lp) -> BOOL {
            D2D1_SIZE_F ws = *(D2D1_SIZE_F*)lp;
            float mw = (float)(m->right - m->left), mh = (float)(m->bottom - m->top);
            float k = std::max(mw / ws.width, mh / ws.height), w = ws.width * k, h = ws.height * k;
            float x = m->left - g_area.left + (mw - w) / 2, y = m->top - g_area.top + (mh - h) / 2;
            g_rt->PushAxisAlignedClip(D2D1::RectF((float)(m->left - g_area.left), (float)(m->top - g_area.top), (float)(m->right - g_area.left), (float)(m->bottom - g_area.top)), D2D1_ANTIALIAS_MODE_ALIASED);
            g_rt->DrawBitmap(g_wall, D2D1::RectF(x, y, x + w, y + h), 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            g_rt->PopAxisAlignedClip();
            return TRUE;
        }, (LPARAM)&ws);
    }
    float r = ((g_cfg.color >> 16) & 0xFF) / 255.0f, g = ((g_cfg.color >> 8) & 0xFF) / 255.0f, b = (g_cfg.color & 0xFF) / 255.0f;
    ID2D1SolidColorBrush* brush = nullptr; ID2D1SolidColorBrush* head = nullptr;
    g_rt->CreateSolidColorBrush(D2D1::ColorF(r, g, b), &brush);
    g_rt->CreateSolidColorBrush(D2D1::ColorF(r + (1 - r) * 0.85f, g + (1 - g) * 0.85f, b + (1 - b) * 0.85f), &head); // cabeça quase branca: aparece até com a cor grafite
    int frame = (int)(elapsedMs / (1000.0f / std::max(1, g_cfg.fps)));
    for (int col = 0; brush && head && col < g_cols; ++col) {
        float x = col * g_charW;
        int length = g_colLengths[col];
        float headPos = fmodf(cycleProgress * (g_rows + length) * g_colSpeeds[col] * 2 + g_colOffsets[col], (float)g_rows + length);
        int headRow = (int)headPos;
        int headN = std::max(1, (int)(length * g_cfg.headRatio));
        for (int i = 0; i < length; ++i) {
            int row = headRow - i;
            if (row < 0 || row >= g_rows) continue;
            float alpha = i < headN ? std::min(1.0f, g_cfg.opacity * 3.0f) : (1.0f - (float)i / length) * g_cfg.opacity;
            if (alpha < 0.004f) continue;
            uint32_t cs = (col * 73856093u) ^ (row * 19349663u) ^ ((uint32_t)(frame / 6) * 83492791u);
            cs ^= cs << 13; cs ^= cs >> 17; cs ^= cs << 5;
            wchar_t wc = (wchar_t)g_chars[cs % g_chars.size()];
            ID2D1SolidColorBrush* br = i < headN ? head : brush;
            br->SetOpacity(alpha);
            g_rt->DrawTextW(&wc, 1, g_font, D2D1::RectF(x, row * g_charH, x + g_charW, (row + 1) * g_charH), br);
        }
    }
    if (brush) brush->Release(); if (head) head->Release();
    HRESULT hr = g_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) { if (g_wall) { g_wall->Release(); g_wall = nullptr; } g_rt->Release(); g_rt = nullptr; }
    else if (FAILED(hr)) DIAG_HR("chuva: desenho falhou", hr);
}

// Cria a janela da chuva na camada do papel de parede (atrás dos ícones).
inline bool CreateRainWindow() {
    g_workerW = FindWallpaperParent(&g_insertAfter);
    if (!g_workerW) { diag::Note("chuva", "camada do papel de parede não encontrada"); return false; }
    RECT rc; GetClientRect(g_workerW, &rc);
    g_area = {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), 0, 0};
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) { w = GetSystemMetrics(SM_CXVIRTUALSCREEN); h = GetSystemMetrics(SM_CYVIRTUALSCREEN); }

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc = [](HWND hw, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            switch (msg) {
            case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(hw, &ps); EndPaint(hw, &ps); DrawFrame(); return 0; }
            case WM_ERASEBKGND: return 1;
            case WM_TIMER: if (!ShouldPause()) DrawFrame(); return 0;
            case WM_SETTINGCHANGE: if (wp == SPI_SETDESKWALLPAPER && g_rt) LoadWallpaper(); DrawFrame(); return 0;
            case WM_DISPLAYCHANGE: case WM_SIZE:
                if (g_wall) { g_wall->Release(); g_wall = nullptr; }
                if (g_rt) { g_rt->Release(); g_rt = nullptr; }
                return 0;
            }
            return DefWindowProcW(hw, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"VizuRain";
        RegisterClassExW(&wc);
        registered = true;
    }
    // Filha comum (sem camadas): desenhamos o papel de parede e a chuva por cima, opaco.
    // No 24H2 o Progman é composto pelo DWM sem bitmap próprio: uma filha comum fica invisível ali.
    // Filha "em camadas" com opacidade total (Windows 8+, o manifesto declara Windows 10) aparece e o Direct2D desenha nela.
    DWORD ex = WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | (g_insertAfter ? WS_EX_LAYERED : 0);
    g_rainWnd = CreateWindowExW(ex, L"VizuRain", L"", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                                0, 0, w, h, g_workerW, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_rainWnd) { diag::Note("chuva", "CreateWindow", HRESULT_FROM_WIN32(GetLastError())); return false; }
    if (ex & WS_EX_LAYERED) SetLayeredWindowAttributes(g_rainWnd, 0, 255, LWA_ALPHA);
    if (g_insertAfter) SetWindowPos(g_rainWnd, g_insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); // logo abaixo dos ícones

    if (!g_font && FAILED(g_dw->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &g_font))) return false;
    g_font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    g_font->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    g_charW = 12; g_charH = 18;
    IDWriteTextLayout* layout = nullptr; DWRITE_TEXT_METRICS tm;
    if (SUCCEEDED(g_dw->CreateTextLayout(L"\x30A2", 1, g_font, 100, 100, &layout))) {
        if (SUCCEEDED(layout->GetMetrics(&tm)) && tm.width > 2 && tm.height > 2) { g_charW = tm.width + 2; g_charH = tm.height; }
        layout->Release();
    }
    g_cols = (int)(w / g_charW) + 1;
    g_rows = (int)(h / g_charH) + 1;
    g_colSpeeds.assign(g_cols, 0); g_colOffsets.assign(g_cols, 0); g_colLengths.assign(g_cols, 0);
    g_seed = 0xC0FFEE ^ (w * 31) ^ (h * 17) ^ (g_cfg.cycleMs / 1000);
    for (int i = 0; i < g_cols; ++i) {
        g_colSpeeds[i] = 0.5f + (Rand() % 1000) / 2000.0f;
        g_colOffsets[i] = (float)(Rand() % (unsigned)(g_rows + 1));
        g_colLengths[i] = 5 + (int)(Rand() % (unsigned)std::max(1, g_rows / 3));
    }
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_startTime);
    g_running = true;
    g_timerId = SetTimer(g_rainWnd, 1, 1000 / std::max(1, g_cfg.fps), nullptr);
    DrawFrame();
    diag::Log("chuva: ligada (" + std::string(g_insertAfter ? "Progman/24H2" : "WorkerW") + ")");
    return true;
}

// Timer callback - chamado pelo WM_TIMER
inline void OnTimer() {
    if (ShouldPause()) return;
    DrawFrame();
}

// Inicializa factories D2D/DWrite (chamado uma vez no wWinMain)
inline void InitFactories(ID2D1Factory* d2d, IDWriteFactory* dw) {
    g_d2d = d2d;
    g_dw = dw;
}

// Inicia chuva
inline bool Start() {
    if (g_running) return true;
    if (!g_d2d || !g_dw) return false;

    InitCharset();
    return CreateRainWindow();
}

// Para chuva
inline void Stop() {
    if (!g_running) return;
    g_running = false;
    if (g_timerId) { KillTimer(g_rainWnd, g_timerId); g_timerId = 0; }
    if (g_rainWnd) { DestroyWindow(g_rainWnd); g_rainWnd = nullptr; }
    if (g_wall) { g_wall->Release(); g_wall = nullptr; }
    if (g_rt) { g_rt->Release(); g_rt = nullptr; }
    if (g_font) { g_font->Release(); g_font = nullptr; }
    g_workerW = nullptr;
}

// Atualiza configuração (chamado quando usuário muda settings)
inline void UpdateConfig(const Config& cfg) {
    bool wasRunning = g_running;
    g_cfg = cfg;
    if (wasRunning) {
        Stop();
        Start();
    }
}

// Verifica se deve rodar (tema Classic ativo + enabled)
inline bool ShouldRun(int currentTheme) {
    return (currentTheme == ui::kThemeClassic || currentTheme == ui::kThemeCrispy) && g_cfg.enabled; // Classic e Crispy
}

} // namespace rain