// Leitores de arquivo e comparação de versão: arquivos corrompidos falham sem derrubar o programa.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000003
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <d2d1_3.h>
#include "../Formats.h"
#include "../Update.h"
#include "../PdfEdit.h"
#include <iostream>
#include <stdexcept>
#include <string>

static int checks = 0;
static void Check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }

static void Put(fmt::Bytes& b, UINT64 v, int bytes, bool big) {
    for (int i = 0; i < bytes; ++i) b.push_back((BYTE)(v >> 8 * (big ? bytes - 1 - i : i)));
}
// Cabeçalho PSD + as três seções vazias + tipo de compressão.
static fmt::Bytes Psd(UINT channels, UINT h, UINT w, UINT compression) {
    fmt::Bytes b = {'8', 'B', 'P', 'S', 0, 1, 0, 0, 0, 0, 0, 0};
    Put(b, channels, 2, true); Put(b, h, 4, true); Put(b, w, 4, true); Put(b, 8, 2, true); Put(b, 3, 2, true); // 8 bits, RGB
    for (int i = 0; i < 3; ++i) Put(b, 0, 4, true);
    Put(b, compression, 2, true);
    return b;
}
static HRESULT Render(const fmt::Bytes& data, const wchar_t* ext, HBITMAP* bmp) {
    IStream* s = SHCreateMemStream(data.data(), (UINT)data.size());
    bool alpha = false;
    HRESULT hr = s ? fmt::Render(s, ext, 256, 0, bmp, &alpha) : E_OUTOFMEMORY;
    if (s) s->Release();
    return hr;
}

// PSD 2 x 2 salvo com "Maximizar compatibilidade" desligado: composta toda branca, hasRealMergedData = 0
// (resource 1057) e uma camada vermelha sem compressão cobrindo tudo.
static fmt::Bytes PsdNoComposite() {
    fmt::Bytes b = {'8', 'B', 'P', 'S', 0, 1, 0, 0, 0, 0, 0, 0};
    Put(b, 3, 2, true); Put(b, 2, 4, true); Put(b, 2, 4, true); Put(b, 8, 2, true); Put(b, 3, 2, true);
    Put(b, 0, 4, true); // color mode
    fmt::Bytes res = {'8', 'B', 'I', 'M'};
    Put(res, 1057, 2, true); Put(res, 0, 2, true); // nome vazio (pascal, par)
    Put(res, 5, 4, true); Put(res, 1, 4, true); res.push_back(0); res.push_back(0); // versão 1, hasRealMergedData 0 + pad
    Put(b, res.size(), 4, true); b.insert(b.end(), res.begin(), res.end());
    fmt::Bytes li;
    Put(li, 1, 2, true); // 1 camada
    Put(li, 0, 4, true); Put(li, 0, 4, true); Put(li, 2, 4, true); Put(li, 2, 4, true); // topo, esquerda, baixo, direita
    Put(li, 4, 2, true);
    for (int id : {-1, 0, 1, 2}) { Put(li, (USHORT)(short)id, 2, true); Put(li, 2 + 4, 4, true); }
    for (char c : std::string("8BIMnorm")) li.push_back((BYTE)c);
    li.push_back(255); li.push_back(0); li.push_back(0); li.push_back(0); // opacidade, clipping, flags (visível), filler
    Put(li, 4 + 4 + 4, 4, true); Put(li, 0, 4, true); Put(li, 0, 4, true); Put(li, 0, 4, true); // máscara, faixas, nome vazio (4 bytes)
    for (int id : {-1, 0, 1, 2}) { Put(li, 0, 2, true); for (int i = 0; i < 4; ++i) li.push_back(id == -1 || id == 0 ? 255 : 0); }
    fmt::Bytes lm; Put(lm, li.size(), 4, true); lm.insert(lm.end(), li.begin(), li.end()); Put(lm, 0, 4, true);
    Put(b, lm.size(), 4, true); b.insert(b.end(), lm.begin(), lm.end());
    Put(b, 0, 2, true);
    for (int i = 0; i < 12; ++i) b.push_back(255); // composta branca
    return b;
}

