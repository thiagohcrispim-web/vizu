// Ui.h - visual "vidro" inspirado no visionOS: tema do Windows, desfoque acrylic e desenho em Direct2D.
#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <map>
#include <vector>
#include "Feather.h"
#include "Fonts.h"

namespace ui {

// ---------- tema ----------

enum { kThemeLight = 0, kThemeDark, kThemeWindows, kThemeCrispy, kThemeClassic }; // Claro, Escuro, Windows, Vizu Crispy, Vizu Classic

struct Theme {
    bool dark = true, glass = false, neon = false; // neon: brilho laranja (tema Crispy)
    const wchar_t* font = L"Bahnschrift";
    D2D1_COLOR_F tint;        // fundo da janela (sobre o desfoque)
    D2D1_COLOR_F panel;       // cartões / cápsulas
    D2D1_COLOR_F panelHot;    // cartão sob o mouse
    D2D1_COLOR_F stroke;      // borda de luz
    D2D1_COLOR_F text, text2; // texto principal / secundário
    D2D1_COLOR_F accent, onAccent, danger, track, knob, knobOn;
    D2D1_COLOR_F segFill, segText; // item selecionado do controle segmentado
    D2D1_COLOR_F icon;             // cor dos ícones
    bool classic = false;          // Vizu Classic: chapado, laranja só como preenchimento
    UINT glow = 0xFFFFFF;     // cor do brilho que segue o mouse no anel
    float glowA = 0.10f;
};

inline D2D1_COLOR_F Rgba(UINT rgb, float a = 1) { return D2D1::ColorF(rgb, a); }

// Cor do neon do tema Crispy (escolhida pelo usuário).
inline UINT& NeonColor() { static UINT c = 0xFF6633; return c; }

inline bool SystemDark() {
    DWORD v = 1, sz = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme",
                 RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v == 0;
}

// Cor de destaque do Windows (Configurações > Personalização > Cores).
inline UINT SystemAccent() {
    DWORD v = 0, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS)
        return (v & 0xFF) << 16 | (v & 0xFF00) | (v >> 16 & 0xFF); // ABGR -> RGB
    return 0x0067C0;
}

// Crispy V2: grafite em vidro, cartões em degradê de 115°, borda degradê branco -> neon e neon em duas camadas.
// Cor dominante #FF6633 (ou a "Cor do neon" escolhida pelo usuário). Fonte Bahnschrift.
inline Theme CrispyTheme(bool glass) {
    Theme t;
    t.dark = true; t.glass = glass; t.neon = true;
    t.font = L"Bahnschrift";
    t.tint = glass ? Rgba(0x101013, 0.55f) : Rgba(0x08080A); // vidro: grafite ~55% sobre o acrílico do DWM
    t.panel = Rgba(0x16171B, 0.92f); t.panelHot = Rgba(NeonColor(), 0.16f); t.stroke = Rgba(0x232326);
    t.text = Rgba(0xF2F2F2); t.text2 = Rgba(0xA8A8AF);
    t.accent = Rgba(NeonColor()); t.onAccent = Rgba(0x0E0E10); t.danger = Rgba(0xFF3B5C);
    t.track = Rgba(0x141416); t.knob = Rgba(0xF2F2F2); t.knobOn = Rgba(0xF2F2F2);
    t.glow = NeonColor(); t.glowA = 0.40f;
    t.segFill = Rgba(NeonColor(), 0.18f); t.segText = Rgba(NeonColor()); t.icon = t.text;
    return t;
}

// Vizu Classic: o Design System Vizu (grafite e Ardósia, laranja #FF6633 só como preenchimento, sem brilhos).
inline Theme ClassicTheme() {
    Theme t;
    t.dark = true; t.glass = false; t.classic = true;
    t.tint = Rgba(0x2E333B);
    t.panel = Rgba(0x4A4F57); t.panelHot = Rgba(0x5C6472); t.stroke = Rgba(0x5C6472);
    t.text = Rgba(0xFFFFFF); t.text2 = Rgba(0xBCC2CC);
    t.accent = Rgba(0xFF6633); t.onAccent = Rgba(0x000000); t.danger = Rgba(0xFF6961);
    t.track = Rgba(0x5C6472); t.knob = Rgba(0xF2F2F2); t.knobOn = Rgba(0xF2F2F2);
    t.segFill = Rgba(0xF2F2F2); t.segText = Rgba(0x1E2126); t.icon = Rgba(0xFFFFFF);
    t.glow = 0xFFFFFF; t.glowA = 0.05f;
    return t;
}

