// PdfPptx.h - PDF -> PowerPoint (.pptx) com texto editável.
//
// Como funciona (mesma técnica do Vizu Conversion Engine):
//  1. O PDFium (motor de PDF do Chrome, BSD/Apache, embutido compactado no Vizu) lê cada letra com posição,
//     linha de base, tamanho, cor, fonte, negrito e itálico.
//  2. As letras viram linhas e as linhas viram parágrafos.
//  3. Os objetos de texto convertidos são removidos da página (inclusive dentro de Form XObjects) e a página é
//     desenhada sem eles: formas, tabelas, fotos e degradês ficam no fundo, sem letras duplicadas.
//  4. O texto volta como caixas de texto nativas do PowerPoint. Fundo de uma cor só vira a cor do slide.
// Texto girado, contornado, transparente ou sem Unicode fica no fundo (não aparece duas vezes).
// Página escaneada: camada de texto invisível ou OCR do Windows; cada linha só vira texto se o entorno for liso.
// Se algo falhar numa página, ela vira a imagem da página original. Sem o motor, todas as páginas viram imagem.
#pragma once
#include <windows.h>
#include <dwrite.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Convert.h"
#include "Ops.h"
#include "Ocr.h"

namespace pdfppt {
using fmt::Bytes;

// ---------------- PDFium (carregado sob demanda) ----------------

struct FS_RECTF { float left, top, right, bottom; };
struct FS_MATRIX { float a, b, c, d, e, f; };
typedef void* H;

struct Api {
    bool ok = false;
    std::wstring error;
    void (*InitLibrary)();
    H (*LoadMemDocument64)(const void*, size_t, const char*);
    void (*CloseDocument)(H);
    unsigned long (*GetLastError)();
    int (*GetPageCount)(H);
    H (*LoadPage)(H, int);
    void (*ClosePage)(H);
    float (*GetPageWidthF)(H);
    float (*GetPageHeightF)(H);
    int (*GetPageBoundingBox)(H, FS_RECTF*);
    int (*GetRotation)(H);
    H (*TextLoadPage)(H);
    void (*TextClosePage)(H);
    int (*TextCountChars)(H);
    unsigned (*TextGetUnicode)(H, int);
    int (*TextIsGenerated)(H, int);
    H (*TextGetTextObject)(H, int);
    int (*TextGetLooseCharBox)(H, int, FS_RECTF*);
    int (*TextGetCharOrigin)(H, int, double*, double*);
    float (*TextGetCharAngle)(H, int);
    double (*TextGetFontSize)(H, int);
    int (*TextGetFillColor)(H, int, unsigned*, unsigned*, unsigned*, unsigned*);
    int (*TextObjGetRenderMode)(H);
    int (*ObjGetMatrix)(H, FS_MATRIX*);
    H (*TextObjGetFont)(H);
    size_t (*FontGetBaseFontName)(H, char*, size_t);
    int (*FontGetWeight)(H);
    int (*FontGetItalicAngle)(H, int*);
    int (*PageCountObjects)(H);
    H (*PageGetObject)(H, int);
    int (*ObjGetType)(H);
    int (*FormCountObjects)(H);
    H (*FormGetObject)(H, unsigned long);
    int (*PageRemoveObject)(H, H);
    int (*FormRemoveObject)(H, H);
    void (*ObjDestroy)(H);
    H (*BitmapCreateEx)(int, int, int, void*, int);
    int (*BitmapFillRect)(H, int, int, int, int, unsigned long);
    void (*RenderPageBitmap)(H, H, int, int, int, int, int, int);
    void (*BitmapDestroy)(H);
};

inline std::mutex& Lock() { static std::mutex m; return m; } // o PDFium não é thread-safe

inline bool Bind(Api& a, HMODULE dll) {
    bool all = true;
    auto get = [&](auto& fn, const char* name) { fn = (std::remove_reference_t<decltype(fn)>)(void*)GetProcAddress(dll, name); all &= fn != nullptr; };
    get(a.InitLibrary, "FPDF_InitLibrary"); get(a.LoadMemDocument64, "FPDF_LoadMemDocument64");
    get(a.CloseDocument, "FPDF_CloseDocument"); get(a.GetLastError, "FPDF_GetLastError");
    get(a.GetPageCount, "FPDF_GetPageCount"); get(a.LoadPage, "FPDF_LoadPage"); get(a.ClosePage, "FPDF_ClosePage");
    get(a.GetPageWidthF, "FPDF_GetPageWidthF"); get(a.GetPageHeightF, "FPDF_GetPageHeightF");
    get(a.GetPageBoundingBox, "FPDF_GetPageBoundingBox"); get(a.GetRotation, "FPDFPage_GetRotation");
    get(a.TextLoadPage, "FPDFText_LoadPage"); get(a.TextClosePage, "FPDFText_ClosePage");
    get(a.TextCountChars, "FPDFText_CountChars"); get(a.TextGetUnicode, "FPDFText_GetUnicode");
    get(a.TextIsGenerated, "FPDFText_IsGenerated"); get(a.TextGetTextObject, "FPDFText_GetTextObject");
    get(a.TextGetLooseCharBox, "FPDFText_GetLooseCharBox"); get(a.TextGetCharOrigin, "FPDFText_GetCharOrigin");
    get(a.TextGetCharAngle, "FPDFText_GetCharAngle"); get(a.TextGetFontSize, "FPDFText_GetFontSize");
    get(a.TextGetFillColor, "FPDFText_GetFillColor"); get(a.TextObjGetRenderMode, "FPDFTextObj_GetTextRenderMode");
    get(a.ObjGetMatrix, "FPDFPageObj_GetMatrix"); get(a.TextObjGetFont, "FPDFTextObj_GetFont");
    get(a.FontGetBaseFontName, "FPDFFont_GetBaseFontName"); get(a.FontGetWeight, "FPDFFont_GetWeight");
    get(a.FontGetItalicAngle, "FPDFFont_GetItalicAngle"); get(a.PageCountObjects, "FPDFPage_CountObjects");
    get(a.PageGetObject, "FPDFPage_GetObject"); get(a.ObjGetType, "FPDFPageObj_GetType");
    get(a.FormCountObjects, "FPDFFormObj_CountObjects"); get(a.FormGetObject, "FPDFFormObj_GetObject");
    get(a.PageRemoveObject, "FPDFPage_RemoveObject"); get(a.FormRemoveObject, "FPDFFormObj_RemoveObject");
    get(a.ObjDestroy, "FPDFPageObj_Destroy"); get(a.BitmapCreateEx, "FPDFBitmap_CreateEx");
    get(a.BitmapFillRect, "FPDFBitmap_FillRect"); get(a.RenderPageBitmap, "FPDF_RenderPageBitmap");
    get(a.BitmapDestroy, "FPDFBitmap_Destroy");
    return all;
}

// Versão do PDFium embutido (pypdfium2 5.15.2, win_amd64); a pasta muda junto com a versão.
static const wchar_t* kEngineFolder = L"pdfium-5.15.2";
static const int kEngineResource = 330;

// Extrai o pdfium.dll (recurso compactado com gzip) para %LOCALAPPDATA%\Vizu\<versão> e carrega.
// O arquivo em disco é conferido byte a byte contra o recurso antes de carregar.
inline HMODULE ExtractEmbedded(std::wstring& error) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(kEngineResource), MAKEINTRESOURCEW(10));
    HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
    const BYTE* p = g ? (const BYTE*)LockResource(g) : nullptr;
    DWORD n = r ? SizeofResource(nullptr, r) : 0;
    if (!p || !n) { error = L"motor de PDF não incluído nesta versão"; return nullptr; }
    Bytes gz(p, p + n), dll;
    if (!fmt::Gunzip(gz, dll) || dll.size() < 1024) { error = L"motor de PDF danificado"; return nullptr; }
    wchar_t base[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
    if (!len || len >= MAX_PATH) { error = L"pasta LOCALAPPDATA indisponível"; return nullptr; }
    std::wstring dir = std::wstring(base) + L"\\Vizu";
    CreateDirectoryW(dir.c_str(), nullptr);
    dir += L"\\"; dir += kEngineFolder;
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring path = dir + L"\\pdfium.dll";
    bool same = false;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size{};
        if (GetFileSizeEx(f, &size) && (UINT64)size.QuadPart == dll.size()) {
            Bytes disk(dll.size()); DWORD got = 0;
            same = ReadFile(f, disk.data(), (DWORD)disk.size(), &got, nullptr) && got == disk.size() && disk == dll;
        }
        CloseHandle(f);
    }
    if (!same) {
        std::wstring tmp = path + L".tmp";
        DeleteFileW(tmp.c_str()); // sobra de uma tentativa interrompida
        if (!ops::WriteFileBytes(tmp, dll.data(), dll.size()) || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            DeleteFileW(tmp.c_str());
            error = L"não foi possível preparar o motor de PDF";
            return nullptr;
        }
    }
    HMODULE m = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m) error = L"o Windows não carregou o motor de PDF";
    return m;
}

