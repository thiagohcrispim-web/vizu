// AltMenu.h - Menu circular radial (Alt) com dois lados
#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <memory>

namespace alt {

enum Side { kSideConvert = 0, kSideTools = 1 };
enum Segment {
    // Side 0: Conversão
    kSegPng = 0, kSegJpg, kSegPdf, kSegWebp, kSegTiff, kSegZip,
    // Side 1: Ferramentas
    kSegResize, kSegCrop, kSegRotate, kSegGrayscale, kSegStripMeta, kSegCompressPdf,
    // Side 1 cont.: PDF
    kSegMergePdf, kSegSplitPdf, kSegExtractPdf, kSegCompressPdf2,
    // Extras
    kSegPsdLayers, kSegColorSheet, kSegHexCss, kSegPalettes, kSegFontSpecimen,
    kSegCount
};

struct RingConfig {
    std::wstring label;
    std::wstring shortcut; // ex: "P" para PNG
    bool enabled = true;
};

class AltMenu {
public:
    static AltMenu& Instance();

    void Initialize(HWND parentWnd, HINSTANCE inst);
    void Show(HWND defView, const std::vector<std::wstring>& files);
    void Hide();
    bool IsVisible() const { return visible_; }
    bool CursorOver() const;

    // Called from timer
    void Update();

    // Called from WM_LBUTTONUP on g_altWnd
    void OnClick(POINT pt);

    // Called from WM_MOUSEMOVE on g_altWnd
    void OnMouseMove(POINT pt);

    // Called from WM_MOUSEWHEEL on g_altWnd
    void OnMouseWheel(short delta);

    // Render
    void Paint();

    // Handle window messages for g_altWnd
    static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

private:
    AltMenu() = default;
    ~AltMenu() = default;
    AltMenu(const AltMenu&) = delete;
    AltMenu& operator=(const AltMenu&) = delete;

    void BuildSides();
    void RenderRing(HWND h);
    int SegmentAt(POINT pt) const;

    HWND parentWnd_ = nullptr;
    HWND altWnd_ = nullptr;
    HINSTANCE inst_ = nullptr;

    bool visible_ = false;
    bool converting_ = false;
    Side currentSide_ = kSideConvert;
    float ringAngle_ = 0.0f; // para animação de rotação

    std::vector<std::wstring> convFiles_;
    std::vector<RingConfig> convertSegs_;
    std::vector<RingConfig> toolSegs_;

    // Hit testing
    struct HitRect { int id; RECT rect; };
    std::vector<HitRect> hits_;
};

} // namespace alt