// CdrVector.h - CorelDRAW (CDR) e CMX lidos de verdade, sem o Inkscape: PDF vetorial, SVG e imagens em alta resolução.
//
// A libcdr (LibreOffice, MPL 2.0) lê o arquivo e descreve cada página: caminhos, preenchimentos (cor, degradê),
// contornos, imagens e textos. Este arquivo recebe essa descrição (uma lista de desenho por página) e:
//  - escreve um PDF vetorial próprio (caminhos, degradês como sombreamento do PDF, transparência, imagens);
//  - escreve SVG (um arquivo por página);
//  - para PNG/JPG/WEBP/TIFF, desenha o PDF com o PDFium na resolução pedida.
// Os textos são diagramados com as fontes do Windows (DirectWrite) e viram curvas: a aparência fica exata no PDF e
// no SVG mesmo em outro computador (o texto não fica editável nesses arquivos).
// Não suportado (fica sem preenchimento ou com a cor base): preenchimento com bitmap/textura, sombras e efeitos.
#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <librevenge/librevenge.h>
#include <librevenge-stream/librevenge-stream.h>
#include <libcdr/libcdr.h>
#include <zlib.h>
#include "PdfPptx.h"
#include "engines/VectorSink.h"

namespace cdrv {
static const double M_PI_ = 3.14159265358979323846;

// ---------------- lista de desenho ----------------

struct Pt { double x = 0, y = 0; };                     // pontos, origem em cima à esquerda da página
struct Seg { char op; Pt p[3]; };                       // M p0 | L p0 | C p0 p1 p2 | Z
struct Stop { double offset; uint32_t rgb; double opacity; };
struct Paint {
    enum Kind { None, Solid, Linear, Radial } kind = None;
    uint32_t rgb = 0; double opacity = 1;
    std::vector<Stop> stops;
    double x1 = 0, y1 = 0, x2 = 1, y2 = 0;              // degradê linear, em unidades da caixa do objeto (y para baixo)
    double cx = .5, cy = .5, r = .5;                     // degradê radial, idem
};
struct Stroke { bool on = false; double width = 0.25; uint32_t rgb = 0; double opacity = 1; std::vector<double> dash; int cap = 0, join = 0; };
struct Item {
    bool image = false;
    std::vector<Seg> segs; bool evenOdd = true;
    Paint fill; Stroke stroke;
    int w = 0, h = 0; std::vector<uint8_t> rgba;         // imagem (alfa direto)
    Pt tl, tr, bl;                                       // cantos da imagem na página
};
struct Page { double w = 0, h = 0; std::vector<Item> items; };
struct Drawing { std::vector<Page> pages; };

inline bool IsCdr(const std::wstring& path) {
    const wchar_t* e = PathFindExtensionW(path.c_str());
    return e && (!_wcsicmp(e, L".cdr") || !_wcsicmp(e, L".cmx"));
}

// ---------------- auxiliares ----------------

inline double Inch(const librevenge::RVNGProperty* p) {
    if (!p) return 0;
    switch (p->getUnit()) {
    case librevenge::RVNG_POINT: return p->getDouble() / 72;
    case librevenge::RVNG_TWIP: return p->getDouble() / 1440;
    default: return p->getDouble();
    }
}
inline double Ptv(const librevenge::RVNGProperty* p) { return Inch(p) * 72; }
inline uint32_t Color(const librevenge::RVNGProperty* p, uint32_t fallback = 0) {
    if (!p) return fallback;
    std::string s = p->getStr().cstr();
    if (s.size() != 7 || s[0] != '#') return fallback;
    return (uint32_t)strtoul(s.c_str() + 1, nullptr, 16);
}
inline std::wstring Wide(const char* utf8) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], n);
    return w;
}
inline std::vector<uint8_t> Base64(const std::string& s) {
    std::vector<uint8_t> out; int val = 0, bits = -8;
    for (unsigned char c : s) {
        int d = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : -1;
        if (d < 0) continue;
        val = (val << 6) | d; bits += 6;
        if (bits >= 0) { out.push_back((uint8_t)(val >> bits)); bits -= 8; }
    }
    return out;
}
// Imagem (BMP, PNG, JPG...) -> RGBA com alfa direto, pelo WIC.
inline bool DecodeRgba(const std::vector<uint8_t>& data, int* w, int* h, std::vector<uint8_t>& rgba) {
    IWICImagingFactory* f = ops::Wic();
    if (!f) return false;
    bool ok = false;
    IWICStream* s = nullptr; IWICBitmapDecoder* d = nullptr; IWICBitmapFrameDecode* fr = nullptr; IWICFormatConverter* cv = nullptr;
    UINT W = 0, Hh = 0;
    if (SUCCEEDED(f->CreateStream(&s)) && SUCCEEDED(s->InitializeFromMemory((BYTE*)data.data(), (DWORD)data.size())) &&
        SUCCEEDED(f->CreateDecoderFromStream(s, nullptr, WICDecodeMetadataCacheOnDemand, &d)) && SUCCEEDED(d->GetFrame(0, &fr)) &&
        SUCCEEDED(f->CreateFormatConverter(&cv)) &&
        SUCCEEDED(cv->Initialize(fr, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(cv->GetSize(&W, &Hh)) && W && Hh && (UINT64)W * Hh <= 100000000ull) {
        rgba.resize((size_t)W * Hh * 4);
        ok = SUCCEEDED(cv->CopyPixels(nullptr, W * 4, (UINT)rgba.size(), rgba.data()));
        *w = (int)W; *h = (int)Hh;
    }
    if (cv) cv->Release(); if (fr) fr->Release(); if (d) d->Release(); if (s) s->Release();
    f->Release();
    return ok;
}

// ---------------- fontes e texto (DirectWrite) ----------------

struct Face { IDWriteFontFace* face = nullptr; DWRITE_FONT_METRICS m{}; };

class Fonts {
public:
    ~Fonts() { for (auto& f : cache_) if (f.second.face) f.second.face->Release(); if (coll_) coll_->Release(); if (dw_) dw_->Release(); }
    // Fonte para o nome do CDR ("Arial", "Arial Bold"...); se não estiver instalada, Arial.
    Face Get(const std::wstring& name) {
        auto it = cache_.find(name);
        if (it != cache_.end()) return it->second;
        Face f = Open(name);
        for (const wchar_t* alt : {L"Arial", L"Segoe UI", L"Tahoma", L"Microsoft Sans Serif"}) // fonte que falta: a mais comum
            if (!f.face && name != alt) f = Open(alt);
        return cache_[name] = f;
    }
private:
    IDWriteFactory* dw_ = nullptr;
    IDWriteFontCollection* coll_ = nullptr;
    std::map<std::wstring, Face> cache_;
    Face Open(std::wstring name) {
        Face out;
        if (!coll_) {
            if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dw_)) ||
                FAILED(dw_->GetSystemFontCollection(&coll_, FALSE))) return out;
        }
        DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL; DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
        UINT32 idx = 0; BOOL exists = FALSE;
        for (int tries = 0; tries < 4 && !exists; ++tries) {
            if (FAILED(coll_->FindFamilyName(name.c_str(), &idx, &exists))) return out;
            if (exists) break;
            size_t sp = name.find_last_of(L' '); // "Arial Bold Italic" -> "Arial" + estilo
            if (sp == std::wstring::npos) return out;
            std::wstring word = pdfppt::LowerW(name.substr(sp + 1));
            if (word == L"bold" || word == L"black" || word == L"heavy") weight = DWRITE_FONT_WEIGHT_BOLD;
            else if (word == L"italic" || word == L"oblique") style = DWRITE_FONT_STYLE_ITALIC;
            else if (word != L"regular" && word != L"normal" && word != L"medium" && word != L"light") return out;
            name.resize(sp);
        }
        IDWriteFontFamily* fam = nullptr; IDWriteFont* font = nullptr;
        if (SUCCEEDED(coll_->GetFontFamily(idx, &fam)) && SUCCEEDED(fam->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL, style, &font)) &&
            SUCCEEDED(font->CreateFontFace(&out.face)))
            out.face->GetMetrics(&out.m);
        if (font) font->Release(); if (fam) fam->Release();
        return out;
    }
};

