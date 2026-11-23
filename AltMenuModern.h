// AltMenuModern.h - Menu circular radial modernizado (C++17)
// Substitui AltMenu.h/cpp por versão com RAII, enum class, structured bindings, etc.
#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <array>
#include <optional>
#include <cmath>
#include <algorithm>
#include "Resource.h"
#include "Jobs.h"
#include "Config.h"

namespace alt {

// ============================================================================
// Enums modernos
// ============================================================================
enum class Side { Convert = 0, Tools = 1 };

enum class ConvertSegment : int {
    Png = 0, Jpg, Pdf, Webp, Tiff, Zip,
    Count
};

enum class ToolSegment : int {
    Resize = 0, Crop, Rotate, Grayscale, StripMeta, CompressPdf,
    MergePdf, SplitPdf, ExtractPdf,
    PsdLayers, ColorSheet, HexCss, Palettes, FontSpecimen,
    Count
};

// ============================================================================
// Configuração de segmento
// ============================================================================
struct SegmentConfig {
    std::wstring label_pt, label_en, label_es;
    wchar_t shortcut = 0;
    bool enabled = true;

    const std::wstring& Label() const {
        // Simplificado - usaria i18n::CurrentLang()
        return label_pt;
    }
};

// ============================================================================
// Estado do segmento renderizado (para hit testing)
// ============================================================================
struct RenderedSegment {
    float startAngle = 0.0f;
    float sweepAngle = 0.0f;
    float innerRadius = 0.0f;
    float outerRadius = 0.0f;
    int index = -1;
    Side side = Side::Convert;
};

// ============================================================================
// AltMenu Modernizado
// ============================================================================
class AltMenu {
public:
    // Singleton
    static AltMenu& Instance() {
        static AltMenu instance;
        return instance;
    }

    // Non-copyable, movable
    AltMenu(const AltMenu&) = delete;
    AltMenu& operator=(const AltMenu&) = delete;

    // Inicialização
    bool Initialize(HWND parentWnd, HINSTANCE inst) {
        parentWnd_ = parentWnd;
        inst_ = inst;

        WNDCLASSEXW wc{sizeof(wc)};
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProcStatic;
        wc.hInstance = inst_;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"VizuAlt";
        return RegisterClassExW(&wc) != 0;
    }

    // Mostra o menu
    void Show(HWND defView, const std::vector<std::wstring>& files) {
        if (visible_ || files.empty()) return;

        files_ = files;
        currentSide_ = Side::Convert;
        ringRotation_ = 0.0f;

        // Calcula posição centralizada na janela pai
        RECT rc;
        GetWindowRect(parentWnd_, &rc);
        constexpr int kSize = 480;
        int x = rc.left + (rc.right - rc.left - kSize) / 2;
        int y = rc.top + (rc.bottom - rc.top - kSize) / 2;

        // Cria janela
        wnd_ = res::CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
            L"VizuAlt", L"", WS_POPUP,
            x, y, kSize, kSize,
            nullptr, nullptr, inst_, this
        );

        if (!wnd_) return;

        // Habilita glass/acrylic
        ui::EnableGlass(wnd_.Get(), theme::ThemeManager::Instance().IsDark());
        SetLayeredWindowAttributes(wnd_.Get(), 0, 0, LWA_ALPHA);
        ShowWindow(wnd_.Get(), SW_SHOWNOACTIVATE);

        visible_ = true;
        BuildSegments();
        InvalidateRect(wnd_.Get(), nullptr, FALSE);
    }

    void Hide() {
        if (!visible_) return;
        visible_ = false;
        wnd_.Reset();
        files_.clear();
        renderedSegments_.clear();
    }

    bool IsVisible() const { return visible_; }

    bool CursorOver() const {
        if (!visible_ || !wnd_) return false;
        POINT pt; GetCursorPos(&pt);
        RECT rc; GetWindowRect(wnd_.Get(), &rc);
        return PtInRect(&rc, pt);
    }

    // Atualização periódica (timer)
    void Update() {
        if (!visible_) return;

        // Verifica se Alt ainda está pressionado
        if (!(GetAsyncKeyState(VK_MENU) < 0)) {
            if (!CursorOver()) Hide();
            return;
        }

        // Verifica outras teclas (mantém aberto se outra tecla pressionada)
        for (int vk = 1; vk < 255; ++vk) {
            if (vk != VK_MENU && vk != VK_LMENU && vk != VK_RMENU && GetAsyncKeyState(vk) < 0) {
                return;
            }
        }
    }

    // Click handler
    void OnClick(POINT pt) {
        auto seg = SegmentAt(pt);
        if (!seg) {
            // Centro: troca de lado
            ToggleSide();
            return;
        }

        ExecuteSegment(*seg);
        Hide();
    }

    // Mouse move
    void OnMouseMove(POINT pt) {
        // Hot tracking - poderia atualizar segmento sob cursor
        hoveredSegment_ = SegmentAt(pt);
    }

    // Mouse wheel - troca lado
    void OnMouseWheel(short delta) {
        (void)delta;
        ToggleSide();
    }

