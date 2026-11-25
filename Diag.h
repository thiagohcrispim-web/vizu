// Diag.h - registro de erros, vigia de travamento e envio anônimo (só com o aceite).
//
// Tudo fica em %LOCALAPPDATA%\DesignThumbs\erros.log (no máximo ~64 KB; as linhas antigas saem).
// Com o aceite (HKCU\Software\DesignThumbs\SendReports = 1), cada tipo de erro é enviado no máximo
// uma vez por dia para o receptor (Cloudflare Worker, ver server/report-worker), que abre uma issue no GitHub.
//
// O relatório NUNCA leva nomes de arquivo, caminhos, nome de usuário ou do PC. Só: versão, Windows,
// escala, tema, recursos ligados, onde e qual erro, resultado do autoteste e um número aleatório da instalação.
#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdio>

namespace diag {

// Endereço do receptor. Vazio = nada é enviado (só o erros.log local).
inline const wchar_t* kReportHost = L"";            // ex.: L"vizu-reports.<conta>.workers.dev"
inline const wchar_t* kReportPath = L"/report";
inline const wchar_t* kCfgKey = L"Software\\DesignThumbs";

inline const wchar_t* g_version = L"?";
inline std::wstring (*g_context)() = nullptr;       // tema, escala, recursos ligados (preenchido pelo Vizu.cpp)
inline std::mutex g_mu;

inline DWORD RegDword(const wchar_t* name, DWORD def) {
    DWORD v = def, sz = sizeof(v);
    return RegGetValueW(HKEY_CURRENT_USER, kCfgKey, name, RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS ? v : def;
}
inline void RegSetDword(const wchar_t* name, DWORD v) { RegSetKeyValueW(HKEY_CURRENT_USER, kCfgKey, name, REG_DWORD, &v, sizeof(v)); }
inline bool Consent() { return RegDword(L"SendReports", 0) != 0; }
inline void SetConsent(bool on) { RegSetDword(L"SendReports", on ? 1 : 0); }

inline std::wstring Dir() {
    wchar_t p[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", p, MAX_PATH);
    if (!n || n > MAX_PATH - 40) return L"";
    std::wstring d = std::wstring(p) + L"\\DesignThumbs"; CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

// Número aleatório da instalação (não identifica a pessoa; só agrupa erros do mesmo PC).
inline std::wstring InstallId() {
    wchar_t buf[40] = {}; DWORD cb = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, kCfgKey, L"ReportId", RRF_RT_REG_SZ, nullptr, buf, &cb) == ERROR_SUCCESS && buf[0]) return buf;
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    ULONGLONG x = c.QuadPart ^ (GetTickCount64() << 21) ^ ((ULONGLONG)GetCurrentProcessId() << 40);
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33;
    swprintf(buf, 40, L"%016llx", x);
    RegSetKeyValueW(HKEY_CURRENT_USER, kCfgKey, L"ReportId", REG_SZ, buf, (DWORD)((wcslen(buf) + 1) * sizeof(wchar_t)));
    return buf;
}

inline std::string Utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
inline std::string Json(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c == '\n') o += "\\n"; else if (c == '\r') {} else if (c == '\t') o += "\\t";
        else if (c < 0x20) { char b[8]; sprintf(b, "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o + "\"";
}

inline std::wstring WindowsVersion() {
    typedef LONG(WINAPI * Fn)(OSVERSIONINFOW*);
    OSVERSIONINFOW v = {sizeof(v)};
    if (auto f = (Fn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")) f(&v);
    wchar_t b[48]; swprintf(b, 48, L"%lu.%lu.%lu", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
    return b;
}

// Endereço dentro do Vizu.exe (+0x...), para traduzir em nome de função com o build/Vizu.map.
inline std::string Where(void* addr) {
    char b[32]; ULONG_PTR base = (ULONG_PTR)GetModuleHandleW(nullptr), a = (ULONG_PTR)addr;
    if (a >= base && a - base < 0x4000000) sprintf(b, "+0x%llX", (unsigned long long)(a - base)); else sprintf(b, "?");
    return b;
}

// Envia um relatório (em segundo plano; falha em silêncio). Só com aceite e com receptor configurado.
inline void Send(const std::string& body) {
    if (!Consent() || !kReportHost[0]) return;
    std::thread([body] {
        HINTERNET s = WinHttpOpen(L"Vizu-Report", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!s) return;
        WinHttpSetTimeouts(s, 5000, 5000, 8000, 8000);
        HINTERNET c = WinHttpConnect(s, kReportHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
        HINTERNET r = c ? WinHttpOpenRequest(c, L"POST", kReportPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
        if (r) {
            const wchar_t* h = L"Content-Type: application/json\r\n";
            if (WinHttpSendRequest(r, h, (DWORD)-1, (void*)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0)) WinHttpReceiveResponse(r, nullptr);
            WinHttpCloseHandle(r);
        }
        if (c) WinHttpCloseHandle(c);
        WinHttpCloseHandle(s);
    }).detach();
}

inline std::string Report(const char* kind, const std::string& where, const std::string& detail, const std::string& selftest = "") {
    std::wstring ctx = g_context ? g_context() : L"";
    std::string sig = std::string(kind) + "|" + where;
    return "{\"app\":\"vizu\",\"version\":" + Json(Utf8(g_version)) + ",\"windows\":" + Json(Utf8(WindowsVersion())) +
           ",\"id\":" + Json(Utf8(InstallId())) + ",\"kind\":" + Json(kind) + ",\"where\":" + Json(where) +
           ",\"signature\":" + Json(sig) + ",\"detail\":" + Json(detail.substr(0, 1500)) + ",\"context\":" + Json(Utf8(ctx)) +
           ",\"selftest\":" + Json(selftest.substr(0, 1500)) + "}";
}

// Uma vez por dia por tipo de erro (assinatura).
inline bool FirstToday(const std::string& sig) {
    DWORD h = 2166136261u; for (char c : sig) h = (h ^ (unsigned char)c) * 16777619u;
    wchar_t name[48]; swprintf(name, 48, L"ReportDay_%08lx", h);
    DWORD today = (DWORD)(GetTickCount64() / 1000); FILETIME ft; GetSystemTimeAsFileTime(&ft);
    today = (DWORD)((((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 864000000000ULL);
    if (RegDword(name, 0) == today) return false;
    RegSetDword(name, today); return true;
}

// Acrescenta uma linha ao erros.log (mantém só o final do arquivo quando passa de 64 KB).
inline void Log(const std::string& line) {
    std::wstring d = Dir(); if (d.empty()) return;
    std::wstring path = d + L"\\erros.log";
    std::lock_guard<std::mutex> lk(g_mu);
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa) && fa.nFileSizeLow > 64 * 1024) {
        FILE* f = _wfopen(path.c_str(), L"rb");
        if (f) {
            std::string all(fa.nFileSizeLow, 0); size_t n = fread(all.data(), 1, all.size(), f); fclose(f); all.resize(n);
            size_t cut = all.find('\n', all.size() / 2);
            if (cut != std::string::npos && (f = _wfopen(path.c_str(), L"wb"))) { fwrite(all.data() + cut + 1, 1, all.size() - cut - 1, f); fclose(f); }
        }
    }
    FILE* f = _wfopen(path.c_str(), L"ab"); if (!f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d  %ls  %s\r\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, g_version, line.c_str());
    fclose(f);
}

// Registra um erro: no erros.log sempre; enviado uma vez por dia por tipo (com aceite).
inline void Note(const char* kind, const char* where, HRESULT hr = S_OK, const std::string& detail = "") {
    char code[24] = ""; if (hr != S_OK) sprintf(code, " hr=0x%08lX", (unsigned long)hr);
    Log(std::string(kind) + " em " + where + code + (detail.empty() ? "" : "  " + detail));
    std::string w = std::string(where) + code;
    if (Consent() && kReportHost[0] && FirstToday(std::string(kind) + "|" + w)) Send(Report(kind, w, detail));
}
// Uso: DIAG_HR("desenho", hr) dentro de uma função; o nome da função entra como "onde".
#define DIAG_HR(kind, hr) ::diag::Note(kind, __FUNCTION__, hr)

// ---------- vigia de travamento ----------
// Uma thread pergunta à janela principal a cada 2 s se ela responde. Sem resposta por 5 s: registra
// "travou" com o endereço onde a thread principal estava (traduzível pelo Vizu.map).
inline std::atomic<bool> g_watchStop{false};
inline void StartWatchdog(HWND main, DWORD mainThreadId) {
    std::thread([main, mainThreadId] {
        bool reported = false;
        while (!g_watchStop) {
            Sleep(2000);
            if (!IsWindow(main)) return;
            DWORD_PTR res;
            if (SendMessageTimeoutW(main, WM_NULL, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 5000, &res)) { reported = false; continue; }
            if (reported || g_watchStop) continue;
            reported = true;
            std::string where = "?";
            if (HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, mainThreadId)) {
                if (SuspendThread(t) != (DWORD)-1) {
                    CONTEXT cx = {}; cx.ContextFlags = CONTEXT_CONTROL;
                    if (GetThreadContext(t, &cx)) where = Where((void*)cx.Rip);
                    ResumeThread(t);
                }
                CloseHandle(t);
            }
            Note("travou", where.c_str(), S_OK, "a janela principal ficou mais de 5 s sem responder");
        }
    }).detach();
}

// Na abertura: se o Vizu fechou sozinho da última vez (crash.log), envia uma vez (com aceite).
inline void SendPendingCrash() {
    std::wstring d = Dir(); if (d.empty()) return;
    std::wstring path = d + L"\\crash.log", sent = d + L"\\crash-enviado.log";
    FILE* f = _wfopen(path.c_str(), L"rb"); if (!f) return;
    std::string text(4096, 0); text.resize(fread(text.data(), 1, text.size(), f)); fclose(f);
    Log("fechou sozinho na última vez (crash.log)");
    if (Consent() && kReportHost[0]) {
        size_t code = text.find("code=");
        std::string sig = code != std::string::npos ? text.substr(code, text.find_first_of("\r\n ", code + 16) - code) : "crash";
        Send(Report("fechou", sig, text));
    }
    DeleteFileW(sent.c_str()); MoveFileW(path.c_str(), sent.c_str());
}

} // namespace diag