// Recebe os contornos das letras (coordenadas com y para baixo) e acrescenta segmentos ao item.
class OutlineSink : public ID2D1SimplifiedGeometrySink {
public:
    OutlineSink(std::vector<Seg>& out, double ox, double oy) : out_(out), ox_(ox), oy_(oy) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** p) override {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(ID2D1SimplifiedGeometrySink)) { *p = this; return S_OK; }
        *p = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    void STDMETHODCALLTYPE SetFillMode(D2D1_FILL_MODE) override {}
    void STDMETHODCALLTYPE SetSegmentFlags(D2D1_PATH_SEGMENT) override {}
    void STDMETHODCALLTYPE BeginFigure(D2D1_POINT_2F p, D2D1_FIGURE_BEGIN) override { out_.push_back({'M', {P(p)}}); }
    void STDMETHODCALLTYPE AddLines(const D2D1_POINT_2F* p, UINT32 n) override { for (UINT32 i = 0; i < n; ++i) out_.push_back({'L', {P(p[i])}}); }
    void STDMETHODCALLTYPE AddBeziers(const D2D1_BEZIER_SEGMENT* b, UINT32 n) override {
        for (UINT32 i = 0; i < n; ++i) out_.push_back({'C', {P(b[i].point1), P(b[i].point2), P(b[i].point3)}});
    }
    void STDMETHODCALLTYPE EndFigure(D2D1_FIGURE_END) override { out_.push_back({'Z', {}}); }
    HRESULT STDMETHODCALLTYPE Close() override { return S_OK; }
private:
    std::vector<Seg>& out_; double ox_, oy_;
    Pt P(D2D1_POINT_2F p) const { return {ox_ + p.x, oy_ + p.y}; }
};

struct TextRun { std::wstring text, font = L"Arial"; double size = 12; uint32_t rgb = 0; };
struct TextLine { std::vector<TextRun> runs; };
struct TextPara { int align = 0; std::vector<TextLine> lines; }; // 0 esquerda, 1 centro, 2 direita

// ---------------- coletor (recebe a descrição da libcdr) ----------------

class Collector : public librevenge::RVNGDrawingInterface {
public:
    Drawing drawing;

    void startDocument(const librevenge::RVNGPropertyList&) override {}
    void endDocument() override {}
    void setDocumentMetaData(const librevenge::RVNGPropertyList&) override {}
    void defineEmbeddedFont(const librevenge::RVNGPropertyList&) override {}
    void startPage(const librevenge::RVNGPropertyList& p) override {
        Page pg;
        pg.w = p["svg:width"] ? Ptv(p["svg:width"]) : 612; pg.h = p["svg:height"] ? Ptv(p["svg:height"]) : 792;
        if (p["librevenge:master-page-name"]) {
            auto m = masters_.find(p["librevenge:master-page-name"]->getStr().cstr());
            if (m != masters_.end()) pg.items = m->second.items;
        }
        drawing.pages.push_back(pg);
        page_ = &drawing.pages.back();
    }
    void endPage() override { page_ = nullptr; }
    void startMasterPage(const librevenge::RVNGPropertyList& p) override {
        master_ = p["librevenge:master-page-name"] ? p["librevenge:master-page-name"]->getStr().cstr() : "";
        masters_[master_] = Page();
        page_ = &masters_[master_];
    }
    void endMasterPage() override { page_ = nullptr; master_.clear(); }
    void setStyle(const librevenge::RVNGPropertyList& p) override { style_ = p; }
    void startLayer(const librevenge::RVNGPropertyList&) override {}
    void endLayer() override {}
    void startEmbeddedGraphics(const librevenge::RVNGPropertyList&) override {}
    void endEmbeddedGraphics() override {}
    void openGroup(const librevenge::RVNGPropertyList&) override {}
    void closeGroup() override {}

    void drawRectangle(const librevenge::RVNGPropertyList& p) override {
        if (!p["svg:x"] || !p["svg:y"] || !p["svg:width"] || !p["svg:height"]) return;
        double x = Ptv(p["svg:x"]), y = Ptv(p["svg:y"]), w = Ptv(p["svg:width"]), h = Ptv(p["svg:height"]);
        std::vector<Seg> s = {{'M', {{x, y}}}, {'L', {{x + w, y}}}, {'L', {{x + w, y + h}}}, {'L', {{x, y + h}}}, {'Z', {}}};
        Shape(s);
    }
    void drawEllipse(const librevenge::RVNGPropertyList& p) override {
        if (!p["svg:cx"] || !p["svg:cy"] || !p["svg:rx"] || !p["svg:ry"]) return;
        double cx = Ptv(p["svg:cx"]), cy = Ptv(p["svg:cy"]), rx = Ptv(p["svg:rx"]), ry = Ptv(p["svg:ry"]);
        double a = p["librevenge:rotate"] ? -p["librevenge:rotate"]->getDouble() * M_PI_ / 180 : 0, k = 0.5522847498;
        auto at = [&](double x, double y) { return Pt{cx + x * cos(a) - y * sin(a), cy + x * sin(a) + y * cos(a)}; };
        std::vector<Seg> s = {{'M', {at(rx, 0)}},
                              {'C', {at(rx, k * ry), at(k * rx, ry), at(0, ry)}}, {'C', {at(-k * rx, ry), at(-rx, k * ry), at(-rx, 0)}},
                              {'C', {at(-rx, -k * ry), at(-k * rx, -ry), at(0, -ry)}}, {'C', {at(k * rx, -ry), at(rx, -k * ry), at(rx, 0)}}, {'Z', {}}};
        Shape(s);
    }
    void drawPolygon(const librevenge::RVNGPropertyList& p) override { Poly(p, true); }
    void drawPolyline(const librevenge::RVNGPropertyList& p) override { Poly(p, false); }
    void drawPath(const librevenge::RVNGPropertyList& p) override {
        const librevenge::RVNGPropertyListVector* d = p.child("svg:d");
        if (!d) return;
        std::vector<Seg> s; Pt cur, start;
        for (unsigned long i = 0; i < d->count(); ++i) {
            const librevenge::RVNGPropertyList& e = (*d)[i];
            if (!e["librevenge:path-action"]) continue;
            std::string act = e["librevenge:path-action"]->getStr().cstr();
            if (act.size() != 1) continue;
            char c = act[0];
            Pt to{e["svg:x"] ? Ptv(e["svg:x"]) : cur.x, e["svg:y"] ? Ptv(e["svg:y"]) : cur.y};
            if (c == 'M') { s.push_back({'M', {to}}); cur = start = to; }
            else if (c == 'L' || c == 'H' || c == 'V' || c == 'T') { s.push_back({'L', {to}}); cur = to; }
            else if (c == 'C' && e["svg:x1"] && e["svg:y1"] && e["svg:x2"] && e["svg:y2"]) {
                s.push_back({'C', {{Ptv(e["svg:x1"]), Ptv(e["svg:y1"])}, {Ptv(e["svg:x2"]), Ptv(e["svg:y2"])}, to}}); cur = to;
            } else if ((c == 'Q' || c == 'S') && e["svg:x1"] && e["svg:y1"]) {
                Pt q{Ptv(e["svg:x1"]), Ptv(e["svg:y1"])};
                s.push_back({'C', {{cur.x + 2 * (q.x - cur.x) / 3, cur.y + 2 * (q.y - cur.y) / 3}, {to.x + 2 * (q.x - to.x) / 3, to.y + 2 * (q.y - to.y) / 3}, to}});
                cur = to;
            } else if (c == 'A' && e["svg:rx"] && e["svg:ry"]) {
                Arc(s, cur, Ptv(e["svg:rx"]), Ptv(e["svg:ry"]), e["librevenge:rotate"] ? e["librevenge:rotate"]->getDouble() : 0,
                    e["librevenge:large-arc"] ? e["librevenge:large-arc"]->getInt() != 0 : true, e["librevenge:sweep"] ? e["librevenge:sweep"]->getInt() != 0 : true, to);
                cur = to;
            } else if (c == 'Z') { s.push_back({'Z', {}}); cur = start; }
        }
        Shape(s);
    }
    void drawGraphicObject(const librevenge::RVNGPropertyList& p) override {
        if (!page_ || !p["office:binary-data"] || !p["svg:x"] || !p["svg:y"] || !p["svg:width"] || !p["svg:height"]) return;
        Item it; it.image = true;
        if (!DecodeRgba(Base64(p["office:binary-data"]->getStr().cstr()), &it.w, &it.h, it.rgba)) return;
        double x = Ptv(p["svg:x"]), y = Ptv(p["svg:y"]), w = Ptv(p["svg:width"]), h = Ptv(p["svg:height"]);
        bool fx = p["draw:mirror-horizontal"] && p["draw:mirror-horizontal"]->getInt(), fy = p["draw:mirror-vertical"] && p["draw:mirror-vertical"]->getInt();
        double a = p["librevenge:rotate"] ? p["librevenge:rotate"]->getDouble() * M_PI_ / 180 : 0, cx = x + w / 2, cy = y + h / 2;
        auto T = [&](double px, double py) { // como o SVG: translate(c) scale(flip) rotate(a) translate(-c)
            double dx = px - cx, dy = py - cy, rx = dx * cos(a) - dy * sin(a), ry = dx * sin(a) + dy * cos(a);
            return Pt{cx + (fx ? -rx : rx), cy + (fy ? -ry : ry)};
        };
        it.tl = T(x, y); it.tr = T(x + w, y); it.bl = T(x, y + h);
        page_->items.push_back(std::move(it));
    }
    void drawConnector(const librevenge::RVNGPropertyList&) override {}

