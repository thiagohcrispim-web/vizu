// Fonts.h - Inter e Space Grotesk embutidas no Vizu.exe (RCDATA 40-45), para o desenho sair igual em qualquer PC,
// mesmo sem essas fontes instaladas. Usa a mesma técnica de PaletteFont.h (arquivo em memória no DirectWrite).
#pragma once
#include <windows.h>
#include <dwrite_3.h>
#include <string>
#include <vector>
#include <cwctype>

namespace fonts {

constexpr int kFirstResource = 40, kLastResource = 45; // ver Vizu.rc

inline IDWriteFontCollection* g_collection = nullptr;  // só as fontes do Vizu
inline std::vector<std::wstring> g_families;           // nomes em minúsculas

inline std::wstring Lower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

// Registra as fontes no mesmo factory que vai criar os formatos de texto. Chamar uma vez, por factory.
inline bool Init(IDWriteFactory* base) {
    if (g_collection) return true;
    if (!base) return false;
    IDWriteFactory5* f5 = nullptr;
    if (FAILED(base->QueryInterface(__uuidof(IDWriteFactory5), (void**)&f5))) return false;
    IDWriteInMemoryFontFileLoader* loader = nullptr;
    IDWriteFontSetBuilder1* builder = nullptr;
    IDWriteFontSet* set = nullptr;
    IDWriteFontCollection1* coll = nullptr;
    bool ok = SUCCEEDED(f5->CreateInMemoryFontFileLoader(&loader)) && SUCCEEDED(f5->RegisterFontFileLoader(loader)) &&
              SUCCEEDED(f5->CreateFontSetBuilder(&builder));
    for (int id = kFirstResource; ok && id <= kLastResource; ++id) {
        HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
        HGLOBAL mem = res ? LoadResource(nullptr, res) : nullptr;
        const void* data = mem ? LockResource(mem) : nullptr;
        IDWriteFontFile* file = nullptr;
        ok = data && SUCCEEDED(loader->CreateInMemoryFontFileReference(f5, data, SizeofResource(nullptr, res), nullptr, &file)) &&
             SUCCEEDED(builder->AddFontFile(file));
        if (file) file->Release();
    }
    ok = ok && SUCCEEDED(builder->CreateFontSet(&set)) && SUCCEEDED(f5->CreateFontCollectionFromFontSet(set, &coll));
    if (ok) {
        g_collection = coll;
        UINT32 count = coll->GetFontFamilyCount();
        for (UINT32 i = 0; i < count; ++i) {
            IDWriteFontFamily1* fam = nullptr; IDWriteLocalizedStrings* names = nullptr;
            if (SUCCEEDED(coll->GetFontFamily(i, &fam)) && SUCCEEDED(fam->GetFamilyNames(&names))) {
                UINT32 len = 0; names->GetStringLength(0, &len);
                std::wstring name(len + 1, L'\0');
                names->GetString(0, &name[0], len + 1);
                name.resize(len);
                g_families.push_back(Lower(name));
                names->Release();
            }
            if (fam) fam->Release();
        }
    }
    if (set) set->Release();
    if (builder) builder->Release();
    if (loader) loader->Release();
    f5->Release();
    return ok;
}

// A família vem embutida? Se não, o chamador usa a coleção do sistema.
inline bool Has(const wchar_t* family) {
    if (!family || !g_collection) return false;
    std::wstring name = Lower(family);
    for (const auto& f : g_families) if (f == name) return true;
    return false;
}

inline IDWriteFontCollection* Collection() { return g_collection; }

} // namespace fonts
