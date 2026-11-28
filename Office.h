// Office.h - documentos do Word, Excel e PowerPoint (e OpenDocument) para PDF.
// O Windows não converte esses formatos sozinho; o Vizu usa o que estiver instalado:
//  1. o próprio Microsoft Office (Word, Excel ou PowerPoint, por automação, sem janela);
//  2. o LibreOffice (gratuito), em modo sem interface.
// Nada é alterado no documento original: ele é aberto só para leitura. Documentos com senha falham em vez de
// pedir a senha. Uma instância do Office que o usuário já tinha aberta nunca é fechada; uma que o Vizu abriu e
// travou é encerrada depois do tempo-limite.
#pragma once
#include <windows.h>
#include <oleauto.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <set>
#include <string>
#include <vector>
#include "VectorConvert.h"

namespace office {

enum App { kNone, kWord, kExcel, kPowerPoint };

inline std::wstring Lower(std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
inline std::wstring Ext(const std::wstring& path) { const wchar_t* e = PathFindExtensionW(path.c_str()); return Lower(e ? e : L""); }

inline App AppFor(const std::wstring& path) {
    static const wchar_t* word[] = {L".doc", L".docx", L".docm", L".dot", L".dotx", L".dotm", L".rtf", L".odt"};
    static const wchar_t* excel[] = {L".xls", L".xlsx", L".xlsm", L".xlsb", L".xlt", L".xltx", L".ods"};
    static const wchar_t* ppt[] = {L".ppt", L".pptx", L".pptm", L".pps", L".ppsx", L".pot", L".potx", L".odp"};
    std::wstring e = Ext(path);
    for (auto x : word) if (e == x) return kWord;
    for (auto x : excel) if (e == x) return kExcel;
    for (auto x : ppt) if (e == x) return kPowerPoint;
    return kNone;
}
inline bool IsOfficeDoc(const std::wstring& path) { return AppFor(path) != kNone; }

// ---------- automação COM (IDispatch, sem bibliotecas de tipos) ----------

struct Var {
    VARIANT v;
    Var() { VariantInit(&v); }
    explicit Var(const std::wstring& s) { VariantInit(&v); v.vt = VT_BSTR; v.bstrVal = SysAllocString(s.c_str()); }
    explicit Var(int i) { VariantInit(&v); v.vt = VT_I4; v.lVal = i; }
    static Var Bool(bool b) { Var r; r.v.vt = VT_BOOL; r.v.boolVal = b ? VARIANT_TRUE : VARIANT_FALSE; return r; }
    static Var Missing() { Var r; r.v.vt = VT_ERROR; r.v.scode = DISP_E_PARAMNOTFOUND; return r; }
    Var(const Var& o) { VariantInit(&v); VariantCopy(&v, const_cast<VARIANT*>(&o.v)); }
    Var& operator=(const Var& o) { if (this != &o) { VariantClear(&v); VariantCopy(&v, const_cast<VARIANT*>(&o.v)); } return *this; }
    ~Var() { VariantClear(&v); }
    IDispatch* Disp() const { return v.vt == VT_DISPATCH ? v.pdispVal : nullptr; }
};

// Chama um método ou lê/grava uma propriedade pelo nome. Argumentos na ordem normal (são invertidos aqui).
inline HRESULT Call(IDispatch* obj, const wchar_t* name, WORD kind, std::vector<Var> args, Var* result = nullptr) {
    if (!obj) return E_POINTER;
    DISPID id; LPOLESTR n = (LPOLESTR)name;
    HRESULT hr = obj->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;
    std::vector<VARIANT> raw;
    for (auto it = args.rbegin(); it != args.rend(); ++it) raw.push_back(it->v);
    DISPPARAMS dp = {raw.empty() ? nullptr : raw.data(), nullptr, (UINT)raw.size(), 0};
    DISPID put = DISPID_PROPERTYPUT;
    if (kind == DISPATCH_PROPERTYPUT) { dp.rgdispidNamedArgs = &put; dp.cNamedArgs = 1; }
    Var tmp;
    return obj->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, kind, &dp, result ? &result->v : &tmp.v, nullptr, nullptr);
}
inline Var Get(IDispatch* obj, const wchar_t* name) { Var r; Call(obj, name, DISPATCH_PROPERTYGET, {}, &r); return r; }
inline void Put(IDispatch* obj, const wchar_t* name, Var value) { Call(obj, name, DISPATCH_PROPERTYPUT, {value}); }

inline const wchar_t* ProgId(App a) { return a == kWord ? L"Word.Application" : a == kExcel ? L"Excel.Application" : L"PowerPoint.Application"; }
inline const wchar_t* ExeName(App a) { return a == kWord ? L"winword.exe" : a == kExcel ? L"excel.exe" : L"powerpnt.exe"; }

inline bool OfficeInstalled(App a) { CLSID c; return SUCCEEDED(CLSIDFromProgID(ProgId(a), &c)); }

inline std::set<DWORD> Processes(const wchar_t* exe) {
    std::set<DWORD> ids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return ids;
    PROCESSENTRY32W pe = {sizeof(pe)};
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (!_wcsicmp(pe.szExeFile, exe)) ids.insert(pe.th32ProcessID);
    CloseHandle(snap);
    return ids;
}

// Converte com o Office, numa thread própria (STA). Retorna S_OK, ou um erro.
inline HRESULT OfficeExport(App app, const std::wstring& src, const std::wstring& pdf) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HRESULT hr = E_FAIL;
    CLSID clsid;
    IDispatch* xl = nullptr;
    if (SUCCEEDED(CLSIDFromProgID(ProgId(app), &clsid)) &&
        SUCCEEDED(hr = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, (void**)&xl)) && xl) {
        // Uma instância que o usuário já tinha aberta continua aberta no fim.
        Var visible = Get(xl, L"Visible");
        bool userInstance = visible.v.vt == VT_BOOL ? visible.v.boolVal != VARIANT_FALSE : visible.v.vt == VT_I4 && visible.v.lVal != 0;
        if (app == kWord) {
            Put(xl, L"DisplayAlerts", Var(0)); // wdAlertsNone
            Var docs = Get(xl, L"Documents"), doc;
            // FileName, ConfirmConversions, ReadOnly, AddToRecentFiles, PasswordDocument (senha falsa: falha em vez de perguntar)
            hr = Call(docs.Disp(), L"Open", DISPATCH_METHOD, {Var(src), Var::Bool(false), Var::Bool(true), Var::Bool(false), Var(std::wstring(L"vizu-sem-senha"))}, &doc);
            if (SUCCEEDED(hr) && doc.Disp()) {
                hr = Call(doc.Disp(), L"ExportAsFixedFormat", DISPATCH_METHOD, {Var(pdf), Var(17)}); // wdExportFormatPDF
                Call(doc.Disp(), L"Close", DISPATCH_METHOD, {Var(0)}); // sem salvar
            } else if (SUCCEEDED(hr)) hr = E_FAIL;
        } else if (app == kExcel) {
            Put(xl, L"DisplayAlerts", Var::Bool(false));
            Var books = Get(xl, L"Workbooks"), book;
            // Filename, UpdateLinks=0, ReadOnly, Format, Password (senha falsa: falha em vez de perguntar)
            hr = Call(books.Disp(), L"Open", DISPATCH_METHOD, {Var(src), Var(0), Var::Bool(true), Var::Missing(), Var(std::wstring(L"vizu-sem-senha"))}, &book);
            if (SUCCEEDED(hr) && book.Disp()) {
                hr = Call(book.Disp(), L"ExportAsFixedFormat", DISPATCH_METHOD, {Var(0), Var(pdf)}); // xlTypePDF
                Call(book.Disp(), L"Close", DISPATCH_METHOD, {Var::Bool(false)});
            } else if (SUCCEEDED(hr)) hr = E_FAIL;
        } else {
            Put(xl, L"DisplayAlerts", Var(1)); // ppAlertsNone
            Var pres = Get(xl, L"Presentations"), p;
            // FileName, ReadOnly=msoTrue, Untitled=msoFalse, WithWindow=msoFalse
            hr = Call(pres.Disp(), L"Open", DISPATCH_METHOD, {Var(src), Var(-1), Var(0), Var(0)}, &p);
            if (SUCCEEDED(hr) && p.Disp()) {
                hr = Call(p.Disp(), L"SaveAs", DISPATCH_METHOD, {Var(pdf), Var(32)}); // ppSaveAsPDF
                Call(p.Disp(), L"Close", DISPATCH_METHOD, {});
            } else if (SUCCEEDED(hr)) hr = E_FAIL;
        }
        if (!userInstance) Call(xl, L"Quit", DISPATCH_METHOD, {});
        xl->Release();
    }
    CoUninitialize();
    return hr;
}