inline Theme MakeTheme(int mode, bool glass) {
    if (mode == kThemeCrispy) return CrispyTheme(glass);
    if (mode == kThemeClassic) return ClassicTheme();
    Theme t;
    bool dark = mode == kThemeDark || (mode == kThemeWindows && SystemDark());
    t.dark = dark; t.glass = glass;
    if (mode == kThemeDark) { // preto x chumbo, sem cor
        // Escuro do canvas: preto (#0F0F11), cinza-claro para o que fica ligado, sem cor.
        t.tint = glass ? Rgba(0x0F0F11, 0.80f) : Rgba(0x0F0F11);
        t.panel = Rgba(0xFFFFFF, 0.05f); t.panelHot = Rgba(0xFFFFFF, 0.10f); t.stroke = Rgba(0x2A2A2E);
        t.text = Rgba(0xEDEDEF); t.text2 = Rgba(0xA1A1A8);
        t.accent = Rgba(0xEDEDEF); t.onAccent = Rgba(0x0F0F11); t.danger = Rgba(0xFF6961);
        // Ligado: trilha clara com knob escuro (com knob claro sobre trilha clara o interruptor some).
        t.track = Rgba(0x2A2A2E); t.knob = Rgba(0xF2F2F2); t.knobOn = Rgba(0x0F0F11);
        t.segFill = Rgba(0xEDEDEF); t.segText = Rgba(0x0F0F11); t.icon = Rgba(0x9AA1AD);
        t.glow = 0xFFFFFF; t.glowA = 0.10f;
    } else if (dark) { // Padrão Windows, escuro
        t.tint = glass ? Rgba(0x1C1C1E, 0.62f) : Rgba(0x1C1C1E);
        t.panel = Rgba(0xFFFFFF, 0.08f); t.panelHot = Rgba(0xFFFFFF, 0.14f); t.stroke = Rgba(0xFFFFFF, 0.14f);
        t.text = Rgba(0xFFFFFF, 0.96f); t.text2 = Rgba(0xFFFFFF, 0.58f);
        t.accent = Rgba(SystemAccent()); t.onAccent = Rgba(0xFFFFFF); t.danger = Rgba(0xFF6961);
        t.track = Rgba(0xFFFFFF, 0.18f); t.knob = Rgba(0xFFFFFF); t.knobOn = Rgba(0xFFFFFF);
        t.segFill = Rgba(0xFFFFFF, 0.14f); t.segText = t.text; t.icon = t.text;
        t.glow = SystemAccent(); t.glowA = 0.22f;
    } else { // claro: minimalista (grafite) ou Padrão Windows (cor de destaque do sistema)
        t.tint = glass ? Rgba(0xF5F5F7, 0.70f) : Rgba(0xF5F5F7);
        t.panel = Rgba(0xFFFFFF, 0.62f); t.panelHot = Rgba(0xFFFFFF, 0.88f); t.stroke = Rgba(0x000000, 0.07f);
        t.text = Rgba(0x1D1D1F); t.text2 = Rgba(0x000000, 0.52f);
        bool win = mode == kThemeWindows;
        t.accent = win ? Rgba(SystemAccent()) : Rgba(0x1D1D1F); t.onAccent = Rgba(0xFFFFFF); t.danger = Rgba(0xD70015);
        t.track = Rgba(0x000000, 0.10f); t.knob = Rgba(0xFFFFFF); t.knobOn = Rgba(0xFFFFFF);
        t.glow = win ? SystemAccent() : 0xFFFFFF; t.glowA = win ? 0.16f : 0.30f;
        t.segFill = Rgba(0xFFFFFF); t.segText = t.text; t.icon = t.text;
        if (!win) { // Claro do canvas: branco, preto e laranja só nos interruptores e seleções
            t.tint = Rgba(0xFFFFFF); t.stroke = Rgba(0xD5D9DF);
            t.text = Rgba(0x000000); t.text2 = Rgba(0x4A4F57);
            t.accent = Rgba(0xFF6633); t.onAccent = Rgba(0x000000);
            t.track = Rgba(0xD5D9DF); t.knob = Rgba(0xF2F2F2); t.knobOn = Rgba(0xF2F2F2);
            t.segFill = Rgba(0x000000); t.segText = Rgba(0xFFFFFF); t.icon = Rgba(0x000000);
        }
    }
    return t;
}

// Fundo de vidro: halos suaves de luz (no Crispy, degradê neon laranja sobre preto).
inline void Backdrop(ID2D1RenderTarget* rt, const Theme& th, float w, float h) {
    if (th.classic) return; // Vizu Classic: fundo chapado
    struct B { float x, y, r; UINT c; float a; };
    if (th.neon) { // Crispy V2: mancha laranja em cima à esquerda e azulada embaixo à direita (mais fortes sem o vidro do DWM)
        float m = (w > h ? w : h), k = th.glass ? 0.6f : 1.0f;
        B cb[2] = {{w * 0.08f, h * 0.02f, m * 0.60f, NeonColor(), 0.20f * k}, {w * 0.95f, h * 1.0f, m * 0.55f, 0x3A5BFF, 0.12f * k}};
        ID2D1SolidColorBrush* b = nullptr;
        for (auto& x : cb) for (int i = 24; i >= 1; --i) {
            float r = x.r * i / 24;
            if (SUCCEEDED(rt->CreateSolidColorBrush(Rgba(x.c, x.a / 24 * 1.6f), &b))) { rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x.x, x.y), r, r), b); b->Release(); }
        }
        return;
    }
    B bl[3] = {{w * 0.05f, h * 0.0f, (w > h ? w : h) * 0.55f, th.neon ? NeonColor() : th.dark ? 0x5A5A62u : 0xFFFFFFu, th.neon ? 0.22f : th.dark ? 0.16f : 0.55f},
               {w * 1.0f, h * 1.05f, (w > h ? w : h) * 0.55f, th.neon ? NeonColor() : th.dark ? 0x3A3A40u : 0xD0D4DCu, th.neon ? 0.05f : th.dark ? 0.14f : 0.35f},
               {w * 0.85f, h * 0.1f, (w > h ? w : h) * 0.30f, th.neon ? NeonColor() : th.glow, th.neon ? 0.10f : th.dark ? 0.04f : 0.10f}};
    ID2D1SolidColorBrush* b = nullptr;
    for (auto& x : bl) {
        const int steps = 24; // anéis concêntricos com pouca opacidade = degradê radial suave
        for (int i = steps; i >= 1; --i) {
            float r = x.r * i / steps;
            if (SUCCEEDED(rt->CreateSolidColorBrush(Rgba(x.c, x.a / steps * 1.6f), &b))) { rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x.x, x.y), r, r), b); b->Release(); }
        }
    }
}

// Desfoque "acrylic" atrás da janela (funciona mesmo com a janela inativa). Retorna false se indisponível.
inline bool EnableGlass(HWND h, bool dark) {
    MARGINS m = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(h, &m);
    DWORD corner = 2; // DWMWCP_ROUND (Windows 11)
    DwmSetWindowAttribute(h, 33, &corner, sizeof(corner));
    BOOL d = dark;
    DwmSetWindowAttribute(h, 20, &d, sizeof(d));
    struct AccentPolicy { int state, flags; DWORD color; int anim; };
    struct CompositionData { int attrib; void* data; SIZE_T size; };
    typedef BOOL(WINAPI * SetWca)(HWND, CompositionData*);
    static SetWca set = (SetWca)(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");
    DWORD backdrop = 3; // DWMSBT_TRANSIENTWINDOW (acrílico do Windows 11 22H2+); ignorado nas versões antigas
    DwmSetWindowAttribute(h, 38, &backdrop, sizeof(backdrop));
    if (!set) return false;
    AccentPolicy ap = {4 /* ACCENT_ENABLE_ACRYLICBLURBEHIND */, 0, 0x01000000, 0};
    CompositionData cd = {19 /* WCA_ACCENT_POLICY */, &ap, sizeof(ap)};
    return set(h, &cd) != FALSE;
}


// ---------- movimento ----------

// "Mostrar animações no Windows" desligado = reduzir movimento.
inline bool ReducedMotion() { BOOL on = TRUE; SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0); return !on; }

// cubic-bezier(x1, y1, x2, y2) como no CSS.
inline float CubicBezier(float x1, float y1, float x2, float y2, float x) {
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    auto bx = [&](float t) { float u = 1 - t; return 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t; };
    auto by = [&](float t) { float u = 1 - t; return 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t; };
    float lo = 0, hi = 1, t = x;
    for (int i = 0; i < 24; ++i) { float v = bx(t); if (fabsf(v - x) < 1e-4f) break; if (v < x) lo = t; else hi = t; t = (lo + hi) / 2; }
    return by(t);
}
inline float EaseOut(float t) { return CubicBezier(0.23f, 1, 0.32f, 1, t); }   // entradas
inline float EaseInOut(float t) { return CubicBezier(0.77f, 0, 0.175f, 1, t); } // o que já está na tela