// Motor pronto para uso (uma vez por processo). dllPath vazio = o embutido no Vizu (os testes passam um caminho).
inline Api& Engine(const std::wstring& dllPath = L"") {
    static Api api;
    static bool tried = false;
    if (tried) return api;
    tried = true;
    HMODULE m = dllPath.empty() ? ExtractEmbedded(api.error) : LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m) { if (api.error.empty()) api.error = L"motor de PDF não encontrado"; return api; }
    if (!Bind(api, m)) { api.error = L"motor de PDF incompatível"; return api; }
    api.InitLibrary();
    api.ok = true;
    return api;
}

// ---------------- texto: letras, linhas, parágrafos ----------------

struct Char {
    std::wstring text;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, baseline = 0, size = 0; // pontos, origem em cima à esquerda
    COLORREF color = 0;
    std::wstring font = L"Arial";
    bool bold = false, italic = false, generated = false;
};
struct Run {
    std::wstring text, font;
    float size; COLORREF color; bool bold, italic;
    bool SameStyle(const Char& c) const {
        return font == c.font && std::fabs(size - c.size) < 0.25f && color == c.color && bold == c.bold && italic == c.italic;
    }
};
struct Line {
    std::vector<Run> runs;
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f, baseline = 0, size = 0, lastX1 = -1e30f;
    std::wstring Text() const { std::wstring t; for (auto& r : runs) t += r.text; return t; }
    void Add(const Char& c) {
        if (c.generated) {
            if (!runs.empty() && (runs.back().text.empty() || runs.back().text.back() != L' ')) runs.back().text += L' ';
            return;
        }
        if (!runs.empty() && runs.back().SameStyle(c)) runs.back().text += c.text;
        else runs.push_back({c.text, c.font, c.size, c.color, c.bold, c.italic});
        x0 = std::min(x0, c.x0); y0 = std::min(y0, c.y0); x1 = std::max(x1, c.x1); y1 = std::max(y1, c.y1);
        baseline = c.baseline; size = std::max(size, c.size); lastX1 = c.x1;
    }
    void Finish() {
        if (!runs.empty()) { auto& t = runs.back().text; while (!t.empty() && iswspace(t.back())) t.pop_back(); }
        runs.erase(std::remove_if(runs.begin(), runs.end(), [](const Run& r) { return r.text.empty(); }), runs.end());
    }
};
struct Block {
    std::vector<Line> lines;
    float X0() const { float v = 1e30f; for (auto& l : lines) v = std::min(v, l.x0); return v; }
    float Y0() const { float v = 1e30f; for (auto& l : lines) v = std::min(v, l.y0); return v; }
    float X1() const { float v = -1e30f; for (auto& l : lines) v = std::max(v, l.x1); return v; }
    float Y1() const { float v = -1e30f; for (auto& l : lines) v = std::max(v, l.y1); return v; }
    float Pitch() const { // distância média entre linhas de base (0 = uma linha só)
        if (lines.size() < 2) return 0;
        return (lines.back().baseline - lines.front().baseline) / (float)(lines.size() - 1);
    }
};

inline bool Blank(const std::wstring& s) { for (wchar_t c : s) if (!iswspace(c)) return false; return true; }

// Linha nova quando a linha de base muda, o texto volta para trás ou o espaço até a próxima letra é grande demais
// (colunas e células viram caixas separadas). Espaço inserido quando a distância passa de 25% do tamanho.
inline std::vector<Line> GroupLines(const std::vector<Char>& chars) {
    std::vector<Line> lines;
    Line cur; bool open = false;
    auto close = [&] { if (open && !cur.runs.empty()) lines.push_back(cur); cur = Line(); open = false; };
    for (const Char& c : chars) {
        if (c.generated) {
            if (c.text == L"\r" || c.text == L"\n" || c.text == L"\r\n") close();
            else if (open) cur.Add(c);
            continue;
        }
        if (Blank(c.text) && !open) continue;
        if (open && !cur.runs.empty()) {
            float size = std::max({cur.size, c.size, 1.0f}), gap = c.x0 - cur.lastX1;
            bool fresh = std::fabs(c.baseline - cur.baseline) > 0.35f * size || gap < -0.6f * size || gap > 2.2f * size;
            if (fresh) close();
            else if (gap > 0.25f * size && cur.runs.back().text.back() != L' ' && c.text != L" ") { Char sp; sp.generated = true; cur.Add(sp); }
        }
        if (!open) { if (Blank(c.text)) continue; open = true; }
        cur.Add(c);
    }
    close();
    std::vector<Line> out;
    for (auto& l : lines) { l.Finish(); if (!l.runs.empty()) out.push_back(l); }
    return out;
}

