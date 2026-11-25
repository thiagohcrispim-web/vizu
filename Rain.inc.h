// Rain.inc.h - Chuva digital para o tema Vizu Classic
// WorkerW layer entre papel de parede e ícones, loop perfeito, hash determinístico
#pragma once
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <vector>
#include <string>
#include <random>
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

// Encontra WorkerW correta (Win 11 23H2: irmã do SHELLDLL_DefView; 24H2+: filha do Progman)
inline HWND FindWorkerW() {
    HWND progman = FindWindowW(L"Progman", nullptr);
    if (!progman) return nullptr;

    // Envia mensagem para Progman criar WorkerW se necessário (0x052C)
    SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);

    HWND workerW = nullptr;
    // Procura WorkerW que é irmã do SHELLDLL_DefView (23H2) ou filha do Progman (24H2+)
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        HWND* pWorker = (HWND*)lParam;
        wchar_t className[256];
        GetClassNameW(hwnd, className, 256);
        if (wcscmp(className, L"WorkerW") == 0) {
            // Verifica se tem SHELLDLL_DefView como filha (23H2) ou é filha do Progman (24H2+)
            HWND defView = FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
            HWND parent = GetParent(hwnd);
            HWND progman = FindWindowW(L"Progman", nullptr);
            if (defView || parent == progman) {
                *pWorker = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }, (LPARAM)&workerW);

    return workerW;
}

// Cria janela de chuva como filha da WorkerW (atrás dos ícones, na frente do papel de parede)
inline bool CreateRainWindow() {
    g_workerW = FindWorkerW();
    if (!g_workerW) return false;

    RECT rc;
    GetClientRect(g_workerW, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = [](HWND h, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
        switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(h, &ps);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1; // não apagar fundo, nós desenhamos tudo
        case WM_TIMER:
            if (wp == g_timerId) {
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        case WM_DISPLAYCHANGE:
        case WM_SIZE:
            // Recriar render target no próximo frame
            if (g_rt) { g_rt->Release(); g_rt = nullptr; }
            return 0;
        case WM_DESTROY:
            if (g_timerId) KillTimer(h, g_timerId);
            return 0;
        }
        return DefWindowProcW(h, msg, wp, lp);
    };
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"VizuRain";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE
    // Para ficar atrás dos ícones mas receber paint
    g_rainWnd = CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        L"VizuRain", L"", WS_CHILD | WS_VISIBLE,
        0, 0, w, h, g_workerW, nullptr, GetModuleHandleW(nullptr), nullptr
    );

    if (!g_rainWnd) return false;

    // Layered com alpha per-pixel
    SetLayeredWindowAttributes(g_rainWnd, 0, 255, LWA_ALPHA);

    // Criar render target D2D
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96, 96
    );
    if (FAILED(g_d2d->CreateHwndRenderTarget(props,
        D2D1::HwndRenderTargetProperties(g_rainWnd, D2D1::SizeU(w, h)), &g_rt))) {
        return false;
    }
    g_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    // Fonte monoespaçada para chuva
    if (FAILED(g_dw->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &g_font))) {
        return false;
    }
    g_font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    g_font->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    // Medir caractere
    DWRITE_TEXT_METRICS tm;
    IDWriteTextLayout* layout = nullptr;
    if (SUCCEEDED(g_dw->CreateTextLayout(L"ア", 1, g_font, 100, 100, &layout))) {
        layout->GetMetrics(&tm);
        g_charW = tm.width;
        g_charH = tm.height;
        layout->Release();
    }

    // Inicializar colunas
    g_cols = (int)(w / g_charW) + 2;
    g_rows = (int)(h / g_charH) + 2;
    g_colSpeeds.assign(g_cols, 0);
    g_colOffsets.assign(g_cols, 0);
    g_colLengths.assign(g_cols, 0);

    // Seed determinístico baseado em resolução + config
    g_seed = 0xC0FFEE ^ (w * 31) ^ (h * 17) ^ (g_cfg.cycleMs / 1000);

    for (int i = 0; i < g_cols; ++i) {
        g_colSpeeds[i] = 0.5f + (Rand() % 1000) / 2000.0f; // 0.5-1.0 células/frame
        g_colOffsets[i] = (Rand() % 1000) / 1000.0f * g_cfg.cycleMs / 16.67f; // offset no ciclo
        g_colLengths[i] = 5 + (Rand() % (g_rows / 2)); // comprimento variável
    }

    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_startTime);

    // Timer para animação (~16ms para 60fps, mas respeita FPS config)
    int interval = 1000 / g_cfg.fps;
    g_timerId = SetTimer(g_rainWnd, 1, interval, nullptr);

    g_running = true;
    return true;
}

