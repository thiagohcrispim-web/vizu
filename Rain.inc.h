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
#include <dwmapi.h>
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
    // WallClock (WallEngine, v19.8): relógio gigante na mesma camada, por baixo da chuva.
    bool clock = false;
    UINT clockColors[3] = {0xFF6633, 0xFFFFFF, 0x4A4F57};
    int clockCount = 1;              // 1 a 3 cores (degradê de cima para baixo)
    float clockOpacity = 0.6f;
    int clockSize = 60;              // % da largura do monitor principal (1 a 100)
    bool clockGlass = false;         // vidro: o papel de parede desfocado dentro dos números
    std::wstring clockFont = L"Segoe UI";
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
inline ID2D1Bitmap* g_blur = nullptr;   // o mesmo papel de parede em 1/16, desfocado (vidro do WallClock)
inline IDWriteTextFormat* g_clockFmt = nullptr;
inline float g_clockFmtSize = 0;
inline std::wstring g_clockFmtFont;
inline void ReleaseWall() {
    if (g_wall) { g_wall->Release(); g_wall = nullptr; }
    if (g_blur) { g_blur->Release(); g_blur = nullptr; }
}
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
    ReleaseWall();
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

// Pausar só quando não dá para ver a chuva: TODOS os monitores cobertos por janelas maximizadas/tela cheia,
// tela bloqueada, ou na bateria fraca. (Na 19.5.9 uma janela maximizada em QUALQUER monitor, ou "Mostrar animações"
// desligado no Windows, congelava a chuva inteira.)
inline const char* g_pauseReason = "";
inline unsigned long long g_frames = 0;
inline bool MonitorCovered(HMONITOR mon) {
    MONITORINFO mi = {sizeof(mi)}; if (!GetMonitorInfoW(mon, &mi)) return false;
    struct Ctx { RECT work; bool covered; } c = {mi.rcWork, false};
    EnumWindows([](HWND w, LPARAM lp) -> BOOL {
        auto* c = (Ctx*)lp;
        if (!IsWindowVisible(w) || IsIconic(w)) return TRUE;
        BOOL cloaked = FALSE; DwmGetWindowAttribute(w, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
        if (cloaked) return TRUE;
        LONG ex = GetWindowLongW(w, GWL_EXSTYLE);
        if (ex & (WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE)) return TRUE; // relógio, widgets, sobreposições
        wchar_t cls[64] = {}; GetClassNameW(w, cls, 64);
        if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"Shell_SecondaryTrayWnd")) return TRUE;
        RECT r; if (!GetWindowRect(w, &r)) return TRUE;
        if (r.left <= c->work.left + 8 && r.top <= c->work.top + 8 && r.right >= c->work.right - 8 && r.bottom >= c->work.bottom - 8) { c->covered = true; return FALSE; }
        return TRUE;
    }, (LPARAM)&c);
    return c.covered;
}
inline bool ShouldPause() {
    const char* why = "";
    if (!GetForegroundWindow()) why = "tela bloqueada";
    else {
        SYSTEM_POWER_STATUS ps;
        if (GetSystemPowerStatus(&ps) && ps.ACLineStatus == 0 && (ps.BatteryFlag & (2 | 4))) why = "bateria fraca";
        else {
            bool all = true;
            EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR m, HDC, LPRECT, LPARAM lp) -> BOOL { if (!MonitorCovered(m)) { *(bool*)lp = false; return FALSE; } return TRUE; }, (LPARAM)&all);
            if (all) why = "todos os monitores cobertos por janelas";
        }
    }
    if (strcmp(why, g_pauseReason)) { // registra só quando muda
        diag::Log(std::string("chuva: ") + (why[0] ? std::string("pausada (") + why + ")" : std::string("andando")));
        g_pauseReason = why;
    }
    return why[0] != 0;
}