// Junta linhas que formam um parágrafo: mesma margem, tamanho parecido e espaçamento regular (procura entre os
// blocos recentes, porque colunas e células aparecem intercaladas na ordem do PDF).
inline std::vector<Block> GroupBlocks(const std::vector<Line>& lines) {
    std::vector<Block> blocks;
    for (const Line& line : lines) {
        bool joined = false;
        size_t from = blocks.size() > 12 ? blocks.size() - 12 : 0;
        for (size_t k = blocks.size(); k-- > from;) {
            Block& b = blocks[k];
            const Line& prev = b.lines.back();
            float size = std::max(prev.size, line.size), pitch = line.baseline - prev.baseline;
            bool sameLeft = std::fabs(line.x0 - prev.x0) < 0.6f * size;
            bool similar = std::fabs(line.size - prev.size) < 0.15f * size;
            float bp = b.Pitch();
            bool regular = pitch >= 0.9f * size && pitch <= 2.0f * size && (bp == 0 || std::fabs(pitch - bp) < 0.04f * size);
            if (sameLeft && similar && regular) { b.lines.push_back(line); joined = true; break; }
        }
        if (!joined) blocks.push_back(Block{{line}});
    }
    return blocks;
}

// ---------------- fontes ----------------

struct FontStyle { std::wstring family; bool bold, italic; };

inline std::wstring LowerW(std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
inline bool HasAny(const std::wstring& s, std::initializer_list<const wchar_t*> keys) {
    for (auto k : keys) if (s.find(k) != std::wstring::npos) return true;
    return false;
}

// "ABCDEF+TimesNewRomanPS-BoldItalicMT" -> Times New Roman, negrito, itálico.
inline FontStyle ParseFont(const std::string& raw, int weight, int italicAngle) {
    std::wstring name(raw.begin(), raw.end());
    if (name.size() > 7 && name[6] == L'+' && std::all_of(name.begin(), name.begin() + 6, [](wchar_t c) { return c >= L'A' && c <= L'Z'; }))
        name = name.substr(7);
    size_t cut = name.find_first_of(L"-,");
    std::wstring fam = name.substr(0, cut), style = cut == std::wstring::npos ? L"" : name.substr(cut + 1);
    for (const wchar_t* suf : {L"PSMT", L"PS", L"MT", L"Std", L"LT"}) {
        size_t n = wcslen(suf);
        if (fam.size() > n && fam.compare(fam.size() - n, n, suf) == 0) { fam.resize(fam.size() - n); break; }
    }
    std::wstring words; // TimesNewRoman -> Times New Roman; ABCSans -> ABC Sans
    for (size_t i = 0; i < fam.size(); ++i) {
        wchar_t c = fam[i];
        if (i && iswupper(c) && (iswlower(fam[i - 1]) || iswdigit(fam[i - 1]) || (iswupper(fam[i - 1]) && i + 1 < fam.size() && iswlower(fam[i + 1]))))
            words += L' ';
        words += c == L'_' ? L' ' : c;
    }
    std::wstring all = LowerW(style + words);
    all.erase(std::remove(all.begin(), all.end(), L' '), all.end());
    FontStyle fs;
    fs.bold = weight >= 600 || HasAny(all, {L"bold", L"black", L"heavy", L"semibold", L"demibold", L"demi", L"extrabold", L"ultrabold"});
    fs.italic = std::abs(italicAngle) > 0 || HasAny(all, {L"italic", L"oblique", L"ital"});
    std::wstring kept; // tira as palavras de estilo do nome da família
    size_t i = 0;
    while (i < words.size()) {
        size_t j = words.find(L' ', i); if (j == std::wstring::npos) j = words.size();
        std::wstring w = words.substr(i, j - i), lw = LowerW(w);
        bool styleWord = lw == L"bold" || lw == L"italic" || lw == L"oblique" || lw == L"black" || lw == L"heavy" ||
                         lw == L"semibold" || lw == L"regular" || lw == L"medium" || lw == L"light";
        if (!w.empty() && !styleWord) { if (!kept.empty()) kept += L' '; kept += w; }
        i = j + 1;
    }
    std::wstring key;
    for (wchar_t c : LowerW(kept)) if (c >= L'a' && c <= L'z') key += c;
    static const std::map<std::wstring, std::wstring> standard = {
        {L"helvetica", L"Arial"}, {L"arial", L"Arial"}, {L"arialmt", L"Arial"}, {L"times", L"Times New Roman"},
        {L"timesroman", L"Times New Roman"}, {L"timesnewroman", L"Times New Roman"}, {L"courier", L"Courier New"},
        {L"couriernew", L"Courier New"}, {L"symbol", L"Symbol"}, {L"zapfdingbats", L"Wingdings"}, {L"arialnarrow", L"Arial Narrow"}};
    auto it = standard.find(key);
    fs.family = it != standard.end() ? it->second : kept;
    if (fs.family.empty()) fs.family = L"Arial";
    return fs;
}

inline std::wstring Category(const std::wstring& family) {
    std::wstring l = LowerW(family);
    if (HasAny(l, {L"courier", L"mono", L"consol", L"code", L"menlo", L"typewriter"})) return L"Courier New";
    if (HasAny(l, {L"times", L"serif", L"georgia", L"garamond", L"roman", L"cambria", L"book", L"merriweather", L"playfair",
                   L"minion", L"baskerville", L"palatino", L"caslon", L"bodoni", L"didot", L"century", L"constantia", L"lora"}) &&
        l.find(L"sans") == std::wstring::npos)
        return L"Times New Roman";
    return L"Arial";
}

// Família instalada neste Windows? Se não estiver, a mais próxima da mesma categoria.
inline std::wstring ResolveFont(const std::wstring& family) {
    static std::map<std::wstring, std::wstring> cache;
    auto it = cache.find(family);
    if (it != cache.end()) return it->second;
    std::wstring out = family;
    IDWriteFactory* f = nullptr;
    if (SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&f))) {
        IDWriteFontCollection* fc = nullptr;
        if (SUCCEEDED(f->GetSystemFontCollection(&fc, FALSE))) {
            UINT32 idx = 0; BOOL exists = FALSE;
            if (SUCCEEDED(fc->FindFamilyName(family.c_str(), &idx, &exists)) && !exists) out = Category(family);
            fc->Release();
        }
        f->Release();
    }
    return cache[family] = out;
}

// ---------------- extração (PDFium) ----------------

struct TextObj { bool convertible = true; int mode = 0; float scale = 1; FontStyle style{L"Arial", false, false}; };

struct PageText {
    std::vector<Char> visible, invisible;
    std::unordered_map<H, TextObj> objs;
    std::set<H> Convertible() const { std::set<H> s; for (auto& o : objs) if (o.second.convertible && o.first) s.insert(o.first); return s; }
};

