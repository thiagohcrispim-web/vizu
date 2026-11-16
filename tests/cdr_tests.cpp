// CorelDRAW sem Inkscape: CDR e CMX reais (suíte do LibreOffice) viram PDF, SVG e imagem; arquivo malformado é
// recusado sem travar; degradê, texto (curvas), imagem e arco passam pelo gerador como a libcdr os entrega.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000003
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <d2d1_3.h>
#include "../Jobs.h"
#include <iostream>
#include <stdexcept>

static int checks = 0;
static void Check(bool ok, const std::string& message) { ++checks; if (!ok) throw std::runtime_error(message); }

static std::wstring Fixture(const wchar_t* name) {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir = exe; dir = dir.substr(0, dir.find_last_of(L"\\/"));
    for (const wchar_t* rel : {L"\\..\\tests\\fixtures\\", L"\\tests\\fixtures\\", L"\\fixtures\\"}) {
        std::wstring p = dir + rel + name;
        if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p;
    }
    return name;
}
// Pixel (x, y) em pontos de uma página desenhada a 72 DPI: devolve B, G, R, A.
static const BYTE* At(HBITMAP b, double x, double y) {
    BYTE* px; UINT w, h;
    ops::Pixels(b, &px, &w, &h);
    UINT ix = std::min(w - 1, (UINT)x), iy = std::min(h - 1, (UINT)y);
    return px + ((size_t)iy * w + ix) * 4;
}
static bool Near(const BYTE* p, int r, int g, int b, int tol = 40) { return std::abs(p[2] - r) <= tol && std::abs(p[1] - g) <= tol && std::abs(p[0] - b) <= tol && p[3] > 200; }

