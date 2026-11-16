// PDF -> PPT: texto vira caixa editável, vetores ficam no fundo, texto girado fica no fundo, texto em Form XObject
// sai do fundo, camada de texto invisível de scan vira texto, PDF inválido falha com mensagem.
// Usa o mesmo PDFium embutido do Vizu (recurso 330 do tests/pptx_tests.rc).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000003
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <d2d1_3.h>
#include "../PdfPptx.h"
#include <iostream>
#include <stdexcept>

static int checks = 0;
static void Check(bool ok, const std::string& message) { ++checks; if (!ok) throw std::runtime_error(message); }

static std::wstring Temp(const wchar_t* name) {
    wchar_t dir[MAX_PATH]; GetTempPathW(MAX_PATH, dir);
    return std::wstring(dir) + L"vizu-pptx-" + name;
}
// PDF mínimo; o PDFium reconstrói a tabela xref (offsets não precisam ser exatos).
static std::wstring Pdf(const wchar_t* name, const std::string& content, const std::string& extraObjs = "", const std::string& resources = "") {
    std::string pdf = "%PDF-1.7\n"
        "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"
        "2 0 obj << /Type /Pages /Count 1 /Kids [3 0 R] >> endobj\n"
        "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 720 405] /Contents 4 0 R /Resources << /Font << /F1 5 0 R /F2 6 0 R >> " + resources + " >> >> endobj\n"
        "4 0 obj << /Length " + std::to_string(content.size()) + " >> stream\n" + content + "\nendstream endobj\n"
        "5 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >> endobj\n"
        "6 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Times-BoldItalic /Encoding /WinAnsiEncoding >> endobj\n" + extraObjs +
        "trailer << /Root 1 0 R >>\n%%EOF\n";
    std::wstring path = Temp(name);
    DeleteFileW(path.c_str());
    Check(ops::WriteFileBytes(path, pdf.data(), pdf.size()), "write pdf");
    return path;
}
static std::string ReadAll(const std::wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    std::string s;
    if (f == INVALID_HANDLE_VALUE) return s;
    DWORD n = GetFileSize(f, nullptr), got = 0; s.resize(n);
    ReadFile(f, &s[0], n, &got, nullptr); CloseHandle(f);
    return s;
}
// O ZIP é gravado sem compressão: dá para procurar o XML direto nos bytes.
static std::string Part(const std::string& zip, const std::string& name) {
    for (size_t at = zip.find(name); at != std::string::npos; at = zip.find(name, at + 1)) {
        if (at < 30) continue;
        const BYTE* h = (const BYTE*)zip.data() + at - 30;
        uint16_t len; memcpy(&len, h + 26, 2);
        if (memcmp(h, "PK\3\4", 4) || len != name.size()) continue; // cabeçalho local deste nome
        uint32_t size; memcpy(&size, h + 18, 4);
        return zip.substr(at + name.size(), size);
    }
    return {};
}
static std::wstring Convert(const std::wstring& pdf, const wchar_t* out, pdfppt::Stats& st) {
    std::wstring path = Temp(out), err;
    DeleteFileW(path.c_str());
    Check(pdfppt::Convert(pdf, path, st, err), "convert failed: " + pdfppt::Utf8(err));
    return path;
}