inline PageText Extract(Api& a, H page, float left, float top) {
    PageText out;
    H tp = a.TextLoadPage(page);
    if (!tp) return out;
    std::vector<std::pair<H, Char>> pending;
    int n = a.TextCountChars(tp);
    for (int i = 0; i < n; ++i) {
        unsigned code = a.TextGetUnicode(tp, i);
        Char ch;
        if (code >= 0x10000 && code < 0x110000) { code -= 0x10000; ch.text = {(wchar_t)(0xD800 + (code >> 10)), (wchar_t)(0xDC00 + (code & 0x3FF))}; code += 0x10000; }
        else ch.text = std::wstring(1, code && code < 0x110000 ? (wchar_t)code : (wchar_t)0xFFFD);
        if (a.TextIsGenerated(tp, i)) { ch.generated = true; pending.push_back({nullptr, ch}); continue; }
        H obj = a.TextGetTextObject(tp, i);
        auto found = out.objs.find(obj);
        if (found == out.objs.end()) {
            TextObj t;
            if (!obj) t.convertible = false;
            else {
                t.mode = a.TextObjGetRenderMode(obj);
                if (t.mode != 0) t.convertible = false; // 3 = invisível (OCR); outros: contorno, máscara
                H font = a.TextObjGetFont(obj);
                char name[512] = {}; int angle = 0;
                if (font) { a.FontGetBaseFontName(font, name, sizeof(name)); a.FontGetItalicAngle(font, &angle); }
                t.style = ParseFont(name, font ? std::max(0, a.FontGetWeight(font)) : 0, angle);
                FS_MATRIX m{};
                if (a.ObjGetMatrix(obj, &m)) {
                    float det = std::fabs(m.a * m.d - m.b * m.c);
                    t.scale = det > 0 ? std::sqrt(det) : 1;
                    if (std::fabs(m.b) > 1e-3f || std::fabs(m.c) > 1e-3f || m.a <= 0 || m.d <= 0) t.convertible = false; // girado/espelhado
                }
            }
            found = out.objs.emplace(obj, t).first;
        }
        TextObj& t = found->second;
        FS_RECTF r{};
        if (!a.TextGetLooseCharBox(tp, i, &r)) { t.convertible = false; continue; }
        double ox = 0, oy = 0;
        a.TextGetCharOrigin(tp, i, &ox, &oy);
        float angle = a.TextGetCharAngle(tp, i);
        float size = (float)a.TextGetFontSize(tp, i) * t.scale, loose = r.top - r.bottom;
        if (loose > 0 && size > 0 && (loose / size < 0.6f || loose / size > 2.2f)) size = loose / 1.15f; // escala escondida no Form XObject
        unsigned cr = 0, cg = 0, cb = 0, ca = 255;
        if (a.TextGetFillColor(tp, i, &cr, &cg, &cb, &ca)) { ch.color = RGB(cr, cg, cb); if (ca < 255) t.convertible = false; }
        if (t.convertible && ((angle > 1e-3f && angle < 6.282f) || ch.text[0] == 0xFFFD || (code < 32 && code != 9) || size < 1))
            t.convertible = false;
        ch.x0 = r.left - left; ch.x1 = r.right - left; ch.y0 = top - r.top; ch.y1 = top - r.bottom;
        ch.baseline = top - (float)oy; ch.size = size;
        ch.font = t.style.family; ch.bold = t.style.bold; ch.italic = t.style.italic;
        pending.push_back({obj, ch});
    }
    a.TextClosePage(tp);
    // decisão por objeto: inteiro em caixa de texto ou inteiro no fundo (nada aparece duas vezes)
    for (auto& [obj, ch] : pending) {
        if (ch.generated) { out.visible.push_back(ch); continue; }
        const TextObj& t = out.objs[obj];
        if (t.convertible) out.visible.push_back(ch);
        else if (t.mode == 3) out.invisible.push_back(ch);
        else { Char sp; sp.text = L" "; sp.generated = true; out.visible.push_back(sp); }
    }
    return out;
}

// Remove da página (e dos Form XObjects) os objetos de texto que viraram caixas.
inline int RemoveText(Api& a, H page, const std::set<H>& targets) {
    std::vector<std::pair<H, H>> found; // (form ou nulo, objeto)
    std::function<void(H, int)> walk = [&](H form, int depth) {
        int n = form ? a.FormCountObjects(form) : a.PageCountObjects(page);
        for (int i = 0; i < n; ++i) {
            H o = form ? a.FormGetObject(form, (unsigned long)i) : a.PageGetObject(page, i);
            if (!o) continue;
            int type = a.ObjGetType(o);
            if (type == 1 && targets.count(o)) found.push_back({form, o});
            else if (type == 5 && depth < 15) walk(o, depth + 1);
        }
    };
    walk(nullptr, 0);
    int removed = 0;
    for (auto& [form, o] : found)
        if (form ? a.FormRemoveObject(form, o) : a.PageRemoveObject(page, o)) { a.ObjDestroy(o); ++removed; }
    return removed;
}

// Página em BGRA (DIB de cima para baixo, fundo branco, com anotações).
inline HBITMAP Render(Api& a, H page, UINT w, UINT h) {
    BYTE* bits = nullptr;
    HBITMAP dib = fmt::CreateDib(w, h, &bits);
    if (!dib) return nullptr;
    H bmp = a.BitmapCreateEx((int)w, (int)h, 3 /*FPDFBitmap_BGRx*/, bits, (int)w * 4);
    if (!bmp) { DeleteObject(dib); return nullptr; }
    a.BitmapFillRect(bmp, 0, 0, (int)w, (int)h, 0xFFFFFFFF);
    a.RenderPageBitmap(bmp, page, 0, 0, (int)w, (int)h, 0, 0x01 /*FPDF_ANNOT*/);
    a.BitmapDestroy(bmp);
    for (size_t i = 3; i < (size_t)w * h * 4; i += 4) bits[i] = 255;
    return dib;
}

// ---------------- páginas escaneadas ----------------

struct Unit { std::wstring text; float x0, y0, x1, y1, size; }; // pontos

inline int Dist(const BYTE* p, const BYTE* q) { return std::max({std::abs(p[0] - q[0]), std::abs(p[1] - q[1]), std::abs(p[2] - q[2])}); }