// Papel de parede reduzido a 1/16 com filtro cúbico: ampliado de volta com filtro linear, vira um desfoque suave.
inline void MakeBlur() {
    if (g_blur || !g_wall || !g_rt) return;
    D2D1_SIZE_F full = g_rt->GetSize();
    ID2D1BitmapRenderTarget* brt = nullptr;
    if (FAILED(g_rt->CreateCompatibleRenderTarget(D2D1::SizeF(std::max(1.0f, full.width / 16), std::max(1.0f, full.height / 16)), &brt))) return;
    ID2D1DeviceContext* dc = nullptr;
    brt->QueryInterface(IID_PPV_ARGS(&dc));
    brt->BeginDraw();
    brt->Clear(D2D1::ColorF(0, 0, 0));
    struct Ctx { ID2D1BitmapRenderTarget* brt; ID2D1DeviceContext* dc; } ctx = {brt, dc};
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR, HDC, LPRECT m, LPARAM lp) -> BOOL {
        auto* c = (Ctx*)lp;
        D2D1_SIZE_F ws = g_wall->GetSize();
        float mw = (float)(m->right - m->left), mh = (float)(m->bottom - m->top);
        float k = std::max(mw / ws.width, mh / ws.height), w = ws.width * k, h = ws.height * k;
        float x = m->left - g_area.left + (mw - w) / 2, y = m->top - g_area.top + (mh - h) / 2;
        D2D1_RECT_F clip = D2D1::RectF((m->left - g_area.left) / 16.0f, (m->top - g_area.top) / 16.0f, (m->right - g_area.left) / 16.0f, (m->bottom - g_area.top) / 16.0f);
        D2D1_RECT_F dst = D2D1::RectF(x / 16, y / 16, (x + w) / 16, (y + h) / 16);
        c->brt->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_ALIASED);
        if (c->dc) c->dc->DrawBitmap(g_wall, &dst, 1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC, nullptr, nullptr);
        else c->brt->DrawBitmap(g_wall, dst, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        c->brt->PopAxisAlignedClip();
        return TRUE;
    }, (LPARAM)&ctx);
    if (SUCCEEDED(brt->EndDraw())) brt->GetBitmap(&g_blur);
    if (dc) dc->Release();
    brt->Release();
}