    // Paint
    void Paint(HWND h, ID2D1RenderTarget* rt, IDWriteFactory* dw, const ui::Theme& theme, float scale) {
        if (!visible_) return;

        RECT rc;
        GetClientRect(h, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int cx = w / 2, cy = h / 2;

        constexpr float kInnerR = 120.0f;
        constexpr float kOuterR = 220.0f;
        constexpr float kCenterR = 90.0f;

        const auto& segs = CurrentSegments();
        float segAngle = 2.0f * 3.14159265f / segs.size();

        // Desenha segmentos
        for (size_t i = 0; i < segs.size(); ++i) {
            float startAngle = segAngle * i - 3.14159265f / 2 + ringRotation_;
            float sweepAngle = segAngle;

            const auto& cfg = segs[i];
            bool hot = hoveredSegment_ && hoveredSegment_->index == static_cast<int>(i);

            // Cores baseadas no tema
            auto baseColor = hot ? theme.accent : theme.surface;
            auto textColor = hot ? theme.onAccent : theme.text;

            // Desenha arco do segmento
            // TODO: Implementar com Direct2D geometry
            DrawSegment(rt, cx, cy, kInnerR, kOuterR, startAngle, sweepAngle, baseColor, scale);

            // Desenha texto/atalho
            float midAngle = startAngle + sweepAngle / 2;
            float textR = (kInnerR + kOuterR) / 2;
            float tx = cx + textR * cosf(midAngle);
            float ty = cy + textR * sinf(midAngle);

            // Simplificado - texto centralizado no segmento
            DrawSegmentLabel(rt, dw, cfg.Label(), cfg.shortcut, tx, ty, textColor, scale);
        }

        // Desenha centro (ícone de troca)
        DrawCenter(rt, cx, cy, kCenterR, theme, scale);
    }

    // Window proc static
    static LRESULT CALLBACK WndProcStatic(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = Instance();
        if (h == self->wnd_.Get()) {
            switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                BeginPaint(h, &ps);
                // self->Paint(h, g_d2d, g_dw, g_theme, scale);
                EndPaint(h, &ps);
                return 0;
            }
            case WM_LBUTTONUP: {
                POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                self->OnClick(pt);
                return 0;
            }
            case WM_MOUSEMOVE: {
                POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                self->OnMouseMove(pt);
                return 0;
            }
            case WM_MOUSEWHEEL: {
                short delta = GET_WHEEL_DELTA_WPARAM(wp);
                self->OnMouseWheel(delta);
                return 0;
            }
            case WM_DESTROY:
                self->visible_ = false;
                self->wnd_.Reset();
                return 0;
            }
        }
        return DefWindowProcW(h, msg, wp, lp);
    }