// Apaga da imagem as linhas cujo entorno é liso (pinta com a cor do fundo) e devolve essas linhas como texto.
// Linhas sobre foto, textura ou desenho ficam na imagem.
inline std::vector<Block> ClearUnits(HBITMAP dib, const std::vector<Unit>& units, float ptToPx, int* keptAsImage) {
    std::vector<Block> blocks;
    BYTE* px; UINT W, Hh;
    if (!ops::Pixels(dib, &px, &W, &Hh)) return blocks;
    auto at = [&](int x, int y) { return px + ((size_t)y * W + x) * 4; };
    std::vector<RECT> erased;
    for (const Unit& u : units) {
        if (Blank(u.text)) continue;
        int pad = std::max(2, (int)std::ceil((u.y1 - u.y0) * ptToPx * 0.12f)), ring = 2;
        int l = (int)std::floor(u.x0 * ptToPx) - pad, t = (int)std::floor(u.y0 * ptToPx) - pad;
        int r = (int)std::ceil(u.x1 * ptToPx) + pad, b = (int)std::ceil(u.y1 * ptToPx) + pad;
        ++*keptAsImage;
        if (l - ring < 0 || t - ring < 0 || r + ring > (int)W || b + ring > (int)Hh) continue;
        bool overlap = false;
        for (auto& e : erased) overlap |= l < e.right && r > e.left && t < e.bottom && b > e.top;
        if (overlap) continue;
        std::vector<const BYTE*> ringPx;
        for (int y = t - ring; y < t; ++y) for (int x = l - ring; x < r + ring; ++x) ringPx.push_back(at(x, y));
        for (int y = b; y < b + ring; ++y) for (int x = l - ring; x < r + ring; ++x) ringPx.push_back(at(x, y));
        for (int y = t; y < b; ++y) { for (int x = l - ring; x < l; ++x) ringPx.push_back(at(x, y)); for (int x = r; x < r + ring; ++x) ringPx.push_back(at(x, y)); }
        BYTE bg[4] = {0, 0, 0, 255};
        for (int ch = 0; ch < 3; ++ch) {
            std::vector<BYTE> v; v.reserve(ringPx.size());
            for (auto p : ringPx) v.push_back(p[ch]);
            std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
            bg[ch] = v[v.size() / 2];
        }
        size_t uniform = 0;
        for (auto p : ringPx) uniform += Dist(p, bg) <= 12;
        if (uniform < ringPx.size() * 98 / 100) continue; // entorno não é liso
        std::vector<const BYTE*> ink;
        for (int y = t + pad; y < b - pad; ++y) for (int x = l + pad; x < r - pad; ++x) if (Dist(at(x, y), bg) >= 60) ink.push_back(at(x, y));
        if (ink.size() < 3) continue;
        BYTE col[3];
        for (int ch = 0; ch < 3; ++ch) {
            std::vector<BYTE> v; for (auto p : ink) v.push_back(p[ch]);
            std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
            col[ch] = v[v.size() / 2];
        }
        for (int y = t; y < b; ++y) for (int x = l; x < r; ++x) { BYTE* p = px + ((size_t)y * W + x) * 4; p[0] = bg[0]; p[1] = bg[1]; p[2] = bg[2]; }
        erased.push_back({l, t, r, b});
        Line line;
        line.runs.push_back({u.text, L"Arial", u.size, RGB(col[2], col[1], col[0]), false, false});
        line.x0 = u.x0; line.y0 = u.y0; line.x1 = u.x1; line.y1 = u.y1; line.baseline = u.y1; line.size = u.size;
        blocks.push_back(Block{{line}});
        --*keptAsImage;
    }
    return blocks;
}

// ---------------- PPTX ----------------