inline double NowMs() {
    static LARGE_INTEGER f = {}; if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return c.QuadPart * 1000.0 / f.QuadPart;
}

// Valores que deslizam até o alvo. Cada janela tem um; Frame() no começo do desenho, Pump() no fim.
// Nada anima sozinho: um valor só se move quando o alvo muda (ação ou mouse).
struct Anim {
    struct V { float from, to, cur; double start; float dur; bool inout; };
    std::map<int, V> m;
    double now = 0;
    bool busy = false, reduced = false;
    void Frame() { now = NowMs(); busy = false; reduced = ReducedMotion(); }
    // motion: posição/escala (some no "reduzir movimento"); senão opacidade/cor (no máx. 150 ms nesse modo).
    float Get(int key, float target, float dur = 220, bool inout = false, bool motion = false) {
        auto it = m.find(key);
        if (it == m.end()) { m[key] = {target, target, target, now, 0, inout}; return target; }
        V& v = it->second;
        if (v.to != target) {
            v.from = v.cur; v.to = target; v.start = now; v.inout = inout;
            v.dur = reduced ? (motion ? 0 : (dur < 150 ? dur : 150)) : dur;
        }
        if (v.dur <= 0) return v.cur = v.to;
        float k = (float)((now - v.start) / v.dur);
        if (k >= 1) return v.cur = v.to;
        if (k < 0) k = 0;
        busy = true;
        return v.cur = v.from + (v.to - v.from) * (v.inout ? EaseInOut(k) : EaseOut(k));
    }
    // Começa do valor "from" (entrada de janela/aviso).
    void Start(int key, float from, float to, float dur, bool motion = false) {
        float d = reduced || ReducedMotion() ? (motion ? 0 : (dur < 150 ? dur : 150)) : dur;
        m[key] = {from, to, from, NowMs(), d, false};
    }
    // Ainda animando: pede outro quadro em ~16 ms. Janela oculta ou minimizada não gasta nada.
    void Pump(HWND h, UINT_PTR timer) {
        if (busy && IsWindowVisible(h) && !IsIconic(h)) SetTimer(h, timer, 16, nullptr);
        else KillTimer(h, timer);
    }
};
enum : UINT_PTR { kAnimTick = 0x7A11 }; // id de timer reservado para Anim::Pump em todas as janelas

inline D2D1_COLOR_F Mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float k) {
    return D2D1::ColorF(a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k, a.a + (b.a - a.a) * k);
}

// ---------- superfície Direct2D com transparência ----------

struct Surface {
    HWND hwnd = nullptr;
    ID2D1HwndRenderTarget* rt = nullptr;
    bool Begin(ID2D1Factory* f) {
        if (!rt) {
            RECT rc; GetClientRect(hwnd, &rc);
            D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96); // trabalha em pixels
            if (FAILED(f->CreateHwndRenderTarget(p, D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(rc.right, rc.bottom)), &rt))) return false;
            rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE); // ClearType não funciona sobre transparência
        }
        rt->BeginDraw();
        return true;
    }
    bool End() { // false: recurso perdido, recriar bitmaps
        if (rt->EndDraw() == D2DERR_RECREATE_TARGET) { rt->Release(); rt = nullptr; return false; }
        return true;
    }
    void Resize(UINT w, UINT h) { if (rt) rt->Resize(D2D1::SizeU(w, h)); }
    void Release() { if (rt) { rt->Release(); rt = nullptr; } }
};