private:
    AltMenu() = default;
    ~AltMenu() = default;

    void BuildSegments() {
        // Lado Conversão
        convertSegs_ = {
            {L"PNG", L"PNG", L"PNG", L'P'},
            {L"JPG", L"JPG", L"JPG", L'J'},
            {L"PDF", L"PDF", L"PDF", L'F'},
            {L"WEBP", L"WEBP", L"WEBP", L'W'},
            {L"TIFF", L"TIFF", L"TIFF", L'T'},
            {L"ZIP", L"ZIP", L"ZIP", L'Z'},
        };

        // Lado Ferramentas
        toolSegs_ = {
            {L"Redimensionar", L"Resize", L"Redimensionar", L'R'},
            {L"Recortar", L"Crop", L"Recortar", L'C'},
            {L"Girar", L"Rotate", L"Girar", L'O'},
            {L"Cinza", L"Grayscale", L"Escala de grises", L'G'},
            {L"Sem metadados", L"Strip meta", L"Sin metadatos", L'M'},
            {L"Comprimir PDF", L"Compress PDF", L"Comprimir PDF", L'P'},
            {L"Juntar PDFs", L"Merge PDFs", L"Unir PDFs", L'U'},
            {L"Dividir PDF", L"Split PDF", L"Dividir PDF", L'D'},
            {L"Extrair páginas", L"Extract pages", L"Extraer páginas", L'E'},
            {L"Camadas PSD", L"PSD layers", L"Capas PSD", L'L'},
            {L"Folha de cores", L"Color sheet", L"Hoja de colores", L'S'},
            {L"HEX/CSS", L"HEX/CSS", L"HEX/CSS", L'H'},
            {L"ASE↔ACO", L"ASE↔ACO", L"ASE↔ACO", L'A'},
            {L"Amostra de fonte", L"Font specimen", L"Muestra de fuente", L'N'},
        };
    }

    const std::vector<SegmentConfig>& CurrentSegments() const {
        return (currentSide_ == Side::Convert) ? convertSegs_ : toolSegs_;
    }

    void ToggleSide() {
        currentSide_ = (currentSide_ == Side::Convert) ? Side::Tools : Side::Convert;
        ringRotation_ = 0.0f;
        hoveredSegment_.reset();
        if (wnd_) InvalidateRect(wnd_.Get(), nullptr, FALSE);
    }

    std::optional<RenderedSegment> SegmentAt(POINT pt) const {
        if (!wnd_) return std::nullopt;

        RECT rc;
        GetClientRect(wnd_.Get(), &rc);
        int cx = (rc.left + rc.right) / 2;
        int cy = (rc.top + rc.bottom) / 2;

        float dx = pt.x - cx;
        float dy = pt.y - cy;
        float dist = std::sqrt(dx * dx + dy * dy);

        constexpr float kInnerR = 120.0f;
        constexpr float kOuterR = 220.0f;
        constexpr float kCenterR = 90.0f;

        if (dist < kCenterR) {
            // Centro - retorna nulo para indicar troca de lado
            return std::nullopt;
        }
        if (dist < kInnerR || dist > kOuterR) return std::nullopt;

        float angle = std::atan2(dy, dx);
        if (angle < 0) angle += 2.0f * 3.14159265f;

        const auto& segs = CurrentSegments();
        float segAngle = 2.0f * 3.14159265f / segs.size();

        // Ajusta pela rotação do anel
        angle -= ringRotation_;
        if (angle < 0) angle += 2.0f * 3.14159265f;

        int idx = static_cast<int>((angle + segAngle / 2) / segAngle) % segs.size();
        if (idx < 0) idx += segs.size();

        RenderedSegment seg;
        seg.startAngle = idx * segAngle - 3.14159265f / 2 + ringRotation_;
        seg.sweepAngle = segAngle;
        seg.innerRadius = kInnerR;
        seg.outerRadius = kOuterR;
        seg.index = idx;
        seg.side = currentSide_;

        return seg;
    }

    void ExecuteSegment(const RenderedSegment& seg) {
        const auto& segs = CurrentSegments();
        if (seg.index >= static_cast<int>(segs.size())) return;

        const auto& cfg = segs[seg.index];

        if (currentSide_ == Side::Convert) {
            // Conversão
            jobs::Format fmt = jobs::kFmtPng;
            if (cfg.shortcut == L'J') fmt = jobs::kFmtJpg;
            else if (cfg.shortcut == L'F') fmt = jobs::kFmtPdf;
            else if (cfg.shortcut == L'W') fmt = jobs::kFmtWebp;
            else if (cfg.shortcut == L'T') fmt = jobs::kFmtTiff;
            else if (cfg.shortcut == L'Z') fmt = jobs::kFmtZip;

            jobs::Run(parentWnd_, files_, fmt);
        } else {
            // Ferramentas
            using Tool = jobs::Tool;
            Tool tool = Tool::Resize;

            if (cfg.shortcut == L'R') tool = Tool::Resize;
            else if (cfg.shortcut == L'C') tool = Tool::Crop;
            else if (cfg.shortcut == L'O') tool = Tool::Rotate;
            else if (cfg.shortcut == L'G') tool = Tool::Grayscale;
            else if (cfg.shortcut == L'M') tool = Tool::StripMeta;
            else if (cfg.shortcut == L'P') tool = Tool::CompressPdf;
            else if (cfg.shortcut == L'U') tool = Tool::MergePdf;
            else if (cfg.shortcut == L'D') tool = Tool::SplitPdf;
            else if (cfg.shortcut == L'E') tool = Tool::ExtractPdf;
            else if (cfg.shortcut == L'L') tool = Tool::PsdLayers;
            else if (cfg.shortcut == L'S') tool = Tool::ColorSheet;
            else if (cfg.shortcut == L'H') tool = Tool::HexCss;
            else if (cfg.shortcut == L'A') tool = Tool::Palettes;
            else if (cfg.shortcut == L'N') tool = Tool::FontSpecimen;

            jobs::Run(parentWnd_, files_, tool);
        }
    }

    // Drawing helpers (implementação simplificada)
    void DrawSegment(ID2D1RenderTarget* rt, int cx, int cy,
                     float innerR, float outerR, float startAngle, float sweepAngle,
                     D2D1_COLOR_F color, float scale) {
        // Criar path geometry para o arco
        // Simplificado - implementação real usaria ID2D1PathGeometry
    }

    void DrawSegmentLabel(ID2D1RenderTarget* rt, IDWriteFactory* dw,
                          const std::wstring& label, wchar_t shortcut,
                          float x, float y, D2D1_COLOR_F color, float scale) {
        // Desenha label e atalho
        // Simplificado
    }

    void DrawCenter(ID2D1RenderTarget* rt, int cx, int cy,
                    float radius, const ui::Theme& theme, float scale) {
        // Desenha círculo central com ícone de seta dupla
    }

    // Estado
    HWND parentWnd_ = nullptr;
    HINSTANCE inst_ = nullptr;
    res::ScopedWindow wnd_;

    bool visible_ = false;
    bool converting_ = false;
    Side currentSide_ = Side::Convert;
    float ringRotation_ = 0.0f;

    std::vector<std::wstring> files_;
    std::vector<SegmentConfig> convertSegs_;
    std::vector<SegmentConfig> toolSegs_;
    std::vector<RenderedSegment> renderedSegments_;
    std::optional<RenderedSegment> hoveredSegment_;
};

} // namespace alt