inline std::string Utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
inline std::string Xml(const std::wstring& w) {
    std::wstring e;
    for (wchar_t c : w) {
        if (c == L'&') e += L"&amp;"; else if (c == L'<') e += L"&lt;"; else if (c == L'>') e += L"&gt;"; else if (c == L'"') e += L"&quot;";
        else if (c < 32 && c != 9) e += L' '; else if (c == 0xFFFE || c == 0xFFFF) continue; else e += c;
    }
    return Utf8(e);
}
inline std::string Hex(COLORREF c) { char b[8]; snprintf(b, sizeof(b), "%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c)); return b; }
inline std::string Num(long long v) { return std::to_string(v); }

inline std::string Resource(int id) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
    const char* p = g ? (const char*)LockResource(g) : nullptr;
    return p ? std::string(p, SizeofResource(nullptr, r)) : std::string();
}

// Partes fixas do PPTX (tema, mestre e layout em branco do modelo do python-pptx, MIT), embutidas no Vizu.
struct Template { std::string master, layout, theme, textStyle; bool Ok() const { return !master.empty() && !layout.empty() && !theme.empty() && !textStyle.empty(); } };
inline Template& Parts() { static Template t{Resource(320), Resource(321), Resource(322), Resource(323)}; return t; }

static const char* kNs = "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"";
static const char* kHead = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
static const char* kRelNs = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/";

class Deck {
public:
    long long cx = 0, cy = 0; // EMU

    void Size(float wPt, float hPt) {
        double f = std::min(12700.0, 51206400.0 / std::max(wPt, hPt)); // até 56 polegadas (limite do PowerPoint)
        cx = std::max(914400LL, (long long)(wPt * f)); cy = std::max(914400LL, (long long)(hPt * f));
    }
    // Começa um slide para uma página de wPt x hPt (centralizada, sem distorcer, se o tamanho for outro).
    void Begin(float wPt, float hPt) {
        scale_ = std::min(cx / (double)wPt, cy / (double)hPt);
        ox_ = (cx - wPt * scale_) / 2; oy_ = (cy - hPt * scale_) / 2; wPt_ = wPt; hPt_ = hPt;
        bg_.clear(); shapes_.clear(); rels_.clear(); nextId_ = 2;
    }
    bool Letterboxed() const { return std::fabs(ox_) > 1 || std::fabs(oy_) > 1; }
    void Solid(COLORREF c) { bg_ = "<p:bg><p:bgPr><a:solidFill><a:srgbClr val=\"" + Hex(c) + "\"/></a:solidFill><a:effectLst/></p:bgPr></p:bg>"; }
    void Picture(const Bytes& data, bool jpeg) {
        std::string name = "image" + Num((long long)media_.size() + 1) + (jpeg ? ".jpeg" : ".png");
        media_.push_back({"ppt/media/" + name, data});
        std::string rid = "rId" + Num((long long)rels_.size() + 2);
        rels_ += "<Relationship Id=\"" + rid + "\" Type=\"" + kRelNs + "image\" Target=\"../media/" + name + "\"/>";
        long long id = nextId_++;
        shapes_ += "<p:pic><p:nvPicPr><p:cNvPr id=\"" + Num(id) + "\" name=\"Fundo " + Num(id) + "\"/><p:cNvPicPr><a:picLocks noChangeAspect=\"1\"/></p:cNvPicPr><p:nvPr/></p:nvPicPr>"
                   "<p:blipFill><a:blip r:embed=\"" + rid + "\"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>"
                   "<p:spPr>" + Xfrm(0, 0, wPt_, hPt_) + "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></p:spPr></p:pic>";
    }
    // Uma caixa de texto por bloco; uma linha do PDF = um parágrafo (as quebras do original ficam).
    int Text(const Block& b) {
        float size = 0;
        for (auto& l : b.lines) size = std::max(size, l.size);
        float pitch = b.Pitch(), top = b.Y0();
        if (pitch > 0) top = b.lines[0].baseline - 0.8f * pitch; // com espaçamento exato, a 1ª linha de base fica a ~0,8 dele
        float width = (b.X1() - b.X0()) * 1.06f + size * 0.6f; // folga: as métricas da fonte do Office diferem um pouco
        long long id = nextId_++;
        std::string sp = "<p:sp><p:nvSpPr><p:cNvPr id=\"" + Num(id) + "\" name=\"Texto " + Num(id) + "\"/><p:cNvSpPr txBox=\"1\"/><p:nvPr/></p:nvSpPr>"
                         "<p:spPr>" + Xfrm(b.X0(), top, width, b.Y1() - top) + "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom><a:noFill/></p:spPr>"
                         "<p:txBody><a:bodyPr wrap=\"none\" lIns=\"0\" tIns=\"0\" rIns=\"0\" bIns=\"0\" rtlCol=\"0\" anchor=\"t\"><a:noAutofit/></a:bodyPr><a:lstStyle/>";
        int runs = 0;
        for (auto& l : b.lines) {
            sp += "<a:p><a:pPr>";
            if (pitch > 0) sp += "<a:lnSpc><a:spcPts val=\"" + Num(std::max(1LL, std::llround(pitch * scale_ / 12700 * 100))) + "\"/></a:lnSpc>";
            sp += "<a:spcBef><a:spcPts val=\"0\"/></a:spcBef><a:spcAft><a:spcPts val=\"0\"/></a:spcAft></a:pPr>";
            for (auto& r : l.runs) {
                long long sz = std::max(100LL, std::llround(r.size * scale_ / 12700 * 2) * 50); // meio ponto
                std::string face = Xml(ResolveFont(r.font));
                sp += "<a:r><a:rPr lang=\"pt-BR\" sz=\"" + Num(sz) + "\" b=\"" + (r.bold ? "1" : "0") + "\" i=\"" + (r.italic ? "1" : "0") + "\" dirty=\"0\">"
                      "<a:solidFill><a:srgbClr val=\"" + Hex(r.color) + "\"/></a:solidFill><a:latin typeface=\"" + face + "\"/><a:cs typeface=\"" + face + "\"/></a:rPr>"
                      "<a:t>" + Xml(r.text) + "</a:t></a:r>";
                ++runs;
            }
            sp += "</a:p>";
        }
        shapes_ += sp + "</p:txBody></p:sp>";
        return runs;
    }
    void End() {
        slides_.push_back(std::string(kHead) + "<p:sld " + kNs + "><p:cSld>" + bg_ +
                          "<p:spTree><p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/><a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>" +
                          shapes_ + "</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sld>");
        slideRels_.push_back(std::string(kHead) + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"" +
                             kRelNs + "slideLayout\" Target=\"../slideLayouts/slideLayout1.xml\"/>" + rels_ + "</Relationships>");
    }
    size_t Slides() const { return slides_.size(); }

    bool Save(const std::wstring& path) const {
        const Template& t = Parts();
        if (!t.Ok() || slides_.empty()) return false;
        std::vector<std::pair<std::string, std::string>> files;
        std::string types = std::string(kHead) + "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Default Extension=\"png\" ContentType=\"image/png\"/><Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/>"
            "<Override PartName=\"/ppt/presentation.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml\"/>"
            "<Override PartName=\"/ppt/slideMasters/slideMaster1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml\"/>"
            "<Override PartName=\"/ppt/slideLayouts/slideLayout1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml\"/>"
            "<Override PartName=\"/ppt/theme/theme1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.theme+xml\"/>"
            "<Override PartName=\"/ppt/presProps.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.presProps+xml\"/>"
            "<Override PartName=\"/ppt/viewProps.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.viewProps+xml\"/>"
            "<Override PartName=\"/ppt/tableStyles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.tableStyles+xml\"/>"
            "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
            "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>";
        std::string ids, presRels;
        for (size_t i = 0; i < slides_.size(); ++i) {
            std::string n = Num((long long)i + 1);
            types += "<Override PartName=\"/ppt/slides/slide" + n + ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slide+xml\"/>";
            ids += "<p:sldId id=\"" + Num(256 + (long long)i) + "\" r:id=\"rId" + Num(10 + (long long)i) + "\"/>";
            presRels += "<Relationship Id=\"rId" + Num(10 + (long long)i) + "\" Type=\"" + kRelNs + "slide\" Target=\"slides/slide" + n + ".xml\"/>";
            files.push_back({"ppt/slides/slide" + n + ".xml", slides_[i]});
            files.push_back({"ppt/slides/_rels/slide" + n + ".xml.rels", slideRels_[i]});
        }
        types += "</Types>";
        std::string rels = std::string(kHead) + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
        files.insert(files.begin(), {
            {"[Content_Types].xml", types},
            {"_rels/.rels", rels + "<Relationship Id=\"rId1\" Type=\"" + kRelNs + "officeDocument\" Target=\"ppt/presentation.xml\"/>"
                                   "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
                                   "<Relationship Id=\"rId3\" Type=\"" + kRelNs + "extended-properties\" Target=\"docProps/app.xml\"/></Relationships>"},
            {"docProps/core.xml", std::string(kHead) + "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:creator>Vizu</dc:creator></cp:coreProperties>"},
            {"docProps/app.xml", std::string(kHead) + "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\"><Application>Vizu</Application><Slides>" + Num((long long)slides_.size()) + "</Slides></Properties>"},
            {"ppt/presentation.xml", std::string(kHead) + "<p:presentation " + kNs + " saveSubsetFonts=\"1\"><p:sldMasterIdLst><p:sldMasterId id=\"2147483648\" r:id=\"rId1\"/></p:sldMasterIdLst><p:sldIdLst>" + ids +
                                     "</p:sldIdLst><p:sldSz cx=\"" + Num(cx) + "\" cy=\"" + Num(cy) + "\"/><p:notesSz cx=\"6858000\" cy=\"9144000\"/>" + t.textStyle + "</p:presentation>"},
            {"ppt/_rels/presentation.xml.rels", rels + "<Relationship Id=\"rId1\" Type=\"" + kRelNs + "slideMaster\" Target=\"slideMasters/slideMaster1.xml\"/>"
                                                "<Relationship Id=\"rId2\" Type=\"" + kRelNs + "theme\" Target=\"theme/theme1.xml\"/>"
                                                "<Relationship Id=\"rId3\" Type=\"" + kRelNs + "presProps\" Target=\"presProps.xml\"/>"
                                                "<Relationship Id=\"rId4\" Type=\"" + kRelNs + "viewProps\" Target=\"viewProps.xml\"/>"
                                                "<Relationship Id=\"rId5\" Type=\"" + kRelNs + "tableStyles\" Target=\"tableStyles.xml\"/>" + presRels + "</Relationships>"},
            {"ppt/presProps.xml", std::string(kHead) + "<p:presentationPr " + kNs + "/>"},
            {"ppt/viewProps.xml", std::string(kHead) + "<p:viewPr " + kNs + "><p:normalViewPr><p:restoredLeft sz=\"15620\"/><p:restoredTop sz=\"94660\"/></p:normalViewPr><p:gridSpacing cx=\"76200\" cy=\"76200\"/></p:viewPr>"},
            {"ppt/tableStyles.xml", std::string(kHead) + "<a:tblStyleLst xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" def=\"{5C22544A-7EE6-4342-B048-85BDC9FD1C3A}\"/>"},
            {"ppt/slideMasters/slideMaster1.xml", t.master},
            {"ppt/slideMasters/_rels/slideMaster1.xml.rels", rels + "<Relationship Id=\"rId1\" Type=\"" + kRelNs + "slideLayout\" Target=\"../slideLayouts/slideLayout1.xml\"/>"
                                                         "<Relationship Id=\"rId2\" Type=\"" + kRelNs + "theme\" Target=\"../theme/theme1.xml\"/></Relationships>"},
            {"ppt/slideLayouts/slideLayout1.xml", t.layout},
            {"ppt/slideLayouts/_rels/slideLayout1.xml.rels", rels + "<Relationship Id=\"rId1\" Type=\"" + kRelNs + "slideMaster\" Target=\"../slideMasters/slideMaster1.xml\"/></Relationships>"},
            {"ppt/theme/theme1.xml", t.theme},
        });
        for (auto& m : media_) files.push_back({m.first, std::string(m.second.begin(), m.second.end())});
        return WriteZip(path, files);
    }

private:
    double scale_ = 12700, ox_ = 0, oy_ = 0; float wPt_ = 0, hPt_ = 0;
    std::string bg_, shapes_, rels_;
    long long nextId_ = 2;
    std::vector<std::string> slides_, slideRels_;
    std::vector<std::pair<std::string, Bytes>> media_;

    std::string Xfrm(float x, float y, float w, float h) const {
        return "<a:xfrm><a:off x=\"" + Num(std::llround(ox_ + x * scale_)) + "\" y=\"" + Num(std::llround(oy_ + y * scale_)) + "\"/><a:ext cx=\"" +
               Num(std::max(1LL, std::llround(w * scale_))) + "\" cy=\"" + Num(std::max(1LL, std::llround(h * scale_))) + "\"/></a:xfrm>";
    }

    // ZIP sem compressão (as imagens já são comprimidas), gravado num temporário e renomeado no fim.
    static bool WriteZip(const std::wstring& path, const std::vector<std::pair<std::string, std::string>>& files) {
        static uint32_t table[256];
        if (!table[1]) for (uint32_t i = 0; i < 256; ++i) { uint32_t c = i; for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; }
        std::string z, cd;
        auto p16 = [](std::string& s, uint16_t v) { s.append((const char*)&v, 2); };
        auto p32 = [](std::string& s, uint32_t v) { s.append((const char*)&v, 4); };
        for (auto& [name, data] : files) {
            if (z.size() + data.size() > 0xFFFF0000ull) return false; // sem ZIP64
            uint32_t crc = 0xFFFFFFFFu;
            for (unsigned char c : data) crc = table[(crc ^ c) & 255] ^ (crc >> 8);
            crc ^= 0xFFFFFFFFu;
            uint32_t off = (uint32_t)z.size();
            p32(z, 0x04034b50); p16(z, 20); p16(z, 0x0800); p16(z, 0); p16(z, 0); p16(z, 0x21); p32(z, crc); p32(z, (uint32_t)data.size()); p32(z, (uint32_t)data.size());
            p16(z, (uint16_t)name.size()); p16(z, 0); z += name; z += data;
            p32(cd, 0x02014b50); p16(cd, 20); p16(cd, 20); p16(cd, 0x0800); p16(cd, 0); p16(cd, 0); p16(cd, 0x21); p32(cd, crc); p32(cd, (uint32_t)data.size()); p32(cd, (uint32_t)data.size());
            p16(cd, (uint16_t)name.size()); p16(cd, 0); p16(cd, 0); p16(cd, 0); p16(cd, 0); p32(cd, 0); p32(cd, off); cd += name;
        }
        uint32_t cdOff = (uint32_t)z.size();
        z += cd;
        p32(z, 0x06054b50); p16(z, 0); p16(z, 0); p16(z, (uint16_t)files.size()); p16(z, (uint16_t)files.size()); p32(z, (uint32_t)cd.size()); p32(z, cdOff); p16(z, 0);
        std::wstring tmp = path + L".tmp";
        DeleteFileW(tmp.c_str());
        if (!ops::WriteFileBytes(tmp, z.data(), z.size())) { DeleteFileW(tmp.c_str()); return false; }
        if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(tmp.c_str()); return false; }
        return true;
    }
};