// Desenha um frame da chuva
inline void DrawFrame() {
    if (!g_rt || !g_running) return;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float elapsedMs = (float)((now.QuadPart - g_startTime.QuadPart) * 1000.0 / g_freq.QuadPart);

    // Loop perfeito: fase = t/ciclo, k inteiro (voltas por ciclo)
    float cycleProgress = fmodf(elapsedMs, (float)g_cfg.cycleMs) / g_cfg.cycleMs; // 0..1

    g_rt->BeginDraw();
    g_rt->Clear(D2D1::ColorF(0, 0)); // transparente

    // Cor base
    float r = ((g_cfg.color >> 16) & 0xFF) / 255.0f;
    float g = ((g_cfg.color >> 8) & 0xFF) / 255.0f;
    float b = (g_cfg.color & 0xFF) / 255.0f;

    ID2D1SolidColorBrush* brush = nullptr;
    g_rt->CreateSolidColorBrush(D2D1::ColorF(r, g, b, g_cfg.opacity), &brush);

    // Desenhar cada coluna
    for (int col = 0; col < g_cols; ++col) {
        float x = col * g_charW;
        float speed = g_colSpeeds[col];
        float offset = g_colOffsets[col];
        int length = g_colLengths[col];

        // Posição da cabeça no ciclo (0..rows)
        float headPos = fmodf(cycleProgress * g_rows + offset, (float)g_rows + length);
        int headRow = (int)headPos;

        // Desenhar caracteres da coluna (da cabeça para cima)
        for (int i = 0; i < length; ++i) {
            int row = headRow - i;
            if (row < 0 || row >= g_rows) continue;

            float y = row * g_charH;
            float progress = (float)i / length;
            float alpha = 1.0f - progress; // fade da cabeça para a cauda

            // Cabeça mais brilhante
            if (i < (int)(length * g_cfg.headRatio)) {
                alpha = 1.0f;
            }

            // Pular opacidade < 0.2% (otimização)
            if (alpha * g_cfg.opacity < 0.002f) continue;

            // Hash determinístico para caractere (baseado em col, row, frame)
            uint32_t charSeed = (col * 73856093) ^ (row * 19349663) ^ ((int)(elapsedMs / 16.67f) * 83492791);
            charSeed ^= charSeed << 13;
            charSeed ^= charSeed >> 17;
            charSeed ^= charSeed << 5;
            UINT ch = g_chars[charSeed % g_chars.size()];

            wchar_t wc = (wchar_t)ch;
            D2D1_RECT_F layoutRect = D2D1::RectF(x, y, x + g_charW, y + g_charH);
            brush->SetOpacity(alpha * g_cfg.opacity);
            g_rt->DrawTextW(&wc, 1, g_font, layoutRect, brush);
        }
    }

    if (brush) brush->Release();

    HRESULT hr = g_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        g_rt->Release();
        g_rt = nullptr;
        // Recriar no próximo frame
    }
}

// Pausar/retomar baseado em condições (tela cheia, maximizada, bateria, tela bloqueada, animações desligadas)
inline bool ShouldPause() {
    // Tela cheia / janela maximizada
    HWND fg = GetForegroundWindow();
    if (fg) {
        RECT rc;
        GetWindowRect(fg, &rc);
        MONITORINFO mi = { sizeof(mi) };
        HMONITOR mon = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
        GetMonitorInfo(mon, &mi);
        RECT work = mi.rcWork;
        if (rc.left <= work.left && rc.right >= work.right && rc.top <= work.top && rc.bottom >= work.bottom) {
            return true;
        }
    }

    // Bateria baixa
    SYSTEM_POWER_STATUS ps;
    if (GetSystemPowerStatus(&ps) && ps.BatteryFlag & 128) { // 128 = bateria baixa
        return true;
    }

    // Tela bloqueada
    if (GetForegroundWindow() == nullptr) return true;

    // "Mostrar animações no Windows" desligado
    BOOL animOn = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animOn, 0);
    if (!animOn) return true;

    return false;
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
    return currentTheme == ui::kThemeClassic && g_cfg.enabled;
}

} // namespace rain