int wmain(int argc, wchar_t** argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (argc == 3) { // uso manual: pptx-tests.exe entrada.pdf saida.pptx
        pdfppt::Stats st; std::wstring err;
        bool ok = pdfppt::Convert(argv[1], argv[2], st, err);
        std::wcout << (ok ? L"ok" : err) << L" pages=" << st.pages << L" editable=" << st.editable << L" images=" << st.images << L" blocks=" << st.blocks << L"\n";
        return ok ? 0 : 1;
    }
    try {
        // fontes e agrupamento
        auto fs = pdfppt::ParseFont("ABCDEF+TimesNewRomanPS-BoldItalicMT", 0, 0);
        Check(fs.family == L"Times New Roman" && fs.bold && fs.italic, "font parse TNR");
        fs = pdfppt::ParseFont("Helvetica-Bold", 0, 0);
        Check(fs.family == L"Arial" && fs.bold && !fs.italic, "font parse Helvetica-Bold");
        fs = pdfppt::ParseFont("XYZABC+OpenSans-SemiboldItalic", 0, 0);
        Check(fs.family == L"Open Sans" && fs.bold && fs.italic, "font parse Open Sans");
        Check(pdfppt::Category(L"Garamond Premier") == L"Times New Roman" && pdfppt::Category(L"Fira Mono") == L"Courier New", "category");
        std::vector<pdfppt::Char> chars;
        auto word = [&](const wchar_t* w, float x, float base) {
            for (const wchar_t* p = w; *p; ++p) { pdfppt::Char c; c.text = *p; c.x0 = x; c.x1 = x + 5; c.y0 = base - 8; c.y1 = base + 2; c.baseline = base; c.size = 10; chars.push_back(c); x += 5; }
            return x;
        };
        float e = word(L"Olá", 10, 20); word(L"mundo", e + 3, 20); word(L"longe", 300, 20); word(L"linha2", 10, 32);
        auto lines = pdfppt::GroupLines(chars);
        Check(lines.size() == 3 && lines[0].Text() == L"Olá mundo" && lines[1].Text() == L"longe", "group lines");
        auto blocks = pdfppt::GroupBlocks(lines);
        Check(blocks.size() == 2 && blocks[0].lines.size() == 2, "group blocks across interleaved line");

        pdfppt::Stats st;
        std::wstring err;
        // 1. texto + retângulo: texto editável, retângulo na imagem de fundo
        std::wstring pdf = Pdf(L"a.pdf", "q 0.1 0.4 0.7 rg 40 40 200 100 re f Q BT /F1 28 Tf 50 300 Td (T\355tulo do slide) Tj ET "
                                         "BT /F1 16 Tf 50 260 Td (Primeira linha) Tj 0 -20 Td (Segunda linha) Tj ET "
                                         "BT /F2 14 Tf 400 300 Td (Estilo) Tj ET");
        std::string zip = ReadAll(Convert(pdf, L"a.pptx", st));
        std::string slide = Part(zip, "ppt/slides/slide1.xml");
        Check(st.engine && st.pages == 1 && st.editable == 1 && st.blocks == 3, "stats page 1");
        Check(slide.find("<a:t>T\xC3\xADtulo do slide</a:t>") != std::string::npos, "title editable (utf-8)");
        Check(slide.find("<a:t>Primeira linha</a:t></a:r></a:p><a:p>") != std::string::npos && slide.find("<a:t>Segunda linha</a:t>") != std::string::npos, "paragraph lines");
        Check(slide.find("typeface=\"Times New Roman\"") != std::string::npos && slide.find("b=\"1\" i=\"1\"") != std::string::npos, "bold italic times");
        Check(slide.find("<p:pic>") != std::string::npos && zip.find("ppt/media/image1.png") != std::string::npos, "vector rect kept as picture");
        Check(slide.find("sz=\"2800\"") != std::string::npos, "font size 28");
        Check(!Part(zip, "ppt/presentation.xml").empty() && Part(zip, "ppt/presentation.xml").find("cx=\"9144000\" cy=\"5143500\"") != std::string::npos, "slide size 720x405pt");
        Check(!Part(zip, "ppt/theme/theme1.xml").empty() && !Part(zip, "ppt/slideMasters/slideMaster1.xml").empty(), "template parts");

        // 2. fundo liso: cor do slide, sem imagem
        st = {};
        zip = ReadAll(Convert(Pdf(L"b.pdf", "q 0.12 0.2 0.28 rg 0 0 720 405 re f Q BT 1 1 1 rg /F1 30 Tf 50 200 Td (Branco) Tj ET"), L"b.pptx", st));
        slide = Part(zip, "ppt/slides/slide1.xml");
        Check(slide.find("<p:bg><p:bgPr><a:solidFill><a:srgbClr val=\"1F3347\"/>") != std::string::npos && slide.find("<p:pic>") == std::string::npos, "solid background");
        Check(slide.find("<a:srgbClr val=\"FFFFFF\"/></a:solidFill><a:latin typeface=\"Arial\"/>") != std::string::npos, "white text color");

        // 3. texto girado fica no fundo; texto reto vira caixa
        st = {};
        zip = ReadAll(Convert(Pdf(L"c.pdf", "BT /F1 20 Tf 50 300 Td (Reto) Tj ET BT /F1 20 Tf 0 1 -1 0 600 100 Tm (Girado) Tj ET"), L"c.pptx", st));
        slide = Part(zip, "ppt/slides/slide1.xml");
        Check(slide.find(">Reto<") != std::string::npos && slide.find("Girado") == std::string::npos && slide.find("<p:pic>") != std::string::npos, "rotated text kept in background");
        Check(st.keptObjects == 1, "kept object counted");

        // 4. texto dentro de Form XObject sai do fundo (fundo fica liso)
        st = {};
        std::string form = "BT /F1 24 Tf 60 250 Td (Dentro do form) Tj ET";
        zip = ReadAll(Convert(Pdf(L"d.pdf", "/Fx1 Do", "7 0 obj << /Type /XObject /Subtype /Form /BBox [0 0 720 405] /Resources << /Font << /F1 5 0 R >> >> /Length " +
                                                       std::to_string(form.size()) + " >> stream\n" + form + "\nendstream endobj\n", "/XObject << /Fx1 7 0 R >>"), L"d.pptx", st));
        slide = Part(zip, "ppt/slides/slide1.xml");
        Check(slide.find(">Dentro do form<") != std::string::npos && slide.find("<p:bg>") != std::string::npos && slide.find("<p:pic>") == std::string::npos, "form xobject text removed");

        // 5. fonte tamanho 1 ampliada pela matriz
        st = {};
        zip = ReadAll(Convert(Pdf(L"e.pdf", "BT /F1 1 Tf 20 0 0 20 50 200 Tm (Escala) Tj ET"), L"e.pptx", st));
        Check(Part(zip, "ppt/slides/slide1.xml").find("sz=\"2000\"") != std::string::npos, "matrix scaled font size");

        // 6. scan com camada de texto invisível: a linha sai da imagem e volta como texto
        st = {};
        std::string img;
        for (int y = 0; y < 405; ++y) for (int x = 0; x < 720; ++x) { bool ink = x >= 52 && x < 200 && y >= 95 && y < 112; img += ink ? std::string(3, '\0') : std::string(3, '\xff'); }
        zip = ReadAll(Convert(Pdf(L"f.pdf", "q 720 0 0 405 0 0 cm /Im1 Do Q BT 3 Tr /F1 20 Tf 50 295 Td (Linha reconhecida) Tj ET",
                                  "7 0 obj << /Type /XObject /Subtype /Image /Width 720 /Height 405 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Length " +
                                  std::to_string(img.size()) + " >> stream\n" + img + "\nendstream endobj\n", "/XObject << /Im1 7 0 R >>"), L"f.pptx", st));
        slide = Part(zip, "ppt/slides/slide1.xml");
        Check(slide.find(">Linha reconhecida<") != std::string::npos && slide.find("<p:bg>") != std::string::npos, "invisible text layer becomes editable");
        Check(slide.find("<a:srgbClr val=\"000000\"/></a:solidFill><a:latin") != std::string::npos, "scan ink color");

        // 7. entrada inválida falha com mensagem e não deixa arquivo
        std::wstring bad = Temp(L"bad.pdf"), out = Temp(L"bad.pptx");
        DeleteFileW(bad.c_str());
    ops::WriteFileBytes(bad, "nada", 4);
        DeleteFileW(out.c_str());
        st = {};
        Check(!pdfppt::Convert(bad, out, st, err) && !err.empty() && GetFileAttributesW(out.c_str()) == INVALID_FILE_ATTRIBUTES, "invalid pdf fails cleanly");
    } catch (const std::exception& ex) {
        std::cerr << "FALHOU: " << ex.what() << " (depois de " << checks << " verificações)\n";
        return 1;
    }
    std::cout << checks << " verificações de PDF -> PPT passaram\n";
    return 0;
}