// ---------------- conversão ----------------

struct Stats { int pages = 0, editable = 0, images = 0, blocks = 0, keptObjects = 0; bool engine = false; std::wstring note; };

// Fundo de uma cor só? (então vira a cor do slide e nenhuma imagem é gravada)
inline bool Uniform(HBITMAP dib, COLORREF* c) {
    BYTE* px; UINT w, h;
    if (!ops::Pixels(dib, &px, &w, &h) || !w || !h) return false;
    const BYTE* f = px;
    for (size_t i = 0; i < (size_t)w * h; ++i) if (Dist(px + i * 4, f) > 2) return false;
    *c = RGB(f[2], f[1], f[0]);
    return true;
}
// Foto ou degradê (muitas cores)? JPEG fica bem menor sem perda visível; desenhos e texto ficam em PNG.
inline bool PhotoLike(HBITMAP dib) {
    BYTE* px; UINT w, h;
    if (!ops::Pixels(dib, &px, &w, &h) || !w || !h) return false;
    std::unordered_set<uint32_t> colors;
    for (UINT y = 0; y < 160; ++y) for (UINT x = 0; x < 160; ++x) {
        const BYTE* p = px + ((size_t)(y * h / 160) * w + x * w / 160) * 4;
        colors.insert(p[0] | p[1] << 8 | p[2] << 16);
        if (colors.size() > 1024) return true;
    }
    return false;
}
inline bool Background(Deck& deck, HBITMAP dib) {
    COLORREF c;
    if (!deck.Letterboxed() && Uniform(dib, &c)) { deck.Solid(c); return true; }
    bool jpeg = PhotoLike(dib);
    Bytes data;
    if (!ops::EncodedBytes(dib, jpeg ? ops::kJpg : ops::kPng, 0.88f, data)) return false;
    deck.Picture(data, jpeg);
    return true;
}

// Escala de renderização: 150 DPI, limitada a 40 milhões de pixels.
inline float PixelScale(float wPt, float hPt) {
    float s = 150.0f / 72;
    if (wPt * s * hPt * s > 40e6f) s = std::sqrt(40e6f / (wPt * hPt));
    return s;
}