    // texto: guarda parágrafos, linhas e trechos; no fim diagrama e converte em curvas
    void startTextObject(const librevenge::RVNGPropertyList& p) override {
        tx_ = p["svg:x"] ? Ptv(p["svg:x"]) : 0; ty_ = p["svg:y"] ? Ptv(p["svg:y"]) : 0;
        tw_ = p["svg:width"] ? Ptv(p["svg:width"]) : 0; th_ = p["svg:height"] ? Ptv(p["svg:height"]) : 0;
        paras_.clear(); inText_ = true;
    }
    void endTextObject() override { if (page_) LayoutText(); paras_.clear(); inText_ = false; }
    void openParagraph(const librevenge::RVNGPropertyList& p) override {
        TextPara para;
        std::string a = p["fo:text-align"] ? p["fo:text-align"]->getStr().cstr() : "left";
        para.align = a == "center" ? 1 : (a == "end" || a == "right") ? 2 : 0;
        para.lines.push_back({});
        paras_.push_back(para);
    }
    void closeParagraph() override {}
    void defineCharacterStyle(const librevenge::RVNGPropertyList& p) override { if (p["librevenge:span-id"]) spans_[p["librevenge:span-id"]->getInt()] = p; }
    void openSpan(const librevenge::RVNGPropertyList& p0) override {
        librevenge::RVNGPropertyList p = p0;
        if (p0["librevenge:span-id"] && spans_.count(p0["librevenge:span-id"]->getInt())) p = spans_[p0["librevenge:span-id"]->getInt()];
        span_ = TextRun();
        if (p["style:font-name"]) span_.font = Wide(p["style:font-name"]->getStr().cstr());
        if (p["fo:font-size"]) span_.size = p["fo:font-size"]->getUnit() == librevenge::RVNG_POINT ? p["fo:font-size"]->getDouble() : Ptv(p["fo:font-size"]);
        span_.rgb = Color(p["fo:color"], 0);
        if (p["fo:font-weight"] && std::string(p["fo:font-weight"]->getStr().cstr()) == "bold" && span_.font.find(L"Bold") == std::wstring::npos) span_.font += L" Bold";
        if (p["fo:font-style"] && std::string(p["fo:font-style"]->getStr().cstr()) == "italic" && span_.font.find(L"Italic") == std::wstring::npos) span_.font += L" Italic";
    }
    void closeSpan() override {}
    void insertText(const librevenge::RVNGString& t) override { Add(Wide(t.cstr())); }
    void insertSpace() override { Add(L" "); }
    void insertTab() override { Add(L"\t"); }
    void insertLineBreak() override { if (!paras_.empty()) paras_.back().lines.push_back({}); }
    void insertField(const librevenge::RVNGPropertyList&) override {}
    void openLink(const librevenge::RVNGPropertyList&) override {}
    void closeLink() override {}
    void defineParagraphStyle(const librevenge::RVNGPropertyList&) override {}
    void openOrderedListLevel(const librevenge::RVNGPropertyList&) override {}
    void openUnorderedListLevel(const librevenge::RVNGPropertyList&) override {}
    void closeOrderedListLevel() override {}
    void closeUnorderedListLevel() override {}
    void openListElement(const librevenge::RVNGPropertyList& p) override { openParagraph(p); }
    void closeListElement() override {}
    void startTableObject(const librevenge::RVNGPropertyList&) override {}
    void openTableRow(const librevenge::RVNGPropertyList&) override {}
    void closeTableRow() override {}
    void openTableCell(const librevenge::RVNGPropertyList&) override {}
    void closeTableCell() override {}
    void insertCoveredTableCell(const librevenge::RVNGPropertyList&) override {}
    void endTableObject() override {}

private:
    Page* page_ = nullptr;
    std::map<std::string, Page> masters_;
    std::string master_;
    librevenge::RVNGPropertyList style_;
    std::map<int, librevenge::RVNGPropertyList> spans_;
    std::vector<TextPara> paras_;
    TextRun span_;
    bool inText_ = false;
    double tx_ = 0, ty_ = 0, tw_ = 0, th_ = 0;
    Fonts fonts_;

    void Add(const std::wstring& t) {
        if (!inText_) return;
        if (paras_.empty()) { paras_.push_back({}); paras_.back().lines.push_back({}); }
        auto& runs = paras_.back().lines.back().runs;
        if (!runs.empty() && runs.back().font == span_.font && runs.back().size == span_.size && runs.back().rgb == span_.rgb) runs.back().text += t;
        else { TextRun r = span_; r.text = t; runs.push_back(r); }
    }