int wmain() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        HBITMAP bmp = nullptr;
        // PSD bom, 2 x 2, sem compressão: continua abrindo
        fmt::Bytes good = Psd(3, 2, 2, 0);
        for (int i = 0; i < 12; ++i) good.push_back((BYTE)(i * 20));
        Check(SUCCEEDED(Render(good, L".psd", &bmp)) && bmp, "valid PSD renders");
        DeleteObject(bmp); bmp = nullptr;
        { // composta em branco ("Maximizar compatibilidade" desligado): a miniatura vem das camadas, não branca
            Check(SUCCEEDED(Render(PsdNoComposite(), L".psd", &bmp)) && bmp, "PSD without real composite renders");
            DIBSECTION ds = {};
            Check(GetObject(bmp, sizeof(ds), &ds) == sizeof(ds) && ds.dsBm.bmBits, "PSD bitmap is a DIB");
            const BYTE* px = (const BYTE*)ds.dsBm.bmBits;
            Check(px[2] > 200 && px[1] < 40 && px[0] < 40, "PSD without real composite shows the layer (red), not white");
            DeleteObject(bmp); bmp = nullptr;
        }
        { // canal extra = seleção salva (preta), sem contagem negativa de camadas: não é transparência, não sai branco
            fmt::Bytes sel = Psd(4, 1, 1, 0);
            for (BYTE v : {(BYTE)200, (BYTE)10, (BYTE)10, (BYTE)0}) sel.push_back(v);
            Check(SUCCEEDED(Render(sel, L".psd", &bmp)) && bmp, "PSD with saved selection renders");
            DIBSECTION ds = {};
            Check(GetObject(bmp, sizeof(ds), &ds) == sizeof(ds) && ds.dsBm.bmBits, "PSD selection bitmap is a DIB");
            const BYTE* px = (const BYTE*)ds.dsBm.bmBits;
            Check(px[2] > 180 && px[1] < 40, "saved selection channel is not treated as transparency");
            DeleteObject(bmp); bmp = nullptr;
        }
        // tamanhos absurdos: falha, sem pedir memória nem derrubar o processo
        Check(FAILED(Render(Psd(3, 0xFFFFFFFFu, 1, 0), L".psd", &bmp)), "PSD with absurd height is refused");
        Check(FAILED(Render(Psd(3, 1, 0xFFFFFFFFu, 1), L".psd", &bmp)), "PSD with absurd width is refused");
        Check(FAILED(Render(Psd(3, 300000, 300000, 1), L".psd", &bmp)), "truncated huge PSD is refused");
        // linha comprimida que diz ter 4 GB
        fmt::Bytes rle = Psd(3, 1, 1, 1);
        for (int i = 0; i < 3; ++i) Put(rle, 0xFFFF, 2, true);
        Check(FAILED(Render(rle, L".psd", &bmp)), "PSD with oversized packed row is refused");
        // ZIP cuja prévia diz ocupar 4 GB comprimidos
        fmt::Bytes zip = {'P', 'K', 1, 2};
        zip.resize(46, 0);
        Put(zip, 0, 0, false);
        zip[20] = zip[21] = zip[22] = zip[23] = 0xFF; // tamanho comprimido
        zip[24] = 16;                                  // tamanho real
        const char name[] = "preview.png";
        zip[28] = (BYTE)(sizeof(name) - 1);
        zip.insert(zip.end(), name, name + sizeof(name) - 1);
        UINT cdSize = (UINT)zip.size();
        fmt::Bytes eocd = {'P', 'K', 5, 6, 0, 0, 0, 0, 1, 0, 1, 0};
        Put(eocd, cdSize, 4, false); Put(eocd, 0, 4, false); Put(eocd, 0, 2, false);
        zip.insert(zip.end(), eocd.begin(), eocd.end());
        IStream* zs = SHCreateMemStream(zip.data(), (UINT)zip.size());
        fmt::Bytes img;
        Check(zs && !fmt::ZipPreview(zs, img), "ZIP entry larger than the file is refused");
        if (zs) zs->Release();
        // versões
        Check(upd::Compare(L"9.8.9", L"9.8.10") < 0, "9.8.10 is newer than 9.8.9");
        Check(upd::Compare(L"v9.8.7", L"9.8.7") == 0, "leading v is ignored");
        Check(upd::Compare(L"10.0.0", L"9.9.9") > 0, "major version wins");
        // PDF: um '<' solto no fim do objeto não pode travar o leitor (antes ficava em laço infinito)
        Check(pdfedit::Refs("<< /A <").empty(), "lone '<' at end returns");
        Check(pdfedit::Renumber("5 0 R <", {}).size() > 0, "Renumber finishes with a lone '<'");
        Check(pdfedit::Refs("<</Kids [3 0 R 4 0 R]>>").size() == 2, "references are still found");
        std::wcout << L"format tests passed: " << checks << L" checks\n";
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