// Uma página no modo editável. Retorna false se a página deve virar imagem (o slide ainda não foi começado).
inline bool EditablePage(Api& a, H page, Deck& deck, float wPt, float hPt, Stats& st) {
    if (a.GetRotation(page)) return false;
    FS_RECTF box{};
    if (!a.GetPageBoundingBox(page, &box)) return false;
    float left = box.left, top = box.top;
    PageText text = Extract(a, page, left, top);
    std::vector<Block> blocks = GroupBlocks(GroupLines(text.visible));
    float s = PixelScale(wPt, hPt);
    UINT pw = std::max(1u, (UINT)std::lround(wPt * s)), ph = std::max(1u, (UINT)std::lround(hPt * s));
    for (auto& o : text.objs) if (!o.second.convertible && o.second.mode != 3) ++st.keptObjects;
    if (!blocks.empty()) {
        RemoveText(a, page, text.Convertible());
        HBITMAP dib = Render(a, page, pw, ph);
        if (!dib) return false;
        deck.Begin(wPt, hPt);
        bool ok = Background(deck, dib);
        DeleteObject(dib);
        if (!ok) return false;
        for (auto& b : blocks) deck.Text(b);
        deck.End();
        st.blocks += (int)blocks.size();
        ++st.editable;
        return true;
    }
    // Sem texto visível: página escaneada (camada de texto invisível ou OCR do Windows) ou só desenho.
    HBITMAP dib = Render(a, page, pw, ph);
    if (!dib) return false;
    std::vector<Unit> units;
    if (!text.invisible.empty()) {
        for (auto& l : GroupLines(text.invisible)) units.push_back({l.Text(), l.x0, l.y0, l.x1, l.y1, l.size});
    } else {
        BYTE* px; UINT w, h;
        std::vector<ocr::LineBox> boxes; std::wstring all;
        if (ops::Pixels(dib, &px, &w, &h) && ocr::Recognize(px, (int)w, (int)h, (int)w * 4, true, &all, nullptr, &boxes))
            for (auto& b : boxes)
                if (!Blank(b.text)) {
                    float y0 = b.y0 / s, y1 = b.y1 / s;
                    units.push_back({b.text, b.x0 / s, y0, b.x1 / s, y1, std::max(4.0f, (y1 - y0) / 0.95f)});
                }
    }
    int kept = 0;
    std::vector<Block> scan = ClearUnits(dib, units, s, &kept);
    deck.Begin(wPt, hPt);
    bool ok = Background(deck, dib);
    DeleteObject(dib);
    if (!ok) return false;
    for (auto& b : scan) deck.Text(b);
    deck.End();
    st.blocks += (int)scan.size();
    if (scan.empty()) ++st.images; else ++st.editable;
    return true;
}

// Sem o PDFium: páginas como imagem pelo leitor de PDF do Windows (nada editável, mas o arquivo sai).
inline bool ImagesOnly(const std::wstring& src, const std::wstring& out, Stats& st) {
    fmt::Info info;
    conv::FileInfo(src, &info);
    float wPt = info.pageWmm > 0 ? (float)(info.pageWmm / 25.4 * 72) : 720, hPt = info.pageHmm > 0 ? (float)(info.pageHmm / 25.4 * 72) : 405;
    UINT cx = (UINT)std::lround(std::max(wPt, hPt) * 150 / 72);
    HBITMAP first = nullptr; bool alpha = false; int pages = 1;
    if (!conv::RenderFile(src, cx, 0, &first, &alpha, &pages)) return false;
    Deck deck;
    deck.Size(wPt, hPt);
    for (int p = 0; p < pages; ++p) {
        HBITMAP b = p == 0 ? first : nullptr;
        if (p && !conv::RenderFile(src, cx, p, &b, &alpha, nullptr)) continue;
        HBITMAP flat = ops::Apply(b, ops::Edit(), true); // sem transparência
        DeleteObject(b);
        BYTE* px; UINT w = 0, h = 0;
        if (!flat || !ops::Pixels(flat, &px, &w, &h)) { if (flat) DeleteObject(flat); continue; }
        float k = std::max(wPt, hPt) / std::max(w, h), pw = w * k, ph = h * k; // proporção da imagem da página
        if (!deck.cx) deck.Size(pw, ph);
        deck.Begin(pw, ph);
        Background(deck, flat);
        deck.End();
        DeleteObject(flat);
        ++st.pages; ++st.images;
    }
    return deck.Save(out);
}

// Converte um PDF em PPTX. error recebe o motivo quando falha.
inline bool Convert(const std::wstring& src, const std::wstring& out, Stats& st, std::wstring& error, const std::wstring& engineDll = L"") {
    std::lock_guard<std::mutex> guard(Lock());
    Api& a = Engine(engineDll);
    if (!a.ok) {
        st.note = a.error;
        if (ImagesOnly(src, out, st)) return true;
        error = L"não foi possível converter (" + a.error + L")";
        return false;
    }
    st.engine = true;
    Bytes data;
    {
        HANDLE f = CreateFileW(src.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        LARGE_INTEGER size{};
        if (f == INVALID_HANDLE_VALUE || !GetFileSizeEx(f, &size) || size.QuadPart > (512LL << 20)) {
            if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
            error = L"não foi possível ler o PDF"; return false;
        }
        data.resize((size_t)size.QuadPart); DWORD got = 0;
        bool ok = ReadFile(f, data.data(), (DWORD)data.size(), &got, nullptr) && got == data.size();
        CloseHandle(f);
        if (!ok) { error = L"não foi possível ler o PDF"; return false; }
    }
    H doc = a.LoadMemDocument64(data.data(), data.size(), nullptr);
    if (!doc) { error = a.GetLastError() == 4 ? L"PDF protegido por senha" : L"PDF inválido ou danificado"; return false; }
    int n = a.GetPageCount(doc);
    Deck deck;
    for (int i = 0; i < n; ++i) {
        H page = a.LoadPage(doc, i);
        if (!page) continue;
        float wPt = a.GetPageWidthF(page), hPt = a.GetPageHeightF(page);
        if (wPt < 1 || hPt < 1) { a.ClosePage(page); continue; }
        if (!deck.cx) deck.Size(wPt, hPt);
        ++st.pages;
        bool done = EditablePage(a, page, deck, wPt, hPt, st);
        a.ClosePage(page);
        if (!done) { // a página intacta (recarregada) vira imagem
            page = a.LoadPage(doc, i);
            float s = PixelScale(wPt, hPt);
            HBITMAP dib = page ? Render(a, page, std::max(1u, (UINT)std::lround(wPt * s)), std::max(1u, (UINT)std::lround(hPt * s))) : nullptr;
            if (page) a.ClosePage(page);
            if (dib) { deck.Begin(wPt, hPt); Background(deck, dib); deck.End(); DeleteObject(dib); ++st.images; }
        }
    }
    a.CloseDocument(doc);
    if (!deck.Slides()) { error = L"nenhuma página convertida"; return false; }
    if (!deck.Save(out)) { error = L"não foi possível gravar o PPTX"; return false; }
    return true;
}

} // namespace pdfppt