    void Poly(const librevenge::RVNGPropertyList& p, bool closed) {
        const librevenge::RVNGPropertyListVector* v = p.child("svg:points");
        if (!v || v->count() < 2) return;
        std::vector<Seg> s;
        for (unsigned long i = 0; i < v->count(); ++i) {
            if (!(*v)[i]["svg:x"] || !(*v)[i]["svg:y"]) continue;
            s.push_back({s.empty() ? 'M' : 'L', {{Ptv((*v)[i]["svg:x"]), Ptv((*v)[i]["svg:y"])}}});
        }
        if (closed) s.push_back({'Z', {}});
        Shape(s);
    }

    // Arco do SVG (raios, rotação, flags) em curvas de Bézier.
    static void Arc(std::vector<Seg>& s, Pt p0, double rx, double ry, double rotDeg, bool large, bool sweep, Pt p1) {
        if (rx <= 0 || ry <= 0 || (p0.x == p1.x && p0.y == p1.y)) { s.push_back({'L', {p1}}); return; }
        double phi = rotDeg * M_PI_ / 180, cp = cos(phi), sp = sin(phi);
        double dx = (p0.x - p1.x) / 2, dy = (p0.y - p1.y) / 2, x1 = cp * dx + sp * dy, y1 = -sp * dx + cp * dy;
        double l = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry);
        if (l > 1) { rx *= sqrt(l); ry *= sqrt(l); }
        double num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1, den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
        double k = (large == sweep ? -1 : 1) * sqrt(std::max(0.0, num / den));
        double cxp = k * rx * y1 / ry, cyp = -k * ry * x1 / rx;
        double cx = cp * cxp - sp * cyp + (p0.x + p1.x) / 2, cy = sp * cxp + cp * cyp + (p0.y + p1.y) / 2;
        auto ang = [](double ux, double uy, double vx, double vy) { return atan2(ux * vy - uy * vx, ux * vx + uy * vy); };
        double t1 = ang(1, 0, (x1 - cxp) / rx, (y1 - cyp) / ry), dt = ang((x1 - cxp) / rx, (y1 - cyp) / ry, (-x1 - cxp) / rx, (-y1 - cyp) / ry);
        if (!sweep && dt > 0) dt -= 2 * M_PI_; else if (sweep && dt < 0) dt += 2 * M_PI_;
        int n = (int)std::ceil(std::fabs(dt) / (M_PI_ / 2));
        double step = dt / n, a = 4.0 / 3 * tan(step / 4);
        auto pt = [&](double t, double ox, double oy) { // ponto na elipse + deslocamento na direção da tangente
            double ex = rx * cos(t) + ox, ey = ry * sin(t) + oy;
            return Pt{cx + cp * ex - sp * ey, cy + sp * ex + cp * ey};
        };
        for (int i = 0; i < n; ++i) {
            double ta = t1 + i * step, tb = ta + step;
            Pt c1 = pt(ta, -a * rx * sin(ta), a * ry * cos(ta)), c2 = pt(tb, a * rx * sin(tb), -a * ry * cos(tb)), e = i == n - 1 ? p1 : pt(tb, 0, 0);
            s.push_back({'C', {c1, c2, e}});
        }
    }

    static Paint GradientFrom(const librevenge::RVNGPropertyList& st) {
        Paint g;
        std::string style = st["draw:style"] ? st["draw:style"]->getStr().cstr() : "linear";
        bool radial = style == "radial" || style == "rectangular" || style == "square" || style == "ellipsoid";
        const librevenge::RVNGPropertyListVector* v = st.child("svg:linearGradient");
        if (!v) v = st.child("svg:radialGradient");
        double so = st["librevenge:start-opacity"] ? st["librevenge:start-opacity"]->getDouble() : 1;
        double eo = st["librevenge:end-opacity"] ? st["librevenge:end-opacity"]->getDouble() : 1;
        if (v && v->count()) {
            for (unsigned long i = 0; i < v->count(); ++i) {
                const auto& s = (*v)[i];
                g.stops.push_back({s["svg:offset"] ? s["svg:offset"]->getDouble() : 0, Color(s["svg:stop-color"]), s["svg:stop-opacity"] ? s["svg:stop-opacity"]->getDouble() : 1});
            }
        } else if (st["draw:start-color"] && st["draw:end-color"]) {
            uint32_t a = Color(st["draw:start-color"]), b = Color(st["draw:end-color"]);
            if (radial) g.stops = {{0, b, eo}, {1, a, so}};
            else if (style == "axial") g.stops = {{0, b, eo}, {.5, a, so}, {1, b, eo}};
            else g.stops = {{0, a, so}, {1, b, eo}};
        }
        if (g.stops.empty()) return g;
        for (auto& s : g.stops) s.offset = std::min(1.0, std::max(0.0, s.offset));
        std::stable_sort(g.stops.begin(), g.stops.end(), [](const Stop& a, const Stop& b) { return a.offset < b.offset; });
        if (radial) {
            g.kind = Paint::Radial;
            auto frac = [](const librevenge::RVNGProperty* p, double d) { return p ? p->getDouble() : d; };
            g.cx = frac(st["svg:cx"] ? st["svg:cx"] : st["draw:cx"], .5);
            g.cy = frac(st["svg:cy"] ? st["svg:cy"] : st["draw:cy"], .5);
            g.r = st["svg:r"] ? st["svg:r"]->getDouble() : st["draw:border"] ? 1 - st["draw:border"]->getDouble() : 1;
            if (g.r <= 0) g.r = .01;
        } else {
            g.kind = Paint::Linear;
            double a = -(st["draw:angle"] ? st["draw:angle"]->getDouble() : 0);
            while (a < 0) a += 360;
            while (a >= 360) a -= 360;
            if (std::fabs(a - 270) < 1e-6) { g.x1 = 0; g.y1 = 0; g.x2 = 1; g.y2 = 0; }
            else { // vertical (0,0)->(0,1) girado em volta do centro, como o librevenge faz no SVG
                double r = a * M_PI_ / 180, c = cos(r), s = sin(r);
                auto rot = [&](double x, double y) { double dx = x - .5, dy = y - .5; return Pt{.5 + dx * c - dy * s, .5 + dx * s + dy * c}; };
                Pt p1 = rot(0, 0), p2 = rot(0, 1);
                g.x1 = p1.x; g.y1 = p1.y; g.x2 = p2.x; g.y2 = p2.y;
            }
        }
        return g;
    }

    void Styled(Item& it) {
        const auto& st = style_;
        std::string fill = st["draw:fill"] ? st["draw:fill"]->getStr().cstr() : "solid";
        it.fill.opacity = st["draw:opacity"] ? st["draw:opacity"]->getDouble() : 1;
        if (fill == "solid") { it.fill.kind = Paint::Solid; it.fill.rgb = Color(st["draw:fill-color"], 0); }
        else if (fill == "gradient") { double o = it.fill.opacity; it.fill = GradientFrom(st); it.fill.opacity = o; }
        else if (fill == "bitmap" && st["draw:fill-color"]) { it.fill.kind = Paint::Solid; it.fill.rgb = Color(st["draw:fill-color"]); }
        it.evenOdd = !st["svg:fill-rule"] || std::string(st["svg:fill-rule"]->getStr().cstr()) != "nonzero";
        std::string stroke = st["draw:stroke"] ? st["draw:stroke"]->getStr().cstr() : "none";
        if (stroke != "none") {
            it.stroke.on = true;
            it.stroke.rgb = Color(st["svg:stroke-color"], 0);
            it.stroke.width = st["svg:stroke-width"] ? Ptv(st["svg:stroke-width"]) : 1;
            if (it.stroke.width <= 0) it.stroke.width = 0.25; // linha fina do CorelDRAW (hairline)
            it.stroke.opacity = st["svg:stroke-opacity"] ? st["svg:stroke-opacity"]->getDouble() : 1;
            std::string cap = st["svg:stroke-linecap"] ? st["svg:stroke-linecap"]->getStr().cstr() : "butt";
            std::string join = st["svg:stroke-linejoin"] ? st["svg:stroke-linejoin"]->getStr().cstr() : "miter";
            it.stroke.cap = cap == "round" ? 1 : cap == "square" ? 2 : 0;
            it.stroke.join = join == "round" ? 1 : join == "bevel" ? 2 : 0;
            if (stroke == "dash") {
                int dots = st["draw:dots1"] ? st["draw:dots1"]->getInt() : 1;
                auto len = [&](const librevenge::RVNGProperty* p, double d) {
                    if (!p) return d;
                    return p->getUnit() == librevenge::RVNG_PERCENT ? p->getDouble() * it.stroke.width : Ptv(p);
                };
                double dl = len(st["draw:dots1-length"], it.stroke.width), gap = len(st["draw:distance"], it.stroke.width);
                for (int i = 0; i < std::max(1, std::min(dots, 16)); ++i) { it.stroke.dash.push_back(std::max(0.01, dl)); it.stroke.dash.push_back(std::max(0.01, gap)); }
            }
        }
    }

    void Shape(std::vector<Seg>& s) {
        if (!page_ || s.empty()) return;
        Item it; it.segs = std::move(s);
        Styled(it);
        if (it.fill.kind == Paint::None && !it.stroke.on) return;
        page_->items.push_back(std::move(it));
    }

    // Diagrama o texto e converte as letras em curvas.
    void LayoutText() {
        struct Glyphs { Face f; std::vector<UINT16> ids; std::vector<FLOAT> adv; double size; uint32_t rgb; double width = 0; };
        double baseline = 0; bool first = true;
        for (auto& para : paras_) {
            for (auto& line : para.lines) {
                std::vector<Glyphs> runs; double width = 0, ascent = 0, height = 0;
                for (auto& r : line.runs) {
                    Glyphs g; g.f = fonts_.Get(r.font); g.size = r.size; g.rgb = r.rgb;
                    if (!g.f.face || r.size <= 0) continue;
                    double em = g.f.m.designUnitsPerEm ? g.f.m.designUnitsPerEm : 2048;
                    ascent = std::max(ascent, g.f.m.ascent / em * r.size);
                    height = std::max(height, (g.f.m.ascent + g.f.m.descent + g.f.m.lineGap) / em * r.size);
                    for (size_t i = 0; i < r.text.size(); ++i) {
                        UINT32 cp = r.text[i];
                        if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < r.text.size()) { cp = 0x10000 + ((cp - 0xD800) << 10) + (r.text[i + 1] - 0xDC00); ++i; }
                        bool tab = cp == L'\t';
                        if (tab) cp = L' ';
                        UINT16 id = 0; g.f.face->GetGlyphIndices(&cp, 1, &id);
                        DWRITE_GLYPH_METRICS gm{}; g.f.face->GetDesignGlyphMetrics(&id, 1, &gm, FALSE);
                        float adv = (float)(gm.advanceWidth / em * r.size) * (tab ? 4 : 1);
                        g.ids.push_back(id); g.adv.push_back(adv); g.width += adv;
                    }
                    width += g.width;
                    runs.push_back(g);
                }
                if (height <= 0) height = 12;
                if (first) { baseline = th_ > 0.5 ? ty_ + ascent : ty_; first = false; } // caixa de texto: topo; texto artístico: linha de base
                else baseline += height;
                double x = para.align == 1 ? tx_ + (tw_ - width) / 2 : para.align == 2 ? tx_ + tw_ - width : tx_;
                for (auto& g : runs) {
                    if (!g.ids.empty()) {
                        Item it; it.evenOdd = false; it.fill.kind = Paint::Solid; it.fill.rgb = g.rgb;
                        OutlineSink sink(it.segs, x, baseline);
                        g.f.face->GetGlyphRunOutline((FLOAT)g.size, g.ids.data(), g.adv.data(), nullptr, (UINT32)g.ids.size(), FALSE, FALSE, &sink);
                        if (!it.segs.empty()) page_->items.push_back(std::move(it));
                    }
                    x += g.width;
                }
            }
        }
    }
};

