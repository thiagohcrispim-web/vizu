// Word/Excel/PowerPoint -> PDF: tipos reconhecidos, chamadas COM por nome (testadas com o Scripting.Dictionary do
// Windows) e mensagem clara quando não há Office nem LibreOffice instalados.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000003
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include "../Office.h"
#include <iostream>
#include <stdexcept>

static int checks = 0;
static void Check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }

int wmain() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        using namespace office;
        Check(AppFor(L"C:\\a\\Relatório.DOCX") == kWord && AppFor(L"x.doc") == kWord && AppFor(L"x.rtf") == kWord && AppFor(L"x.odt") == kWord, "word types");
        Check(AppFor(L"x.XLS") == kExcel && AppFor(L"x.xlsx") == kExcel && AppFor(L"x.ods") == kExcel, "excel types");
        Check(AppFor(L"x.ppt") == kPowerPoint && AppFor(L"x.pptx") == kPowerPoint && AppFor(L"x.odp") == kPowerPoint, "powerpoint types");
        Check(!IsOfficeDoc(L"x.pdf") && !IsOfficeDoc(L"x.png") && !IsOfficeDoc(L"docx"), "other types");
        Check(FileUrl(L"C:\\Temp\\perfil vizu").rfind(L"file:///C:/Temp/perfil", 0) == 0, "file url");

        // IDispatch por nome: métodos com vários argumentos (ordem), propriedades com argumento e leitura
        CLSID clsid; IDispatch* dict = nullptr;
        if (SUCCEEDED(CLSIDFromProgID(L"Scripting.Dictionary", &clsid)) &&
            SUCCEEDED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_IDispatch, (void**)&dict)) && dict) {
            Check(SUCCEEDED(Call(dict, L"Add", DISPATCH_METHOD, {Var(std::wstring(L"chave")), Var(42)})), "dispatch Add");
            Check(SUCCEEDED(Call(dict, L"Add", DISPATCH_METHOD, {Var(std::wstring(L"outra")), Var(std::wstring(L"valor"))})), "dispatch Add 2");
            Var count = Get(dict, L"Count");
            Check(count.v.vt == VT_I4 && count.v.lVal == 2, "dispatch property get");
            Var item;
            Check(SUCCEEDED(Call(dict, L"Item", DISPATCH_PROPERTYGET, {Var(std::wstring(L"chave"))}, &item)) && item.v.vt == VT_I4 && item.v.lVal == 42, "argument order");
            Check(FAILED(Call(dict, L"NaoExiste", DISPATCH_METHOD, {})), "unknown member fails");
            dict->Release();
        } else std::cout << "(Scripting.Dictionary indisponível: chamadas COM não testadas)\n";

        // Sem Office nem LibreOffice: falha com explicação e não cria arquivo
        if (!OfficeInstalled(kWord) && LibreOfficeExe().empty()) {
            wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
            std::wstring pdf = std::wstring(tmp) + L"vizu-office-test.pdf", err;
            DeleteFileW(pdf.c_str());
            Check(!ToPdf(std::wstring(tmp) + L"nada.docx", pdf, err) && err.find(L"LibreOffice") != std::wstring::npos, "no engine message");
            Check(GetFileAttributesW(pdf.c_str()) == INVALID_FILE_ATTRIBUTES, "no output without engine");
        }
        std::wstring err;
        Check(!ToPdf(L"x.png", L"y.pdf", err) && !err.empty(), "unsupported");
    } catch (const std::exception& ex) {
        std::cerr << "FALHOU: " << ex.what() << " (depois de " << checks << " verificações)\n";
        return 1;
    }
    std::cout << checks << " verificações de Office -> PDF passaram\n";
    return 0;
}