// Caminho SVG (subconjunto: M L H V C A Z, absolutos e relativos) -> geometria Direct2D, escalado e deslocado.
inline ID2D1PathGeometry* SvgPath(ID2D1Factory* f, const char* d, float sc, float ox, float oy, bool filled = false) {
    ID2D1PathGeometry* g = nullptr; ID2D1GeometrySink* k = nullptr;
    if (FAILED(f->CreatePathGeometry(&g)) || FAILED(g->Open(&k))) { if (g) g->Release(); return nullptr; }
    const char* p = d;
    char cmd = 0;
    float cx = 0, cy = 0, sx = 0, sy = 0, lcx = 0, lcy = 0; // lcx/lcy: último ponto de controle (para 's')
    bool open = false; char lastCmd = 0;
    auto P = [&](float x, float y) { return D2D1::Point2F(ox + x * sc, oy + y * sc); };
    auto skip = [&]() { while (*p == ' ' || *p == ',' || *p == '\n') ++p; };
    auto num = [&]() { skip(); char* e; float v = strtof(p, &e); p = e; return v; };
    auto hasNum = [&]() { skip(); return (*p >= '0' && *p <= '9') || *p == '-' || *p == '.' || *p == '+'; };
    auto begin = [&](float x, float y) { if (open) k->EndFigure(D2D1_FIGURE_END_OPEN); k->BeginFigure(P(x, y), filled ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW); open = true; sx = x; sy = y; };
    while (true) {
        skip();
        if (!*p) break;
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) cmd = *p++;
        bool rel = cmd >= 'a';
        float bx = rel ? cx : 0, by = rel ? cy : 0;
        switch (cmd | 32) {
        case 'm': { float x = num() + bx, y = num() + by; begin(x, y); cx = x; cy = y; cmd = rel ? 'l' : 'L'; break; }
        case 'l': { float x = num() + bx, y = num() + by; k->AddLine(P(x, y)); cx = x; cy = y; break; }
        case 'h': { float x = num() + bx; k->AddLine(P(x, cy)); cx = x; break; }
        case 'v': { float y = num() + by; k->AddLine(P(cx, y)); cy = y; break; }
        case 'c': {
            float x1 = num() + bx, y1 = num() + by, x2 = num() + bx, y2 = num() + by, x = num() + bx, y = num() + by;
            k->AddBezier(D2D1::BezierSegment(P(x1, y1), P(x2, y2), P(x, y))); lcx = x2; lcy = y2; cx = x; cy = y; break;
        }
        case 's': {
            bool prevCubic = (lastCmd | 32) == 'c' || (lastCmd | 32) == 's';
            float x1 = prevCubic ? 2 * cx - lcx : cx, y1 = prevCubic ? 2 * cy - lcy : cy;
            float x2 = num() + bx, y2 = num() + by, x = num() + bx, y = num() + by;
            k->AddBezier(D2D1::BezierSegment(P(x1, y1), P(x2, y2), P(x, y))); lcx = x2; lcy = y2; cx = x; cy = y; break;
        }
        case 'a': {
            float rx = num(), ry = num(), rot = num(), large = num(), sweep = num(), x = num() + bx, y = num() + by;
            if (rx <= 0 || ry <= 0 || (x == cx && y == cy)) { k->AddLine(P(x, y)); cx = x; cy = y; break; }
            { // arco SVG -> curvas Bézier (o mesmo resultado em qualquer renderizador)
                const float PI = 3.14159265f;
                float ph = rot * PI / 180, co = cosf(ph), si = sinf(ph);
                float dx2 = (cx - x) / 2, dy2 = (cy - y) / 2, x1p = co * dx2 + si * dy2, y1p = -si * dx2 + co * dy2;
                rx = fabsf(rx); ry = fabsf(ry);
                float lam = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry);
                if (lam > 1) { float q = sqrtf(lam); rx *= q; ry *= q; }
                float num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p, den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
                float coef = (large != sweep ? 1.0f : -1.0f) * sqrtf(fmaxf(0.0f, den > 0 ? num / den : 0));
                float cxp = coef * rx * y1p / ry, cyp = -coef * ry * x1p / rx;
                float ecx = co * cxp - si * cyp + (cx + x) / 2, ecy = si * cxp + co * cyp + (cy + y) / 2;
                auto ang = [](float ux, float uy, float vx, float vy) { return atan2f(ux * vy - uy * vx, ux * vx + uy * vy); };
                float t1 = ang(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry), dt = ang((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);
                if (sweep == 0 && dt > 0) dt -= 2 * PI; else if (sweep != 0 && dt < 0) dt += 2 * PI;
                int n = (int)ceilf(fabsf(dt) / (PI / 2) - 0.001f); if (n < 1) n = 1;
                float d = dt / n, tk = 4.0f / 3 * tanf(d / 4);
                auto E = [&](float t, float* px, float* py) { *px = ecx + rx * co * cosf(t) - ry * si * sinf(t); *py = ecy + rx * si * cosf(t) + ry * co * sinf(t); };
                auto D = [&](float t, float* px, float* py) { *px = -rx * co * sinf(t) - ry * si * cosf(t); *py = -rx * si * sinf(t) + ry * co * cosf(t); };
                for (int i = 0; i < n; ++i) {
                    float a0 = t1 + i * d, a1 = a0 + d, ax, ay, bx2, by2, dax, day, dbx, dby;
                    E(a0, &ax, &ay); E(a1, &bx2, &by2); D(a0, &dax, &day); D(a1, &dbx, &dby);
                    if (i == n - 1) { bx2 = x; by2 = y; }
                    k->AddBezier(D2D1::BezierSegment(P(ax + tk * dax, ay + tk * day), P(bx2 - tk * dbx, by2 - tk * dby), P(bx2, by2)));
                }
            }
            cx = x; cy = y; break;
        }
        case 'z': if (open) { k->EndFigure(D2D1_FIGURE_END_CLOSED); open = false; } cx = sx; cy = sy; break;
        default: ++p; continue;
        }
        lastCmd = cmd;
        if ((cmd | 32) == 'z') continue;
        if (!hasNum() && *p && !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) ++p;
    }
    if (open) k->EndFigure(D2D1_FIGURE_END_OPEN);
    k->Close(); k->Release();
    return g;
}

// ---------- pintor ----------

// Shared official Vizu wordmark used by settings and installer.
inline void Brand(ID2D1RenderTarget* rt, const Theme& th, float x, float y, float height) {
    static const char* kWord[] = {
            "M-121.57-2069.57H102.46l256.87,572.16,256.88-572.16h224.03l-349.54,744.23H227.96l-349.54-744.23Z",
            "M887.14-1989.34c0-57.01,44.57-88.67,123.16-88.67s121.99,31.67,121.99,88.67-42.23,88.67-121.99,88.67-123.16-31.67-123.16-88.67ZM902.39-1871.11h215.82v545.77h-215.82v-545.77Z",
            "M1179.19-1455.18l364.79-289.25h-360.09v-125.62h609.93v125.62l-367.13,294.53h373v124.57h-620.49v-129.84Z",
            "M2577.32-1325.34h-215.82v-205.85c-10.56,80.23-72.72,210.07-235.76,216.41-168.9,6.33-279.16-100.29-279.16-235.41v-319.86h215.82v292.42c0,90.79,41.05,137.23,98.53,137.23,111.43,0,200.57-164.68,200.57-307.19v-122.46h215.82v544.71Z",
            "M2667.76-1485.87c0-102.03,71.8-158.72,198.4-158.72s196.51,56.68,196.51,158.72-68.02,158.72-196.51,158.72-198.4-56.69-198.4-158.72Z"};
    float sc = height / 830.0f, ox = x + 160 * sc, oy = y + 2120 * sc;
    ID2D1Factory* fac = nullptr; rt->GetFactory(&fac);
    for (int i = 0; i < 5; ++i) {
        ID2D1PathGeometry* g = SvgPath(fac, kWord[i], sc, ox, oy, true);
        ID2D1SolidColorBrush* b = nullptr;
        if (g && SUCCEEDED(rt->CreateSolidColorBrush(i == 4 ? Rgba(0xFF6633) : th.text, &b))) { rt->FillGeometry(g, b); b->Release(); }
        if (g) g->Release();
    }
    fac->Release();
}

class Painter {
public:
    ID2D1RenderTarget* rt;
    IDWriteFactory* dw;
    const Theme& th;
    float s; // escala de DPI
    Anim* an = nullptr;               // valores animados da janela (nulo: sem animação)
    float mx = -1e6f, my = -1e6f;     // mouse em coordenadas do cliente (luz que segue o cursor)
    bool down = false;                // botão do mouse apertado (efeito de "afundar")
    Painter(ID2D1RenderTarget* r, IDWriteFactory* d, const Theme& t, float scale) : rt(r), dw(d), th(t), s(scale) {}
    // Valor animado (sem Anim, vai direto ao alvo).
    float A(int key, float target, float dur = 220, bool inout = false, bool motion = false) { return an && key ? an->Get(key, target, dur, inout, motion) : target; }
    bool Over(const D2D1_RECT_F& r) const { return mx >= r.left && mx < r.right && my >= r.top && my < r.bottom; }