// WallClock: HH:MM centralizado no monitor principal, com a largura escolhida (1% a 100% do monitor).
inline void DrawClock() {
    SYSTEMTIME t; GetLocalTime(&t);
    wchar_t txt[8]; swprintf(txt, 8, L"%02d:%02d", t.wHour, t.wMinute);
    MONITORINFO mi = {sizeof(mi)};
    if (!GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi)) return;
    float mx = (float)(mi.rcMonitor.left - g_area.left), my = (float)(mi.rcMonitor.top - g_area.top);
    float mw = (float)(mi.rcMonitor.right - mi.rcMonitor.left), mh = (float)(mi.rcMonitor.bottom - mi.rcMonitor.top);
    const std::wstring& fam = g_cfg.clockFont.empty() ? std::wstring(L"Segoe UI") : g_cfg.clockFont;
    // largura do texto a 100 px; o tamanho final sai por regra de três
    auto layoutAt = [&](float size) -> IDWriteTextLayout* {
        if (!g_clockFmt || g_clockFmtSize != size || g_clockFmtFont != fam) {
            if (g_clockFmt) { g_clockFmt->Release(); g_clockFmt = nullptr; }
            if (FAILED(g_dw->CreateTextFormat(fam.c_str(), nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"pt-br", &g_clockFmt))) return nullptr;
            g_clockFmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            g_clockFmtSize = size; g_clockFmtFont = fam;
        }
        IDWriteTextLayout* l = nullptr;
        g_dw->CreateTextLayout(txt, (UINT32)wcslen(txt), g_clockFmt, 100000, 100000, &l);
        return l;
    };
    IDWriteTextLayout* probe = layoutAt(100);
    if (!probe) return;
    DWRITE_TEXT_METRICS pm; probe->GetMetrics(&pm); probe->Release();
    if (pm.width < 1) return;
    float target = mw * std::max(1, std::min(100, g_cfg.clockSize)) / 100.0f;
    float size = std::max(4.0f, std::min(100.0f * target / pm.width, mh * 0.9f / (pm.height / 100.0f)));
    size = floorf(size);
    IDWriteTextLayout* lay = layoutAt(size);
    if (!lay) return;
    DWRITE_TEXT_METRICS m; lay->GetMetrics(&m);
    float x = mx + (mw - m.width) / 2 - m.left, y = my + (mh - m.height) / 2 - m.top;
    float op = std::max(0.05f, std::min(1.0f, g_cfg.clockOpacity));
    if (g_cfg.clockGlass) { // vidro: o papel de parede desfocado preenche os números, com um véu claro por cima
        MakeBlur();
        if (g_blur) {
            ID2D1BitmapBrush* bb = nullptr;
            if (SUCCEEDED(g_rt->CreateBitmapBrush(g_blur, D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR), &bb))) {
                bb->SetTransform(D2D1::Matrix3x2F::Scale(16, 16));
                g_rt->DrawTextLayout(D2D1::Point2F(x, y), lay, bb);
                bb->Release();
            }
        }
        ID2D1SolidColorBrush* veil = nullptr;
        if (SUCCEEDED(g_rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.18f * op + 0.04f), &veil))) { g_rt->DrawTextLayout(D2D1::Point2F(x, y), lay, veil); veil->Release(); }
    } else {
        int n = std::max(1, std::min(3, g_cfg.clockCount));
        auto col = [&](int i) { UINT c = g_cfg.clockColors[i]; return D2D1::ColorF(((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f, op); };
        if (n == 1) {
            ID2D1SolidColorBrush* b = nullptr;
            if (SUCCEEDED(g_rt->CreateSolidColorBrush(col(0), &b))) { g_rt->DrawTextLayout(D2D1::Point2F(x, y), lay, b); b->Release(); }
        } else { // degradê sempre de cima para baixo (90°)
            D2D1_GRADIENT_STOP st[3];
            for (int i = 0; i < n; ++i) st[i] = {(float)i / (n - 1), col(i)};
            ID2D1GradientStopCollection* sc = nullptr; ID2D1LinearGradientBrush* lb = nullptr;
            float top = y + m.top, bottom = y + m.top + m.height;
            if (SUCCEEDED(g_rt->CreateGradientStopCollection(st, (UINT32)n, &sc)) &&
                SUCCEEDED(g_rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, top), D2D1::Point2F(0, bottom)), sc, &lb)))
                g_rt->DrawTextLayout(D2D1::Point2F(x, y), lay, lb);
            if (lb) lb->Release(); if (sc) sc->Release();
        }
    }
    lay->Release();
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

    ++g_frames;
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
    if (g_cfg.clock) DrawClock(); // WallClock: camada de baixo
    float r = ((g_cfg.color >> 16) & 0xFF) / 255.0f, g = ((g_cfg.color >> 8) & 0xFF) / 255.0f, b = (g_cfg.color & 0xFF) / 255.0f;
    ID2D1SolidColorBrush* brush = nullptr; ID2D1SolidColorBrush* head = nullptr;
    if (g_cfg.enabled) { // chuva: camada do meio
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
    }
    if (brush) brush->Release(); if (head) head->Release();
    HRESULT hr = g_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) { ReleaseWall(); g_rt->Release(); g_rt = nullptr; }
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
            case WM_SETTINGCHANGE: if (wp == SPI_SETDESKWALLPAPER && g_rt) LoadWallpaper(); DrawFrame(); return 0; // LoadWallpaper também descarta o desfoque
            case WM_DISPLAYCHANGE: case WM_SIZE:
                ReleaseWall();
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
    g_timerId = SetTimer(g_rainWnd, 1, g_cfg.enabled ? 1000 / std::max(1, g_cfg.fps) : 1000, nullptr); // só o relógio: 1 quadro por segundo
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
    ReleaseWall();
    if (g_rt) { g_rt->Release(); g_rt = nullptr; }
    if (g_font) { g_font->Release(); g_font = nullptr; }
    if (g_clockFmt) { g_clockFmt->Release(); g_clockFmt = nullptr; }
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

// Roda com a chuva ou o WallClock ligado, em qualquer tema (WallEngine, v19.8).
inline bool ShouldRun(int currentTheme) {
    return g_cfg.enabled || g_cfg.clock;
}

} // namespace rain