int wmain() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        std::wstring err;
        // 1. CDR real: quatro retângulos coloridos e uma moldura sem preenchimento
        cdrv::Drawing d;
        Check(cdrv::Read(Fixture(L"fdo65220-2.cdr"), d, err), "read cdr: " + pdfppt::Utf8(err));
        Check(d.pages.size() == 1 && d.pages[0].items.size() == 5 && std::fabs(d.pages[0].w - 595.276) < 0.01, "cdr content");
        std::string pdf = cdrv::Pdf(d);
        Check(pdf.rfind("%PDF-1.5", 0) == 0 && pdf.find("%%EOF") != std::string::npos, "pdf written");
        auto bm = cdrv::Raster(d, {0}, 72, err);
        Check(bm.size() == 1, "raster cdr: " + pdfppt::Utf8(err));
        Check(Near(At(bm[0], 100, 140), 0, 0, 255, 10), "blue rectangle where CorelDRAW put it");
        Check(Near(At(bm[0], 100, 260), 255, 0, 0, 10), "red rectangle");
        Check(At(bm[0], 300, 750)[3] == 0, "transparent outside the drawing");
        DeleteObject(bm[0]);
        std::string svg = cdrv::Svg(d.pages[0]);
        Check(svg.find("fill=\"#0000FF\"") != std::string::npos && svg.find("viewBox=\"0 0 595.276 841.89\"") != std::string::npos, "svg");

        // 2. CMX real (Corel Presentation Exchange): emblema com mais de cem objetos
        cdrv::Drawing x;
        Check(cdrv::Read(Fixture(L"fdo63782-1.cmx"), x, err) && x.pages.size() == 1 && x.pages[0].items.size() > 100, "read cmx");
        bm = cdrv::Raster(x, {0}, 144, err);
        Check(bm.size() == 1 && Near(At(bm[0], 80 * 2, 60 * 2), 0, 56, 115, 20) && Near(At(bm[0], 110 * 2, 75 * 2), 0, 56, 115, 20), "cmx blue shield");
        DeleteObject(bm[0]);

        // 3. arquivo malformado: recusado com mensagem, sem travar
        cdrv::Drawing bad;
        Check(!cdrv::Read(Fixture(L"EDB-35217-1.cdr"), bad, err) && !err.empty(), "malformed rejected");
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        std::wstring junk = std::wstring(tmp) + L"vizu-junk.cdr";
        DeleteFileW(junk.c_str()); ops::WriteFileBytes(junk, "RIFF\0\0\0\0CDRxvrsn garbage garbage", 34);
        Check(!cdrv::Read(junk, bad, err), "garbage rejected");

        // 4. o que a libcdr entrega: degradê, arco, texto e imagem
        using librevenge::RVNGPropertyList; using librevenge::RVNGPropertyListVector;
        cdrv::Collector c;
        RVNGPropertyList page; page.insert("svg:width", 3.0); page.insert("svg:height", 2.0);
        c.startPage(page);
        RVNGPropertyList st; st.insert("draw:fill", "gradient"); st.insert("draw:style", "linear"); st.insert("draw:angle", 90);
        st.insert("draw:start-color", "#FF0000"); st.insert("draw:end-color", "#0000FF"); st.insert("draw:stroke", "none");
        c.setStyle(st);
        auto node = [](const char* a, double x, double y) { RVNGPropertyList p; p.insert("librevenge:path-action", a); p.insert("svg:x", x); p.insert("svg:y", y); return p; };
        RVNGPropertyListVector path;
        path.append(node("M", 0, 0)); path.append(node("L", 3, 0)); path.append(node("L", 3, 1)); path.append(node("L", 0, 1)); path.append(node("Z", 0, 0));
        RVNGPropertyList shape; shape.insert("svg:d", path);
        c.drawPath(shape);
        RVNGPropertyList solid; solid.insert("draw:fill", "solid"); solid.insert("draw:fill-color", "#FFCC00"); solid.insert("draw:stroke", "solid");
        solid.insert("svg:stroke-color", "#000000"); solid.insert("svg:stroke-width", 0.0);
        c.setStyle(solid);
        RVNGPropertyListVector arc; // semicírculo de raio 0,25" em (2,5", 1,5")
        arc.append(node("M", 2.25, 1.5));
        RVNGPropertyList a = node("A", 2.75, 1.5); a.insert("svg:rx", 0.25); a.insert("svg:ry", 0.25); a.insert("librevenge:large-arc", 0); a.insert("librevenge:sweep", 1);
        arc.append(a); arc.append(node("Z", 0, 0));
        RVNGPropertyList arcShape; arcShape.insert("svg:d", arc);
        c.drawPath(arcShape);
        RVNGPropertyList text; text.insert("svg:x", 0.1); text.insert("svg:y", 1.6); text.insert("svg:width", 0.0); text.insert("svg:height", 0.0);
        c.startTextObject(text);
        RVNGPropertyList para; para.insert("fo:text-align", "left");
        c.openParagraph(para);
        RVNGPropertyList span; span.insert("style:font-name", "Fonte Que Nao Existe"); span.insert("fo:font-size", 36.0, librevenge::RVNG_POINT); span.insert("fo:color", "#00AA00");
        c.openSpan(span); c.insertText("HHH"); c.closeSpan(); c.closeParagraph();
        c.endTextObject();
        // imagem BMP 2x2 (vermelho, verde / azul, branco), 0,5" x 0,5" em (1", 1,2")
        static const unsigned char bmp[] = {
            'B','M', 70,0,0,0, 0,0,0,0, 54,0,0,0, 40,0,0,0, 2,0,0,0, 2,0,0,0, 1,0, 24,0, 0,0,0,0, 16,0,0,0, 0x13,0x0B,0,0, 0x13,0x0B,0,0, 0,0,0,0, 0,0,0,0,
            255,0,0, 255,255,255, 0,0, 0,0,255, 0,255,0, 0,0};
        librevenge::RVNGBinaryData bin(bmp, sizeof(bmp));
        RVNGPropertyList img; img.insert("svg:x", 1.0); img.insert("svg:y", 1.2); img.insert("svg:width", 0.5); img.insert("svg:height", 0.5);
        img.insert("librevenge:mime-type", "image/bmp"); img.insert("office:binary-data", bin);
        c.drawGraphicObject(img);
        c.endPage();
        Check(c.drawing.pages.size() == 1 && c.drawing.pages[0].items.size() == 4, "synthetic items");
        Check(c.drawing.pages[0].items[0].fill.kind == cdrv::Paint::Linear && c.drawing.pages[0].items[1].stroke.width == 0.25, "gradient and hairline");
        Check(c.drawing.pages[0].items[2].segs.size() > 6, "text became outlines (fallback font)");
        bm = cdrv::Raster(c.drawing, {0}, 72, err);
        Check(bm.size() == 1, "raster synthetic");
        Check(Near(At(bm[0], 4, 36), 255, 0, 0, 25), "gradient starts red on the left");
        Check(Near(At(bm[0], 212, 36), 0, 0, 255, 25), "gradient ends blue on the right");
        // sweep=1 no SVG (y para baixo) vai no sentido horário: da esquerda passa por cima
        Check(Near(At(bm[0], 180, 100), 255, 204, 0, 10) && Near(At(bm[0], 170, 104), 255, 204, 0, 10), "arc filled above");
        Check(At(bm[0], 180, 116)[3] == 0 && At(bm[0], 180, 88)[3] == 0, "arc is a half circle of radius 18pt");
        Check(Near(At(bm[0], 76, 92), 255, 0, 0, 30) && Near(At(bm[0], 101, 92), 0, 255, 0, 30), "image top row");
        Check(Near(At(bm[0], 76, 116), 0, 0, 255, 30) && Near(At(bm[0], 101, 116), 255, 255, 255, 30), "image bottom row");
        int green = 0;
        for (int yy = 85; yy < 116; ++yy) for (int xx = 7; xx < 70; ++xx) green += Near(At(bm[0], xx, yy), 0, 170, 0, 30);
        Check(green > 150, "text drawn in green above the baseline");
        DeleteObject(bm[0]);
        svg = cdrv::Svg(c.drawing.pages[0]);
        Check(svg.find("<linearGradient") != std::string::npos && svg.find("data:image/png;base64,") != std::string::npos, "svg gradient and image");
        // 5. pelo mesmo caminho do menu circular (jobs::Run): PDF vetorial, PNG em alta resolução, SVG e PDF de dois arquivos
        std::wstring dir = std::wstring(tmp) + L"vizu-cdr-jobs";
        CreateDirectoryW(dir.c_str(), nullptr);
        for (const wchar_t* n : {L"\\a.cdr", L"\\a.pdf", L"\\a.png", L"\\a.svg", L"\\b.cmx", L"\\a (+1).pdf"}) DeleteFileW((dir + n).c_str());
        CopyFileW(Fixture(L"fdo65220-2.cdr").c_str(), (dir + L"\\a.cdr").c_str(), FALSE);
        CopyFileW(Fixture(L"fdo63782-1.cmx").c_str(), (dir + L"\\b.cmx").c_str(), FALSE);
        auto run = [&](std::vector<std::wstring> in, ops::Format f) { jobs::Job j; j.format = f; j.dpi = 72; return jobs::Run(in, j, [](size_t, size_t) {}); };
        jobs::Result r = run({dir + L"\\a.cdr"}, ops::kPdf);
        Check(r.outputs.size() == 1 && r.outputs[0] == dir + L"\\a.pdf" && !r.failed, "job pdf");
        std::string made;
        { HANDLE h = CreateFileW(r.outputs[0].c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr); DWORD n = GetFileSize(h, nullptr), got; made.resize(n); ReadFile(h, &made[0], n, &got, nullptr); CloseHandle(h); }
        Check(made.find("/Producer (Vizu)") != std::string::npos && made.find("/Subtype /Image") == std::string::npos, "vector pdf, not an image");
        r = run({dir + L"\\a.cdr"}, ops::kPng);
        Check(r.outputs.size() == 1 && !r.failed, "job png");
        fmt::Info in; conv::FileInfo(r.outputs[0], &in);
        r = run({dir + L"\\a.cdr"}, ops::kSvg);
        Check(r.outputs.size() == 1 && r.outputs[0] == dir + L"\\a.svg" && !r.failed, "job svg without Inkscape");
        r = run({dir + L"\\a.cdr", dir + L"\\b.cmx"}, ops::kPdf);
        Check(r.outputs.size() == 1 && r.outputs[0] == dir + L"\\a (+1).pdf" && !r.failed, "two drawings merged in one pdf");
        pdfedit::File merged;
        Check(merged.Load(r.outputs[0]) && merged.Pages().size() == 2, "merged pdf has both pages");
        WIN32_FIND_DATAW fd; HANDLE fh = FindFirstFileW((dir + L"\\~vizu-*").c_str(), &fd);
        Check(fh == INVALID_HANDLE_VALUE, "temporary files removed");
        if (fh != INVALID_HANDLE_VALUE) FindClose(fh);
    } catch (const std::exception& ex) {
        std::cerr << "FALHOU: " << ex.what() << " (depois de " << checks << " verificações)\n";
        return 1;
    }
    std::cout << checks << " verificações de CorelDRAW passaram\n";
    return 0;
}