// ---------------- leitura ----------------

// Lê o CDR/CMX. error recebe o motivo quando falha.
inline bool Read(const std::wstring& path, Drawing& out, std::wstring& error) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    LARGE_INTEGER size{};
    if (f == INVALID_HANDLE_VALUE || !GetFileSizeEx(f, &size) || size.QuadPart > (256LL << 20) || size.QuadPart < 16) {
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
        error = L"não foi possível ler o arquivo"; return false;
    }
    std::vector<unsigned char> data((size_t)size.QuadPart); DWORD got = 0;
    bool ok = ReadFile(f, data.data(), (DWORD)data.size(), &got, nullptr) && got == data.size();
    CloseHandle(f);
    if (!ok) { error = L"não foi possível ler o arquivo"; return false; }
    Collector c;
    bool parsed = false;
    try {
        librevenge::RVNGStringStream in(data.data(), (unsigned)data.size());
        if (libcdr::CDRDocument::isSupported(&in)) parsed = libcdr::CDRDocument::parse(&in, &c);
        else if (libcdr::CMXDocument::isSupported(&in)) parsed = libcdr::CMXDocument::parse(&in, &c);
        else { error = L"versão do CorelDRAW não suportada"; return false; }
    } catch (...) { parsed = false; }
    if (!parsed || c.drawing.pages.empty()) { error = L"o desenho não pôde ser lido (arquivo danificado ou versão não suportada)"; return false; }
    out = std::move(c.drawing);
    return true;
}

// ---------------- PDF ----------------

