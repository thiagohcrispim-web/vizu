// AltMenu.cpp - Implementação do menu circular radial
#include "AltMenu.h"
#include "Config.h"
#include "ExplorerIntegration.h"
#include "Jobs.h"
#include "Convert.h"
#include "Ui.h"
#include <cmath>
#include <algorithm>

using conv::SafeRelease;

namespace alt {

AltMenu& AltMenu::Instance() {
    static AltMenu instance;
    return instance;
}

void AltMenu::Initialize(HWND parentWnd, HINSTANCE inst) {
    parentWnd_ = parentWnd;
    inst_ = inst;

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"VizuAlt";
    RegisterClassExW(&wc);
}

void AltMenu::Show(HWND defView, const std::vector<std::wstring>& files) {
    if (visible_ || files.empty()) return;

    convFiles_ = files;
    currentSide_ = kSideConvert;
    ringAngle_ = 0.0f;

    // Cria janela do menu
    int size = 480;
    RECT rc;
    GetWindowRect(parentWnd_, &rc);
    int x = rc.left + (rc.right - rc.left - size) / 2;
    int y = rc.top + (rc.bottom - rc.top - size) / 2;

    altWnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        L"VizuAlt", L"", WS_POPUP,
        x, y, size, size,
        nullptr, nullptr, inst_, this
    );

    ui::EnableGlass(altWnd_, theme::ThemeManager::Instance().IsDark());
    SetLayeredWindowAttributes(altWnd_, 0, 0, LWA_ALPHA);
    ShowWindow(altWnd_, SW_SHOWNOACTIVATE);
    visible_ = true;

    BuildSides();
    InvalidateRect(altWnd_, nullptr, FALSE);
}

void AltMenu::Hide() {
    if (!visible_) return;
    visible_ = false;
    if (altWnd_) {
        DestroyWindow(altWnd_);
        altWnd_ = nullptr;
    }
    convFiles_.clear();
}

bool AltMenu::CursorOver() const {
    if (!visible_ || !altWnd_) return false;
    POINT pt; GetCursorPos(&pt);
    RECT rc; GetWindowRect(altWnd_, &rc);
    return PtInRect(&rc, pt);
}

void AltMenu::Update() {
    if (!visible_) return;

    // Verifica se Alt ainda está pressionado
    if (!(GetAsyncKeyState(VK_MENU) < 0)) {
        if (!CursorOver()) Hide();
        return;
    }

    // Verifica outras teclas pressionadas (reinicia timer)
    for (int vk = 1; vk < 255; ++vk) {
        if (vk != VK_MENU && vk != VK_LMENU && vk != VK_RMENU && GetAsyncKeyState(vk) < 0) {
            return; // Outra tecla pressionada, mantém aberto
        }
    }
}

void AltMenu::BuildSides() {
    convertSegs_.clear();
    toolSegs_.clear();

    // Lado 0: Conversão
    convertSegs_ = {
        {T(L"PNG", L"PNG", L"PNG"), L"P", true},
        {T(L"JPG", L"JPG", L"JPG"), L"J", true},
        {T(L"PDF", L"PDF", L"PDF"), L"F", true},
        {T(L"WEBP", L"WEBP", L"WEBP"), L"W", true},
        {T(L"TIFF", L"TIFF", L"TIFF"), L"T", true},
        {T(L"ZIP", L"ZIP", L"ZIP"), L"Z", true},
    };

    // Lado 1: Ferramentas
    toolSegs_ = {
        {T(L"Redimensionar", L"Resize", L"Redimensionar"), L"R", true},
        {T(L"Recortar", L"Crop", L"Recortar"), L"C", true},
        {T(L"Girar", L"Rotate", L"Girar"), L"O", true},
        {T(L"Cinza", L"Grayscale", L"Escala de grises"), L"G", true},
        {T(L"Sem metadados", L"Strip meta", L"Sin metadatos"), L"M", true},
        {T(L"Comprimir PDF", L"Compress PDF", L"Comprimir PDF"), L"P", true},
        // PDF extras
        {T(L"Juntar PDFs", L"Merge PDFs", L"Unir PDFs"), L"U", true},
        {T(L"Dividir PDF", L"Split PDF", L"Dividir PDF"), L"D", true},
        {T(L"Extrair páginas", L"Extract pages", L"Extraer páginas"), L"E", true},
        // Extras
        {T(L"Camadas PSD", L"PSD layers", L"Capas PSD"), L"L", true},
        {T(L"Folha de cores", L"Color sheet", L"Hoja de colores"), L"S", true},
        {T(L"HEX/CSS", L"HEX/CSS", L"HEX/CSS"), L"H", true},
        {T(L"ASE↔ACO", L"ASE↔ACO", L"ASE↔ACO"), L"A", true},
        {T(L"Amostra de fonte", L"Font specimen", L"Muestra de fuente"), L"N", true},
    };
}

int AltMenu::SegmentAt(POINT pt) const {
    if (!altWnd_) return -1;

    RECT rc;
    GetClientRect(altWnd_, &rc);
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;

    float dx = pt.x - cx;
    float dy = pt.y - cy;
    float dist = sqrtf(dx * dx + dy * dy);

    // Raio interno e externo do anel
    float innerR = 120.0f;
    float outerR = 220.0f;

    if (dist < innerR || dist > outerR) return -1; // Centro ou fora

    float angle = atan2f(dy, dx); // -PI a PI
    if (angle < 0) angle += 2 * 3.14159265f;

    const auto& segs = (currentSide_ == kSideConvert) ? convertSegs_ : toolSegs_;
    float segAngle = 2 * 3.14159265f / segs.size();

    int idx = static_cast<int>((angle + segAngle / 2) / segAngle) % segs.size();
    if (idx < 0) idx += segs.size();

    return idx;
}