// ---------- LibreOffice ----------

inline std::wstring LibreOfficeExe() {
    wchar_t buf[MAX_PATH] = {}; DWORD size = sizeof(buf);
    for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER})
        if (RegGetValueW(root, L"SOFTWARE\\LibreOffice\\UNO\\InstallPath", nullptr, RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, buf, &size) == ERROR_SUCCESS) {
            std::wstring exe = std::wstring(buf) + L"\\soffice.exe";
            if (GetFileAttributesW(exe.c_str()) != INVALID_FILE_ATTRIBUTES) return exe;
            size = sizeof(buf);
        }
    for (const wchar_t* var : {L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramW6432"}) {
        wchar_t pf[MAX_PATH] = {};
        if (!GetEnvironmentVariableW(var, pf, MAX_PATH)) continue;
        std::wstring exe = std::wstring(pf) + L"\\LibreOffice\\program\\soffice.exe";
        if (GetFileAttributesW(exe.c_str()) != INVALID_FILE_ATTRIBUTES) return exe;
    }
    return L"";
}

inline std::wstring FileUrl(const std::wstring& path) {
    wchar_t url[2048] = {}; DWORD n = ARRAYSIZE(url);
    return SUCCEEDED(UrlCreateFromPathW(path.c_str(), url, &n, 0)) ? url : L"";
}