inline std::string F(double v) {
    char b[32]; snprintf(b, sizeof(b), "%.3f", std::fabs(v) < 1e-9 ? 0.0 : v);
    std::string s = b;
    while (s.size() > 1 && s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
    return s == "-0" ? "0" : s;
}
inline std::string Rgb(uint32_t c) { return F(((c >> 16) & 255) / 255.0) + " " + F(((c >> 8) & 255) / 255.0) + " " + F((c & 255) / 255.0); }
inline std::string Deflate(const std::string& in) {
    uLongf n = compressBound((uLong)in.size());
    std::string out(n, '\0');
    if (compress2((Bytef*)&out[0], &n, (const Bytef*)in.data(), (uLong)in.size(), 6) != Z_OK) return {};
    out.resize(n);
    return out;
}
inline void Bounds(const std::vector<Seg>& s, double* x0, double* y0, double* x1, double* y1) {
    *x0 = *y0 = 1e30; *x1 = *y1 = -1e30;
    for (auto& g : s) {
        int n = g.op == 'C' ? 3 : g.op == 'Z' ? 0 : 1;
        for (int i = 0; i < n; ++i) { *x0 = std::min(*x0, g.p[i].x); *y0 = std::min(*y0, g.p[i].y); *x1 = std::max(*x1, g.p[i].x); *y1 = std::max(*y1, g.p[i].y); }
    }
}

class PdfOut {
public:
    std::string Build(const Drawing& d) {
        objs_.clear();
        int catalog = Reserve(), pages = Reserve();
        std::vector<int> kids;
        for (auto& pg : d.pages) kids.push_back(PageObj(pg, pages));
        std::string k;
        for (int id : kids) k += std::to_string(id) + " 0 R ";
        Set(pages, "<< /Type /Pages /Count " + std::to_string(kids.size()) + " /Kids [" + k + "] >>");
        Set(catalog, "<< /Type /Catalog /Pages " + std::to_string(pages) + " 0 R >>");
        int info = Add("<< /Producer (Vizu) /Creator (Vizu - CorelDRAW para PDF) >>");
        std::string out = "%PDF-1.5\n%\xE2\xE3\xCF\xD3\n";
        std::vector<size_t> offs;
        for (size_t i = 0; i < objs_.size(); ++i) { offs.push_back(out.size()); out += std::to_string(i + 1) + " 0 obj\n" + objs_[i] + "\nendobj\n"; }
        size_t xref = out.size();
        out += "xref\n0 " + std::to_string(objs_.size() + 1) + "\n0000000000 65535 f \n";
        for (size_t o : offs) { char b[24]; snprintf(b, sizeof(b), "%010zu 00000 n \n", o); out += b; }
        out += "trailer\n<< /Size " + std::to_string(objs_.size() + 1) + " /Root " + std::to_string(catalog) + " 0 R /Info " + std::to_string(info) + " 0 R >>\nstartxref\n" + std::to_string(xref) + "\n%%EOF\n";
        return out;
    }
private:
    std::vector<std::string> objs_;
    int Reserve() { objs_.push_back(""); return (int)objs_.size(); }
    void Set(int id, const std::string& body) { objs_[id - 1] = body; }
    int Add(const std::string& body) { objs_.push_back(body); return (int)objs_.size(); }
    int Stream(const std::string& dict, const std::string& data) {
        std::string z = Deflate(data);
        return Add("<< " + dict + " /Filter /FlateDecode /Length " + std::to_string(z.size()) + " >>\nstream\n" + z + "\nendstream");
    }
    std::string Function(const std::vector<Stop>& st) {
        auto two = [](uint32_t a, uint32_t b) { return "<< /FunctionType 2 /Domain [0 1] /C0 [" + Rgb(a) + "] /C1 [" + Rgb(b) + "] /N 1 >>"; };
        std::vector<Stop> s = st;
        if (s.front().offset > 0) s.insert(s.begin(), {0, s.front().rgb, s.front().opacity});
        if (s.back().offset < 1) s.push_back({1, s.back().rgb, s.back().opacity});
        if (s.size() == 2) return two(s[0].rgb, s[1].rgb);
        std::string fns, bounds, enc;
        for (size_t i = 0; i + 1 < s.size(); ++i) {
            fns += two(s[i].rgb, s[i + 1].rgb) + " ";
            enc += "0 1 ";
            if (i + 2 < s.size()) bounds += F(std::min(1 - 1e-4, std::max(1e-4, s[i + 1].offset + i * 1e-6))) + " ";
        }
        return "<< /FunctionType 3 /Domain [0 1] /Functions [" + fns + "] /Bounds [" + bounds + "] /Encode [" + enc + "] >>";
    }
    int PageObj(const Page& pg, int parent) {
        std::string c, gs, sh, xo;
        std::map<std::string, std::string> alphas;
        auto alpha = [&](double fill, double stroke) -> std::string {
            if (fill >= 1 && stroke >= 1) return "";
            std::string key = F(fill) + "/" + F(stroke);
            if (!alphas.count(key)) {
                std::string name = "GS" + std::to_string(alphas.size() + 1);
                alphas[key] = name;
                gs += "/" + name + " " + std::to_string(Add("<< /Type /ExtGState /ca " + F(fill) + " /CA " + F(stroke) + " >>")) + " 0 R ";
            }
            return "/" + alphas[key] + " gs ";
        };
        double H = pg.h;
        int shN = 0, imN = 0;
        for (auto& it : pg.items) {
            if (it.image) {
                if (it.w <= 0 || it.h <= 0) continue;
                std::string rgb, a; bool hasAlpha = false;
                rgb.reserve((size_t)it.w * it.h * 3); a.reserve((size_t)it.w * it.h);
                for (size_t i = 0; i < it.rgba.size(); i += 4) { rgb += (char)it.rgba[i]; rgb += (char)it.rgba[i + 1]; rgb += (char)it.rgba[i + 2]; a += (char)it.rgba[i + 3]; hasAlpha |= it.rgba[i + 3] != 255; }
                std::string dims = "/Type /XObject /Subtype /Image /Width " + std::to_string(it.w) + " /Height " + std::to_string(it.h) + " /BitsPerComponent 8";
                std::string smask;
                if (hasAlpha) smask = " /SMask " + std::to_string(Stream(dims + " /ColorSpace /DeviceGray", a)) + " 0 R";
                int img = Stream(dims + " /ColorSpace /DeviceRGB" + smask, rgb);
                std::string name = "Im" + std::to_string(++imN);
                xo += "/" + name + " " + std::to_string(img) + " 0 R ";
                Pt br{it.tr.x + it.bl.x - it.tl.x, it.tr.y + it.bl.y - it.tl.y};
                // espaço da imagem: (0,0) = canto de baixo à esquerda, (0,1) = canto de cima
                c += "q " + F(br.x - it.bl.x) + " " + F(it.bl.y - br.y) + " " + F(it.tl.x - it.bl.x) + " " + F(it.bl.y - it.tl.y) + " " +
                     F(it.bl.x) + " " + F(H - it.bl.y) + " cm /" + name + " Do Q\n";
                continue;
            }
            std::string path;
            for (auto& s : it.segs) {
                if (s.op == 'M') path += F(s.p[0].x) + " " + F(H - s.p[0].y) + " m ";
                else if (s.op == 'L') path += F(s.p[0].x) + " " + F(H - s.p[0].y) + " l ";
                else if (s.op == 'C') path += F(s.p[0].x) + " " + F(H - s.p[0].y) + " " + F(s.p[1].x) + " " + F(H - s.p[1].y) + " " + F(s.p[2].x) + " " + F(H - s.p[2].y) + " c ";
                else path += "h ";
            }
            if (path.empty()) continue;
            if (it.fill.kind == Paint::Solid) c += "q " + alpha(it.fill.opacity, 1) + Rgb(it.fill.rgb) + " rg " + path + (it.evenOdd ? "f*" : "f") + " Q\n";
            else if (it.fill.kind == Paint::Linear || it.fill.kind == Paint::Radial) {
                double x0, y0, x1, y1;
                Bounds(it.segs, &x0, &y0, &x1, &y1);
                double op = 0; for (auto& s : it.fill.stops) op += s.opacity; op = op / it.fill.stops.size() * it.fill.opacity;
                if (x1 - x0 < 1e-3 || y1 - y0 < 1e-3) c += "q " + alpha(op, 1) + Rgb(it.fill.stops[0].rgb) + " rg " + path + (it.evenOdd ? "f*" : "f") + " Q\n";
                else {
                    std::string coords = it.fill.kind == Paint::Linear
                        ? "/ShadingType 2 /Coords [" + F(it.fill.x1) + " " + F(it.fill.y1) + " " + F(it.fill.x2) + " " + F(it.fill.y2) + "]"
                        : "/ShadingType 3 /Coords [" + F(it.fill.cx) + " " + F(it.fill.cy) + " 0 " + F(it.fill.cx) + " " + F(it.fill.cy) + " " + F(it.fill.r) + "]";
                    int shading = Add("<< " + coords + " /ColorSpace /DeviceRGB /Function " + Function(it.fill.stops) + " /Extend [true true] >>");
                    std::string name = "Sh" + std::to_string(++shN);
                    sh += "/" + name + " " + std::to_string(shading) + " 0 R ";
                    // recorta pelo caminho; a caixa do objeto vira o quadrado unitário (y para baixo, como no SVG)
                    c += "q " + alpha(op, 1) + path + (it.evenOdd ? "W* n " : "W n ") + F(x1 - x0) + " 0 0 " + F(-(y1 - y0)) + " " + F(x0) + " " + F(H - y0) + " cm /" + name + " sh Q\n";
                }
            }
            if (it.stroke.on) {
                std::string dash = "[";
                for (double d : it.stroke.dash) dash += F(d) + " ";
                c += "q " + alpha(1, it.stroke.opacity) + F(it.stroke.width) + " w " + std::to_string(it.stroke.cap) + " J " + std::to_string(it.stroke.join) + " j " +
                     dash + "] 0 d " + Rgb(it.stroke.rgb) + " RG " + path + "S Q\n";
            }
        }
        int content = Stream("", c);
        std::string res = "<< /ProcSet [/PDF /ImageC /ImageB]";
        if (!gs.empty()) res += " /ExtGState << " + gs + ">>";
        if (!sh.empty()) res += " /Shading << " + sh + ">>";
        if (!xo.empty()) res += " /XObject << " + xo + ">>";
        res += " >>";
        return Add("<< /Type /Page /Parent " + std::to_string(parent) + " 0 R /MediaBox [0 0 " + F(pg.w) + " " + F(pg.h) + "] /Resources " + res +
                   " /Contents " + std::to_string(content) + " 0 R >>");
    }
};

inline std::string Pdf(const Drawing& d) { return PdfOut().Build(d); }

// ---------------- SVG ----------------

inline std::string Hex(uint32_t c) { char b[8]; snprintf(b, sizeof(b), "#%06X", c & 0xFFFFFF); return b; }

inline bool PngBytes(const Item& it, std::string& out) {
    std::vector<uint8_t> bgra(it.rgba.size());
    for (size_t i = 0; i < it.rgba.size(); i += 4) {
        uint8_t a = it.rgba[i + 3];
        bgra[i] = (uint8_t)(it.rgba[i + 2] * a / 255); bgra[i + 1] = (uint8_t)(it.rgba[i + 1] * a / 255); bgra[i + 2] = (uint8_t)(it.rgba[i] * a / 255); bgra[i + 3] = a;
    }
    BYTE* bits = nullptr;
    HBITMAP dib = fmt::CreateDib((UINT)it.w, (UINT)it.h, &bits);
    if (!dib) return false;
    memcpy(bits, bgra.data(), bgra.size());
    std::vector<BYTE> png;
    bool ok = ops::EncodedBytes(dib, ops::kPng, 1, png);
    DeleteObject(dib);
    if (ok) out.assign(png.begin(), png.end());
    return ok;
}
inline std::string B64(const std::string& in) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o; int val = 0, bits = -6;
    for (unsigned char c : in) { val = (val << 8) + c; bits += 8; while (bits >= 0) { o += t[(val >> bits) & 63]; bits -= 6; } }
    if (bits > -6) o += t[((val << 8) >> (bits + 8)) & 63];
    while (o.size() % 4) o += '=';
    return o;
}