void AltMenu::OnClick(POINT pt) {
    int seg = SegmentAt(pt);
    if (seg < 0) {
        // Clique no centro: troca de lado
        currentSide_ = (currentSide_ == kSideConvert) ? kSideTools : kSideConvert;
        ringAngle_ = 0.0f;
        InvalidateRect(altWnd_, nullptr, FALSE);
        return;
    }

    // Executa ação
    const auto& segs = (currentSide_ == kSideConvert) ? convertSegs_ : toolSegs_;
    if (seg >= (int)segs.size()) return;

    const RingConfig& cfg = segs[seg];

    if (currentSide_ == kSideConvert) {
        // Conversão
        jobs::Format fmt = jobs::kFmtPng;
        if (cfg.label == L"JPG" || cfg.label == L"JPG") fmt = jobs::kFmtJpg;
        else if (cfg.label == L"PDF") fmt = jobs::kFmtPdf;
        else if (cfg.label == L"WEBP") fmt = jobs::kFmtWebp;
        else if (cfg.label == L"TIFF") fmt = jobs::kFmtTiff;
        else if (cfg.label == L"ZIP") fmt = jobs::kFmtZip;

        jobs::Run(parentWnd_, convFiles_, fmt);
    } else {
        // Ferramentas
        if (cfg.label == T(L"Redimensionar", L"Resize", L"Redimensionar")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kToolResize);
        } else if (cfg.label == T(L"Recortar", L"Crop", L"Recortar")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kToolCrop);
        } else if (cfg.label == T(L"Girar", L"Rotate", L"Girar")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kToolRotate);
        } else if (cfg.label == T(L"Cinza", L"Grayscale", L"Escala de grises")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kToolGrayscale);
        } else if (cfg.label == T(L"Sem metadados", L"Strip meta", L"Sin metadatos")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kToolStripMeta);
        } else if (cfg.label == T(L"Comprimir PDF", L"Compress PDF", L"Comprimir PDF")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kToolCompressPdf);
        } else if (cfg.label == T(L"Juntar PDFs", L"Merge PDFs", L"Unir PDFs")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kToolMergePdf);
        } else if (cfg.label == T(L"Dividir PDF", L"Split PDF", L"Dividir PDF")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kToolSplitPdf);
        } else if (cfg.label == T(L"Extrair páginas", L"Extract pages", L"Extraer páginas")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kToolExtractPdf);
        } else if (cfg.label == T(L"Camadas PSD", L"PSD layers", L"Capas PSD")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kExtraPsdLayers);
        } else if (cfg.label == T(L"Folha de cores", L"Color sheet", L"Hoja de colores")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kExtraColorSheet);
        } else if (cfg.label == T(L"HEX/CSS", L"HEX/CSS", L"HEX/CSS")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kExtraHexCss);
        } else if (cfg.label == T(L"ASE↔ACO", L"ASE↔ACO", L"ASE↔ACO")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kExtraPalettes);
        } else if (cfg.label == T(L"Amostra de fonte", L"Font specimen", L"Muestra de fuente")) {
            jobs::Run(parentWnd_, convFiles_, jobs::kExtraFontSpecimen);
        }
    }

    Hide();
}

void AltMenu::OnMouseMove(POINT pt) {
    int seg = SegmentAt(pt);
    // Atualiza hot tracking - implementar se necessário
}

void AltMenu::OnMouseWheel(short delta) {
    // Roda do mouse: troca de lado
    currentSide_ = (currentSide_ == kSideConvert) ? kSideTools : kSideConvert;
    ringAngle_ = 0.0f;
    InvalidateRect(altWnd_, nullptr, FALSE);
}

void AltMenu::Paint() {
    if (!visible_ || !altWnd_) return;

    PAINTSTRUCT ps;
    BeginPaint(altWnd_, &ps);

    RECT rc;
    GetClientRect(altWnd_, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    // Cria superfície Direct2D se necessário
    // (simplificado - usa g_d2d, g_dw do VizuMain)

    const auto& segs = (currentSide_ == kSideConvert) ? convertSegs_ : toolSegs_;
    int cx = w / 2, cy = h / 2;
    float innerR = 120.0f;
    float outerR = 220.0f;
    float centerR = 90.0f;

    // Desenha anel
    for (size_t i = 0; i < segs.size(); ++i) {
        float startAngle = 2 * 3.14159265f * i / segs.size() - 3.14159265f / 2;
        float sweepAngle = 2 * 3.14159265f / segs.size();

        // TODO: Desenhar segmento com Direct2D
    }

    // Desenha centro (ícone de troca de lado)
    // TODO: Desenhar centro

    EndPaint(altWnd_, &ps);
}

LRESULT CALLBACK AltMenu::WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = Instance();
    if (h == self->altWnd_) {
        switch (msg) {
        case WM_PAINT:
            self->Paint();
            return 0;
        case WM_LBUTTONUP: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            self->OnClick(pt);
            return 0;
        }
        case WM_MOUSEMOVE: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
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
            self->altWnd_ = nullptr;
            return 0;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

} // namespace alt