    static const wchar_t* IconFont() {
        static const wchar_t* name = nullptr;
        if (!name) {
            name = L"Segoe MDL2 Assets";
            HDC dc = GetDC(nullptr);
            LOGFONTW lf = {}; lf.lfCharSet = DEFAULT_CHARSET; wcscpy_s(lf.lfFaceName, L"Segoe Fluent Icons");
            EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM p) -> int { *(const wchar_t**)p = L"Segoe Fluent Icons"; return 0; }, (LPARAM)&name, 0);
            ReleaseDC(nullptr, dc);
        }
        return name;
    }

    void Fill(const D2D1_RECT_F& r, float radius, D2D1_COLOR_F c) {
        ID2D1SolidColorBrush* b = nullptr;
        if (SUCCEEDED(rt->CreateSolidColorBrush(c, &b))) { rt->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), b); b->Release(); }
    }
    void Stroke(const D2D1_RECT_F& r, float radius, D2D1_COLOR_F c, float w = 1) {
        ID2D1SolidColorBrush* b = nullptr;
        D2D1_RECT_F in = D2D1::RectF(r.left + w / 2, r.top + w / 2, r.right - w / 2, r.bottom - w / 2);
        if (SUCCEEDED(rt->CreateSolidColorBrush(c, &b))) { rt->DrawRoundedRectangle(D2D1::RoundedRect(in, radius, radius), b, w); b->Release(); }
    }
    // Cartão de vidro: preenchimento translúcido + borda de luz. No Crispy, o cartão V2 (ver Card).
    void Glass(const D2D1_RECT_F& r, float radius, bool hot = false, int key = 0) {
        if (th.neon) { Card(r, radius, key); return; }
        Fill(r, radius, hot ? th.panelHot : th.panel);
        Stroke(r, radius, th.stroke);
    }
    // Degradê linear em um ângulo CSS (0° = para cima, 90° = para a direita) cobrindo o retângulo.
    ID2D1LinearGradientBrush* Angle(const D2D1_RECT_F& r, float deg, const D2D1_GRADIENT_STOP* st, UINT n) {
        float a = deg * 3.14159265f / 180, dx = sinf(a), dy = -cosf(a);
        float w = r.right - r.left, h = r.bottom - r.top, half = (fabsf(w * dx) + fabsf(h * dy)) / 2;
        float cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
        ID2D1GradientStopCollection* c = nullptr; ID2D1LinearGradientBrush* b = nullptr;
        if (FAILED(rt->CreateGradientStopCollection(st, n, &c))) return nullptr;
        rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(cx - dx * half, cy - dy * half), D2D1::Point2F(cx + dx * half, cy + dy * half)), c, &b);
        c->Release();
        return b;
    }
    void FillBrush(const D2D1_RECT_F& r, float radius, ID2D1Brush* b) { if (b) rt->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), b); }
    void StrokeBrush(const D2D1_RECT_F& r, float radius, ID2D1Brush* b, float w) {
        if (b) rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(r.left + w / 2, r.top + w / 2, r.right - w / 2, r.bottom - w / 2), radius, radius), b, w);
    }
    // Cartão Crispy V2: degradê de 115°, borda em degradê, filete no topo e luz que segue o mouse (só dentro do cartão).
    // Sem brilho nem granulado atrás: a separação vem do preenchimento e da borda. key != 0 anima a luz (apaga em ~250 ms).
    void Card(const D2D1_RECT_F& r, float radius, int key = 0) {
        if (!th.neon) { Fill(r, radius, th.panel); Stroke(r, radius, th.stroke); return; }
        bool over = Over(r);
        float hov = A(key ? key * 4 + 1 : 0, over ? 1.0f : 0.0f, over ? 300.0f : 250.0f);
        Fill(r, radius, Rgba(0x16171B));
        D2D1_GRADIENT_STOP fs[4] = {{0, Rgba(0x0A0B0E, 0.94f)}, {0.45f, Rgba(0x121318, 0.94f)}, {0.75f, Rgba(0x16171B, 0.92f)}, {1, Rgba(0x1E2028, 0.90f)}};
        if (auto* b = Angle(r, 115, fs, 4)) { FillBrush(r, radius, b); b->Release(); }
        UINT n = NeonColor(), hi = 0xFF8A5C;
        if (n != 0xFF6633) hi = n; // outra cor do neon: o fim da borda segue a escolha
        D2D1_GRADIENT_STOP bs[4] = {{0, Rgba(0xFFFFFF, 0.10f)}, {0.35f, Rgba(n, 0.18f)}, {0.70f, Rgba(n, 0.55f)}, {1, Rgba(hi)}};
        if (auto* b = Angle(r, 115, bs, 4)) { StrokeBrush(r, radius, b, 1 * s); b->Release(); }
        Fill(D2D1::RectF(r.left + radius, r.top + 1 * s, r.right - radius, r.top + 2 * s), 0, Rgba(0xFFFFFF, 0.06f)); // filete de luz no topo
        // luz que segue o cursor (guarda a última posição para apagar no lugar ao sair)
        if (key && an) {
            float lx = A(key * 4 + 2, over ? mx : an->m.count(key * 4 + 2) ? an->m[key * 4 + 2].to : mx, 0);
            float ly = A(key * 4 + 3, over ? my : an->m.count(key * 4 + 3) ? an->m[key * 4 + 3].to : my, 0);
            if (hov > 0.01f) CursorLight(r, radius, lx, ly, hov);
        }
    }
    void CursorLight(const D2D1_RECT_F& r, float radius, float x, float y, float k) {
        ID2D1GradientStopCollection* c = nullptr; ID2D1RadialGradientBrush* b = nullptr;
        D2D1_GRADIENT_STOP st[2] = {{0, Rgba(NeonColor(), 0.09f * k)}, {1, Rgba(NeonColor(), 0)}};
        if (SUCCEEDED(rt->CreateGradientStopCollection(st, 2, &c))) {
            float R = 220 * s;
            rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(D2D1::Point2F(x, y), D2D1::Point2F(0, 0), R, R), c, &b);
            c->Release();
        }
        if (b) { FillBrush(r, radius, b); b->Release(); }
        D2D1_GRADIENT_STOP bs[2] = {{0, Rgba(NeonColor(), 0.55f * k)}, {1, Rgba(NeonColor(), 0)}}; // borda mais intensa perto do cursor
        if (SUCCEEDED(rt->CreateGradientStopCollection(bs, 2, &c))) {
            float R = 140 * s;
            rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(D2D1::Point2F(x, y), D2D1::Point2F(0, 0), R, R), c, &b);
            c->Release();
            if (b) { StrokeBrush(r, radius, b, 1 * s); b->Release(); }
        }
    }
    // Moldura da janela Crispy: contorno interno fino, filete branco no topo e toque laranja embaixo (sem granulado).
    void WindowFrame(const D2D1_RECT_F& r, float radius) {
        if (!th.neon) { Stroke(r, radius, th.stroke); return; }
        D2D1_GRADIENT_STOP bs[4] = {{0, Rgba(0xFFFFFF, 0.10f)}, {0.35f, Rgba(NeonColor(), 0.18f)}, {0.70f, Rgba(NeonColor(), 0.40f)}, {1, Rgba(NeonColor(), 0.75f)}};
        if (auto* b = Angle(r, 115, bs, 4)) { StrokeBrush(r, radius, b, 1 * s); b->Release(); }
        Fill(D2D1::RectF(r.left + radius, r.top + 1 * s, r.right - radius, r.top + 2 * s), 0, Rgba(0xFFFFFF, 0.10f));
        D2D1_GRADIENT_STOP ls[3] = {{0, Rgba(NeonColor(), 0)}, {0.5f, Rgba(NeonColor(), 0.55f)}, {1, Rgba(NeonColor(), 0)}};
        D2D1_RECT_F bl = D2D1::RectF(r.left + radius, r.bottom - 2 * s, r.right - radius, r.bottom - 1 * s);
        if (auto* b = Angle(bl, 90, ls, 3)) { rt->FillRectangle(bl, b); b->Release(); }
    }
    // Barra de rolagem: trilho grafite fino e botão laranja com neon (Crispy) ou discreto (demais temas).
    void ScrollBar(const D2D1_RECT_F& track, float pos, float len) { // pos/len em fração 0..1
        float w = track.right - track.left, h = track.bottom - track.top, th2 = len * h; if (th2 < 30 * s) th2 = 30 * s;
        float ty = track.top + (h - th2) * pos;
        D2D1_RECT_F k = D2D1::RectF(track.left, ty, track.right, ty + th2);
        if (th.neon) { Fill(track, w / 2, Rgba(0x141416)); Neon(k, w / 2, 0.8f); Fill(k, w / 2, th.accent); }
        else Fill(k, w / 2, th.dark ? Rgba(0xFFFFFF, 0.22f) : Rgba(0x000000, 0.22f));
    }
    // Neon em duas camadas: próxima e intensa + larga e suave. k: 0..1. Só no Crispy.
    void Neon(const D2D1_RECT_F& r, float radius, float k = 1) {
        if (!th.neon || k <= 0.01f) return;
        for (int i = 8; i >= 1; --i) { // larga e suave (~14 px)
            float g = i * 1.8f * s;
            Fill(D2D1::RectF(r.left - g, r.top - g, r.right + g, r.bottom + g), radius + g, Rgba(NeonColor(), k * 0.022f));
        }
        for (int i = 3; i >= 1; --i) { // próxima e intensa (~4 px)
            float g = i * 1.3f * s;
            Stroke(D2D1::RectF(r.left - g, r.top - g, r.right + g, r.bottom + g), radius + g, Rgba(NeonColor(), k * 0.16f * (4 - i)), 1.4f * s);
        }
    }
    void Glow(const D2D1_RECT_F& r, float radius) { Neon(r, radius, 1); }
    // Sombra suave barata (camadas translúcidas).
    void Shadow(const D2D1_RECT_F& r, float radius) {
        for (int i = 4; i >= 1; --i) {
            float g = i * 3 * s;
            Fill(D2D1::RectF(r.left - g, r.top - g + 2 * s, r.right + g, r.bottom + g + 4 * s), radius + g, Rgba(0x000000, th.dark ? 0.07f : 0.035f));
        }
    }
    // "Afundar" ao clicar: escala 0,97 por ~120 ms em volta do centro. Devolve o retângulo a usar.
    D2D1_RECT_F Press(const D2D1_RECT_F& r, int key, bool hot) {
        float k = A(key ? key * 4 + 3 : 0, hot && down ? 0.97f : 1.0f, 120, true, true);
        if (k >= 0.999f) return r;
        float cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, w = (r.right - r.left) * k / 2, h = (r.bottom - r.top) * k / 2;
        return D2D1::RectF(cx - w, cy - h, cx + w, cy + h);
    }

    IDWriteTextFormat* Format(float size, DWRITE_FONT_WEIGHT weight, DWRITE_TEXT_ALIGNMENT align, const wchar_t* family = nullptr, bool wrap = false) {
        IDWriteTextFormat* f = nullptr;
        if (!family) family = th.font;
        IDWriteFontCollection* coll = fonts::Has(family) ? fonts::Collection() : nullptr; // Inter e Space Grotesk vêm no .exe
        if (FAILED(dw->CreateTextFormat(family, coll, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size * s, L"pt-br", &f))) return nullptr;
        f->SetTextAlignment(align);
        f->SetParagraphAlignment(wrap ? DWRITE_PARAGRAPH_ALIGNMENT_NEAR : DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (!wrap) {
            f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            DWRITE_TRIMMING trim = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            IDWriteInlineObject* e = nullptr;
            if (SUCCEEDED(dw->CreateEllipsisTrimmingSign(f, &e))) { f->SetTrimming(&trim, e); e->Release(); }
        }
        return f;
    }
    void Text(const std::wstring& t, const D2D1_RECT_F& r, float size, D2D1_COLOR_F c, DWRITE_FONT_WEIGHT w = DWRITE_FONT_WEIGHT_NORMAL,
              DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING) {
        IDWriteTextFormat* f = Format(size, w, align);
        ID2D1SolidColorBrush* b = nullptr;
        if (f && SUCCEEDED(rt->CreateSolidColorBrush(c, &b))) { rt->DrawText(t.c_str(), (UINT32)t.size(), f, r, b); b->Release(); }
        if (f) f->Release();
    }
    // Texto com espaçamento extra entre as letras (em fração do tamanho da fonte), centralizado no retângulo.
    void TextSpaced(const std::wstring& t, const D2D1_RECT_F& r, float size, D2D1_COLOR_F c, DWRITE_FONT_WEIGHT w, float em) {
        IDWriteTextFormat* f = Format(size, w, DWRITE_TEXT_ALIGNMENT_CENTER);
        if (!f) return;
        IDWriteTextLayout* l = nullptr;
        if (SUCCEEDED(dw->CreateTextLayout(t.c_str(), (UINT32)t.size(), f, r.right - r.left, r.bottom - r.top, &l))) {
            IDWriteTextLayout1* l1 = nullptr;
            float sp = em * size * s;
            if (SUCCEEDED(l->QueryInterface(__uuidof(IDWriteTextLayout1), (void**)&l1))) {
                l1->SetCharacterSpacing(sp / 2, sp / 2, 0, DWRITE_TEXT_RANGE{0, (UINT32)t.size()});
                l1->Release();
            }
            ID2D1SolidColorBrush* b = nullptr;
            if (SUCCEEDED(rt->CreateSolidColorBrush(c, &b))) { rt->DrawTextLayout(D2D1::Point2F(r.left, r.top), l, b); b->Release(); }
            l->Release();
        }
        f->Release();
    }
    // Texto com quebra de linha; retorna a altura usada.
    // lineScale: altura da linha em múltiplos do corpo (1.45 padrão; 1.12 para títulos do protótipo).
    // **trecho** sai em negrito (o protótipo usa <b>).
    float Paragraph(const std::wstring& src, float x, float y, float width, float size, D2D1_COLOR_F c, bool draw = true, float lineScale = 1.45f,
                    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL) {
        std::wstring t; std::vector<std::pair<UINT32, UINT32>> bold; UINT32 start = 0; bool on = false;
        for (size_t i = 0; i < src.size();) {
            if (src[i] == L'*' && i + 1 < src.size() && src[i + 1] == L'*') {
                if (on) bold.push_back({start, (UINT32)t.size() - start}); else start = (UINT32)t.size();
                on = !on; i += 2; continue;
            }
            t.push_back(src[i++]);
        }
        IDWriteTextFormat* f = Format(size, weight, DWRITE_TEXT_ALIGNMENT_LEADING, nullptr, true);
        IDWriteTextLayout* lay = nullptr;
        float h = 0;
        if (f && SUCCEEDED(dw->CreateTextLayout(t.c_str(), (UINT32)t.size(), f, width, 10000, &lay))) {
            for (auto& r : bold) lay->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_RANGE{r.first, r.second});
            lay->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, size * s * lineScale, size * s * 1.1f);
            DWRITE_TEXT_METRICS m; lay->GetMetrics(&m); h = m.height;
            ID2D1SolidColorBrush* b = nullptr;
            if (draw && SUCCEEDED(rt->CreateSolidColorBrush(c, &b))) { rt->DrawTextLayout(D2D1::Point2F(x, y), lay, b); b->Release(); }
            lay->Release();
        }
        if (f) f->Release();
        return h;
    }
    float Measure(const std::wstring& t, float size, DWRITE_FONT_WEIGHT w = DWRITE_FONT_WEIGHT_NORMAL) {
        IDWriteTextFormat* f = Format(size, w, DWRITE_TEXT_ALIGNMENT_LEADING);
        IDWriteTextLayout* lay = nullptr;
        float width = 0;
        if (f && SUCCEEDED(dw->CreateTextLayout(t.c_str(), (UINT32)t.size(), f, 10000, 100, &lay))) {
            DWRITE_TEXT_METRICS m; lay->GetMetrics(&m); width = m.widthIncludingTrailingWhitespace;
            lay->Release();
        }
        if (f) f->Release();
        return width;
    }
    // Ícone Feather pelo nome (traço com pontas redondas, como no feathericons.com).
    void Feather(const char* d, const D2D1_RECT_F& r, float box, D2D1_COLOR_F c, float stroke = 2) {
        if (!d) return;
        ID2D1Factory* f = nullptr; rt->GetFactory(&f);
        float sc = box / 24, ox = (r.left + r.right) / 2 - box / 2, oy = (r.top + r.bottom) / 2 - box / 2;
        ID2D1PathGeometry* g = SvgPath(f, d, sc, ox, oy);
        ID2D1StrokeStyle* st = nullptr;
        f->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND), nullptr, 0, &st);
        ID2D1SolidColorBrush* b = nullptr;
        if (g && SUCCEEDED(rt->CreateSolidColorBrush(c, &b))) { rt->DrawGeometry(g, b, (stroke * sc) > 1.1f ? stroke * sc : 1.1f, st); b->Release(); }
        if (st) st->Release();
        if (g) g->Release();
        f->Release();
    }
    void FeatherIcon(const char* name, const D2D1_RECT_F& r, float box, D2D1_COLOR_F c) { Feather(feather::Find(name), r, box, c); }
    // Ícone cheio (feather::kFilled), centralizado no retângulo.
    void FilledIcon(const char* name, const D2D1_RECT_F& r, float box, D2D1_COLOR_F c) {
        const char* d = feather::FindFilled(name);
        if (!d) return;
        ID2D1Factory* f = nullptr; rt->GetFactory(&f);
        ID2D1PathGeometry* g = SvgPath(f, d, box / 24, (r.left + r.right) / 2 - box / 2, (r.top + r.bottom) / 2 - box / 2, true);
        ID2D1SolidColorBrush* b = nullptr;
        if (g && SUCCEEDED(rt->CreateSolidColorBrush(c, &b))) { rt->FillGeometry(g, b); b->Release(); }
        if (g) g->Release();
        f->Release();
    }
    void Icon(const wchar_t* glyph, const D2D1_RECT_F& r, float size, D2D1_COLOR_F c) {
        if (glyph && glyph[0] && !glyph[1]) // glifo conhecido: desenha o equivalente Feather
            if (const char* d = feather::ForGlyph(glyph[0])) { Feather(d, r, size * 1.3f * s, c); return; }
        IDWriteTextFormat* f = Format(size, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, IconFont());
        ID2D1SolidColorBrush* b = nullptr;
        if (f && SUCCEEDED(rt->CreateSolidColorBrush(c, &b))) { rt->DrawText(glyph, (UINT32)wcslen(glyph), f, r, b); b->Release(); }
        if (f) f->Release();
    }
    // Botão redondo de vidro com ícone (no Crispy: translúcido, acende em laranja com neon ao passar o mouse).
    void CircleButton(const D2D1_RECT_F& r0, const wchar_t* glyph, bool hot, int key = 0, bool accent = false) {
        D2D1_RECT_F r = Press(r0, key, hot);
        float rad = (r.bottom - r.top) / 2, k = A(key ? key * 4 + 1 : 0, hot ? 1.0f : 0.0f, 200);
        if (th.neon) {
            Fill(r, rad, Rgba(0xFFFFFF, 0.05f));
            if (accent) Neon(r, rad, 0.6f + 0.4f * k); else Neon(r, rad, k);
            if (k > 0) Fill(r, rad, Rgba(NeonColor(), 0.16f * k));
            Stroke(r, rad, accent ? th.accent : Mix(Rgba(0xFFFFFF, 0.14f), th.accent, k));
            Icon(glyph, r, 13, accent ? th.accent : Mix(th.text2, th.accent, k));
            return;
        }
        if (k > 0) { D2D1_COLOR_F c = th.panelHot; c.a *= k; Fill(r, rad, c); }
        Stroke(r, rad, th.stroke);
        Icon(glyph, r, 13, Mix(th.text2, th.icon, k));
    }
    // Interruptor estilo iOS. key anima a bolinha e a cor (~220 ms).
    void Toggle(const D2D1_RECT_F& r, bool on, bool hot, int key = 0) {
        float h = r.bottom - r.top;
        float pos = A(key ? key * 4 + 1 : 0, on ? 1.0f : 0.0f, 220, true, true);
        float col = A(key ? key * 4 + 2 : 0, on ? 1.0f : 0.0f, 220, true);
        if (th.neon) Neon(r, h / 2, col);
        Fill(r, h / 2, th.track);
        if (col > 0) { D2D1_COLOR_F c = th.accent; c.a *= col; Fill(r, h / 2, c); }
        if (th.neon && col < 1) Stroke(r, h / 2, Rgba(0xFFFFFF, 0.10f * (1 - col)));
        if (hot) Fill(r, h / 2, Rgba(0xFFFFFF, 0.10f));
        float k = h - 4 * s, x = r.left + 2 * s + (r.right - r.left - 4 * s - k) * pos;
        D2D1_RECT_F kr = D2D1::RectF(x, r.top + 2 * s, x + k, r.top + 2 * s + k);
        Fill(D2D1::RectF(kr.left, kr.top + 1 * s, kr.right, kr.bottom + 1 * s), k / 2, Rgba(0x000000, 0.18f));
        Fill(kr, k / 2, Mix(th.knob, th.knobOn, col));
    }
    // Controle segmentado; quem chama guarda os retângulos de cada parte. key: o indicador desliza (~220 ms).
    void Segmented(const D2D1_RECT_F& r, const std::wstring* labels, int n, int sel, int hot, D2D1_RECT_F* parts, int key = 0) {
        float h = r.bottom - r.top;
        Fill(r, h / 2, th.track);
        if (th.neon) Stroke(r, h / 2, Rgba(0xFFFFFF, 0.06f));
        float w = (r.right - r.left - 4 * s) / n;
        for (int i = 0; i < n; ++i) parts[i] = D2D1::RectF(r.left + 2 * s + i * w, r.top + 2 * s, r.left + 2 * s + (i + 1) * w, r.bottom - 2 * s);
        for (int i = 0; i < n; ++i) if (i == hot && i != sel) Fill(parts[i], (h - 4 * s) / 2, Rgba(th.dark ? 0xFFFFFF : 0x000000, 0.06f));
        if (sel >= 0 && sel < n) {
            float x = A(key ? key * 4 + 1 : 0, (float)sel, 220, true, true);
            D2D1_RECT_F ind = D2D1::RectF(r.left + 2 * s + x * w, r.top + 2 * s, r.left + 2 * s + (x + 1) * w, r.bottom - 2 * s);
            float rad = (h - 4 * s) / 2;
            if (th.neon) Neon(ind, rad, 0.7f);
            Fill(ind, rad, th.neon ? Rgba(0x1A1410) : th.segFill);
            if (th.neon) Fill(ind, rad, th.segFill);
            if (th.neon || !th.classic) Stroke(ind, rad, th.neon ? th.accent : th.stroke);
        }
        for (int i = 0; i < n; ++i)
            Text(labels[i], parts[i], 12.5f, i == sel ? th.segText : th.text2, i == sel ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    // [ − valor + ]; minus/plus recebem os retângulos clicáveis.
    void Stepper(const D2D1_RECT_F& r, const std::wstring& value, int hot, D2D1_RECT_F* minus, D2D1_RECT_F* plus) {
        float h = r.bottom - r.top;
        Fill(r, h / 2, th.track);
        *minus = D2D1::RectF(r.left + 2 * s, r.top + 2 * s, r.left + h - 2 * s, r.bottom - 2 * s);
        *plus = D2D1::RectF(r.right - h + 2 * s, r.top + 2 * s, r.right - 2 * s, r.bottom - 2 * s);
        if (hot == 0) Fill(*minus, (h - 4 * s) / 2, th.panelHot);
        if (hot == 1) Fill(*plus, (h - 4 * s) / 2, th.panelHot);
        Icon(L"", *minus, 11, th.text);
        Icon(L"", *plus, 11, th.text);
        Text(value, D2D1::RectF(r.left + h, r.top, r.right - h, r.bottom), 12.5f, th.text, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    // Botão "pílula" com texto. key: o fundo acende (~200 ms) e afunda ao clicar.
    void Pill(const D2D1_RECT_F& r0, const std::wstring& label, bool hot, bool primary = false, int key = 0) {
        D2D1_RECT_F r = Press(r0, key, hot);
        float h = r.bottom - r.top, k = A(key ? key * 4 + 1 : 0, hot ? 1.0f : 0.0f, 200);
        if (primary) {
            if (th.neon) Neon(r, h / 2, 0.8f + 0.2f * k);
            Fill(r, h / 2, th.accent);
            if (k > 0) Fill(r, h / 2, Rgba(0xFFFFFF, 0.14f * k));
        } else if (th.neon) {
            Fill(r, h / 2, Rgba(0xFFFFFF, 0.04f));
            Neon(r, h / 2, k);
            if (k > 0) Fill(r, h / 2, Rgba(NeonColor(), 0.16f * k));
            Stroke(r, h / 2, Mix(Rgba(0xFFFFFF, 0.12f), th.accent, k));
        } else {
            if (k > 0) { D2D1_COLOR_F c = th.panelHot; c.a *= k; Fill(r, h / 2, c); }
            Stroke(r, h / 2, th.stroke);
        }
        Text(label, r, 12.5f, primary ? th.onAccent : th.neon ? Mix(th.text, th.accent, k) : th.text, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    // Texto com neon (links, "Encerrar processo"): duas cópias deslocadas e translúcidas atrás do texto.
    void NeonText(const std::wstring& t, const D2D1_RECT_F& r, float size, DWRITE_FONT_WEIGHT w, DWRITE_TEXT_ALIGNMENT al, float k = 1) {
        if (th.neon && k > 0.01f) {
            const float o[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {2, 2}, {-2, -2}, {2, -2}, {-2, 2}};
            for (int i = 0; i < 8; ++i) {
                float d = (i < 4 ? 1.2f : 2.4f) * s;
                Text(t, D2D1::RectF(r.left + o[i][0] * d, r.top + o[i][1] * d, r.right + o[i][0] * d, r.bottom + o[i][1] * d), size, Rgba(NeonColor(), (i < 4 ? 0.22f : 0.10f) * k), w, al);
            }
        }
        Text(t, r, size, th.neon ? th.accent : th.text, w, al);
    }
};

inline bool Hit(const D2D1_RECT_F& r, POINT p) { return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom; }

} // namespace ui