inline std::string Svg(const Page& pg) {
    std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"1.1\" width=\"" +
                    F(pg.w) + "pt\" height=\"" + F(pg.h) + "pt\" viewBox=\"0 0 " + F(pg.w) + " " + F(pg.h) + "\">\n";
    int grad = 0;
    for (auto& it : pg.items) {
        if (it.image) {
            std::string png;
            if (!PngBytes(it, png)) continue;
            s += "<image x=\"0\" y=\"0\" width=\"1\" height=\"1\" preserveAspectRatio=\"none\" transform=\"matrix(" + F(it.tr.x - it.tl.x) + " " + F(it.tr.y - it.tl.y) + " " +
                 F(it.bl.x - it.tl.x) + " " + F(it.bl.y - it.tl.y) + " " + F(it.tl.x) + " " + F(it.tl.y) + ")\" xlink:href=\"data:image/png;base64," + B64(png) + "\"/>\n";
            continue;
        }
        std::string d;
        for (auto& g : it.segs) {
            if (g.op == 'M') d += "M" + F(g.p[0].x) + " " + F(g.p[0].y);
            else if (g.op == 'L') d += "L" + F(g.p[0].x) + " " + F(g.p[0].y);
            else if (g.op == 'C') d += "C" + F(g.p[0].x) + " " + F(g.p[0].y) + " " + F(g.p[1].x) + " " + F(g.p[1].y) + " " + F(g.p[2].x) + " " + F(g.p[2].y);
            else d += "Z";
        }
        std::string fill = "none";
        if (it.fill.kind == Paint::Solid) fill = Hex(it.fill.rgb);
        else if (it.fill.kind != Paint::None) {
            std::string id = "g" + std::to_string(++grad), stops;
            for (auto& st : it.fill.stops) stops += "<stop offset=\"" + F(st.offset) + "\" stop-color=\"" + Hex(st.rgb) + "\"" + (st.opacity < 1 ? " stop-opacity=\"" + F(st.opacity) + "\"" : "") + "/>";
            if (it.fill.kind == Paint::Linear)
                s += "<defs><linearGradient id=\"" + id + "\" x1=\"" + F(it.fill.x1) + "\" y1=\"" + F(it.fill.y1) + "\" x2=\"" + F(it.fill.x2) + "\" y2=\"" + F(it.fill.y2) + "\">" + stops + "</linearGradient></defs>\n";
            else
                s += "<defs><radialGradient id=\"" + id + "\" cx=\"" + F(it.fill.cx) + "\" cy=\"" + F(it.fill.cy) + "\" r=\"" + F(it.fill.r) + "\">" + stops + "</radialGradient></defs>\n";
            fill = "url(#" + id + ")";
        }
        s += "<path d=\"" + d + "\" fill=\"" + fill + "\"";
        if (it.fill.kind != Paint::None && it.fill.opacity < 1) s += " fill-opacity=\"" + F(it.fill.opacity) + "\"";
        if (it.fill.kind != Paint::None) s += std::string(" fill-rule=\"") + (it.evenOdd ? "evenodd" : "nonzero") + "\"";
        if (it.stroke.on) {
            static const char* caps[] = {"butt", "round", "square"}; static const char* joins[] = {"miter", "round", "bevel"};
            s += " stroke=\"" + Hex(it.stroke.rgb) + "\" stroke-width=\"" + F(it.stroke.width) + "\" stroke-linecap=\"" + caps[it.stroke.cap] + "\" stroke-linejoin=\"" + joins[it.stroke.join] + "\"";
            if (it.stroke.opacity < 1) s += " stroke-opacity=\"" + F(it.stroke.opacity) + "\"";
            if (!it.stroke.dash.empty()) { s += " stroke-dasharray=\""; for (size_t i = 0; i < it.stroke.dash.size(); ++i) s += (i ? " " : "") + F(it.stroke.dash[i]); s += "\""; }
        }
        s += "/>\n";
    }
    return s + "</svg>\n";
}

// ---------------- imagens (PDF desenhado pelo PDFium) ----------------