inline bool LibreOfficeExport(const std::wstring& exe, const std::wstring& src, const std::wstring& pdf, std::wstring& error) {
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    std::wstring work = std::wstring(tmp) + L"vizu-office-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    std::wstring profile = work + L"\\perfil", out = work + L"\\saida";
    CreateDirectoryW(work.c_str(), nullptr); CreateDirectoryW(out.c_str(), nullptr);
    std::wstring log;
    // perfil próprio: funciona mesmo com o LibreOffice do usuário aberto, sem mexer nas configurações dele
    bool ran = vec::Run(exe, {L"-env:UserInstallation=" + FileUrl(profile), L"--headless", L"--norestore", L"--nolockcheck",
                              L"--convert-to", L"pdf", L"--outdir", out, src}, work + L"\\log.txt", log, error, 180000);
    std::wstring produced = out + L"\\" + std::wstring(PathFindFileNameW(src.c_str()));
    produced = produced.substr(0, produced.find_last_of(L'.')) + L".pdf";
    bool ok = GetFileAttributesW(produced.c_str()) != INVALID_FILE_ATTRIBUTES && MoveFileExW(produced.c_str(), pdf.c_str(), MOVEFILE_COPY_ALLOWED);
    SHFILEOPSTRUCTW op = {};
    std::wstring from = work; from.push_back(0);
    op.wFunc = FO_DELETE; op.pFrom = from.c_str(); op.fFlags = FOF_NO_UI;
    SHFileOperationW(&op);
    if (!ok) error = ran ? L"o LibreOffice não gerou o PDF (documento com senha ou danificado?)" : L"o LibreOffice não abriu ou demorou demais";
    return ok;
}

// ---------- entrada ----------

// Converte um documento em PDF (pdf não pode existir). error recebe o motivo quando falha.
inline bool ToPdf(const std::wstring& src, const std::wstring& pdf, std::wstring& error) {
    App app = AppFor(src);
    if (app == kNone) { error = L"formato não suportado"; return false; }
    bool odf = Ext(src).rfind(L".od", 0) == 0;
    if (OfficeInstalled(app)) {
        std::set<DWORD> before = Processes(ExeName(app));
        // A tarefa fica no heap: se o Office não responder nem depois de encerrado, a thread é abandonada sem
        // escrever em memória já liberada (a última referência apaga a tarefa).
        struct Task { App app; std::wstring src, pdf; HRESULT hr = E_FAIL; LONG refs = 2; };
        Task* task = new Task{app, src, pdf};
        auto release = [](Task* t) { if (InterlockedDecrement(&t->refs) == 0) delete t; };
        HANDLE h = CreateThread(nullptr, 0, [](void* p) -> DWORD {
            auto t = (Task*)p;
            t->hr = OfficeExport(t->app, t->src, t->pdf);
            if (InterlockedDecrement(&t->refs) == 0) delete t;
            return 0;
        }, task, 0, nullptr);
        if (!h) { delete task; task = nullptr; }
        bool finished = h && WaitForSingleObject(h, 180000) == WAIT_OBJECT_0;
        if (h && !finished) {
            // travou (diálogo escondido?): encerra só a instância que o Vizu abriu; a de quem já usava o Office fica
            for (DWORD id : Processes(ExeName(app)))
                if (!before.count(id)) { HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, id); if (p) { TerminateProcess(p, 1); CloseHandle(p); } }
            finished = WaitForSingleObject(h, 30000) == WAIT_OBJECT_0;
        }
        HRESULT hr = finished && task ? task->hr : HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        if (h) CloseHandle(h);
        if (task) release(task);
        if (SUCCEEDED(hr) && GetFileAttributesW(pdf.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
        if (!finished) { error = L"o Office não respondeu (feche janelas abertas do Office e tente de novo)"; return false; }
        DeleteFileW(pdf.c_str());
        if (!odf) error = L"o Office não conseguiu abrir o documento (com senha ou danificado?)";
    }
    std::wstring lo = LibreOfficeExe();
    if (!lo.empty()) { error.clear(); return LibreOfficeExport(lo, src, pdf, error); }
    if (error.empty()) error = L"instale o Microsoft Office ou o LibreOffice (gratuito) para converter este arquivo";
    return false;
}

} // namespace office