// Cada página escolhida (índices base zero) como bitmap BGRA pré-multiplicado com fundo transparente.
inline std::vector<HBITMAP> Raster(const Drawing& d, const std::vector<int>& pages, double dpi, std::wstring& error) {
    std::vector<HBITMAP> out;
    std::string pdf = Pdf(d);
    std::lock_guard<std::mutex> guard(pdfppt::Lock());
    pdfppt::Api& a = pdfppt::Engine();
    if (!a.ok) { error = a.error; return out; }
    pdfppt::H doc = a.LoadMemDocument64(pdf.data(), pdf.size(), nullptr);
    if (!doc) { error = L"falha ao desenhar o PDF gerado"; return out; }
    for (int i : pages) {
        if (i < 0 || i >= a.GetPageCount(doc)) continue;
        pdfppt::H page = a.LoadPage(doc, i);
        if (!page) continue;
        double s = dpi / 72, w = a.GetPageWidthF(page) * s, h = a.GetPageHeightF(page) * s;
        if (w * h > 100e6) { double k = std::sqrt(100e6 / (w * h)); w *= k; h *= k; }
        UINT W = std::max(1u, (UINT)std::lround(w)), Hh = std::max(1u, (UINT)std::lround(h));
        BYTE* bits = nullptr;
        HBITMAP dib = fmt::CreateDib(W, Hh, &bits);
        if (dib) {
            pdfppt::H bmp = a.BitmapCreateEx((int)W, (int)Hh, 4 /*FPDFBitmap_BGRA*/, bits, (int)W * 4);
            if (bmp) {
                a.BitmapFillRect(bmp, 0, 0, (int)W, (int)Hh, 0x00000000);
                a.RenderPageBitmap(bmp, page, 0, 0, (int)W, (int)Hh, 0, 0x01 /*FPDF_ANNOT*/);
                a.BitmapDestroy(bmp);
                for (size_t k = 0; k < (size_t)W * Hh * 4; k += 4) { // alfa direto -> pré-multiplicado
                    BYTE al = bits[k + 3];
                    if (al != 255) { bits[k] = (BYTE)(bits[k] * al / 255); bits[k + 1] = (BYTE)(bits[k + 1] * al / 255); bits[k + 2] = (BYTE)(bits[k + 2] * al / 255); }
                }
                out.push_back(dib);
            } else DeleteObject(dib);
        }
        a.ClosePage(page);
    }
    a.CloseDocument(doc);
    return out;
}

// ---------------- Integração com engines::VectorSink ----------------

namespace engines {

// Converte cdrv::Drawing para VectorSink (SVG/EPS/PDF/IDML)
inline void ConvertDrawingToVectorSink(const cdrv::Drawing& drawing, VectorSink* sink, bool flipY = true) {
    using namespace cdrv;

    for (size_t pageIdx = 0; pageIdx < drawing.pages.size(); ++pageIdx) {
        const Page& pg = drawing.pages[pageIdx];
        Size pageSize{ (float)pg.w, (float)pg.h };

        if (pageIdx == 0) {
            sink->BeginDocument(pageSize);
        }

        double H = pg.h;
        int gradId = 0;

        for (const Item& it : pg.items) {
            if (it.image) {
                if (it.w <= 0 || it.h <= 0) continue;

                EmbeddedBitmap bmp;
                bmp.size = { (float)it.w, (float)it.h };
                bmp.data = it.rgba;
                bmp.opacity = 1.0f;

                // Calcular matriz de transformação da imagem
                Pt br{ it.tr.x + it.bl.x - it.tl.x, it.tr.y + it.bl.y - it.tl.y };
                Matrix m;
                m.m11 = (float)(it.tr.x - it.tl.x);
                m.m12 = (float)(it.tr.y - it.tl.y);
                m.m21 = (float)(it.bl.x - it.tl.x);
                m.m22 = (float)(it.bl.y - it.tl.y);
                m.dx = (float)it.tl.x;
                m.dy = flipY ? (float)(H - it.bl.y) : (float)it.tl.y;

                sink->PushState();
                sink->MultiplyTransform(m);
                Rect destRect{ 0, 0, 1, 1 };
                sink->DrawBitmap(bmp, destRect);
                sink->PopState();
                continue;
            }

            // Converter segmentos para path
            sink->BeginPath();
            for (const Seg& s : it.segs) {
                if (s.op == 'M') {
                    float x = (float)s.p[0].x;
                    float y = flipY ? (float)(H - s.p[0].y) : (float)s.p[0].y;
                    sink->MoveTo({ x, y });
                } else if (s.op == 'L') {
                    float x = (float)s.p[0].x;
                    float y = flipY ? (float)(H - s.p[0].y) : (float)s.p[0].y;
                    sink->LineTo({ x, y });
                } else if (s.op == 'C') {
                    Point c1{ (float)s.p[0].x, flipY ? (float)(H - s.p[0].y) : (float)s.p[0].y };
                    Point c2{ (float)s.p[1].x, flipY ? (float)(H - s.p[1].y) : (float)s.p[1].y };
                    Point p{ (float)s.p[2].x, flipY ? (float)(H - s.p[2].y) : (float)s.p[2].y };
                    sink->CubicTo(c1, c2, p);
                } else if (s.op == 'Z') {
                    sink->ClosePath();
                }
            }

            // Configurar fill
            if (it.fill.kind == Paint::Solid) {
                sink->SetFillSolid(Color::FromUINT(it.fill.rgb, (float)it.fill.opacity));
            } else if (it.fill.kind == Paint::Linear) {
                LinearGradient lg;
                lg.start = { (float)(it.fill.x1 * (pg.w)), (float)(it.fill.y1 * (pg.h)) };
                lg.end = { (float)(it.fill.x2 * (pg.w)), (float)(it.fill.y2 * (pg.h)) };
                for (const Stop& st : it.fill.stops) {
                    lg.stops.push_back({ (float)st.offset, Color::FromUINT(st.rgb, (float)(st.opacity * it.fill.opacity)) });
                }
                sink->SetFillLinearGradient(lg);
            } else if (it.fill.kind == Paint::Radial) {
                RadialGradient rg;
                rg.center = { (float)(it.fill.cx * pg.w), (float)(it.fill.cy * pg.h) };
                rg.radiusX = (float)(it.fill.r * std::max(pg.w, pg.h));
                rg.radiusY = rg.radiusX;
                rg.focalPoint = rg.center;
                for (const Stop& st : it.fill.stops) {
                    rg.stops.push_back({ (float)st.offset, Color::FromUINT(st.rgb, (float)(st.opacity * it.fill.opacity)) });
                }
                sink->SetFillRadialGradient(rg);
            } else {
                sink->SetFillNone();
            }

            // Configurar stroke
            if (it.stroke.on) {
                StrokeStyle ss;
                ss.width = (float)it.stroke.width;
                ss.cap = it.stroke.cap == 1 ? StrokeStyle::LineCap::Round : (it.stroke.cap == 2 ? StrokeStyle::LineCap::Square : StrokeStyle::LineCap::Flat);
                ss.join = it.stroke.join == 1 ? StrokeStyle::LineJoin::Round : (it.stroke.join == 2 ? StrokeStyle::LineJoin::Bevel : StrokeStyle::LineJoin::Miter);
                ss.dashPattern.clear();
                for (double d : it.stroke.dash) ss.dashPattern.push_back((float)d);
                ss.dashOffset = 0;
                sink->SetStroke(ss, Color::FromUINT(it.stroke.rgb, (float)it.stroke.opacity));
            } else {
                sink->SetStrokeNone();
            }

            // Desenhar path
            sink->EndPath(it.fill.kind != Paint::None, it.stroke.on);
        }
    }

    sink->EndDocument();
}

} // namespace engines

} // namespace cdrv
