// AiBattery.inc.h - dados dos widgets "Uso de IA" (Claude e ChatGPT) e "Bateria" (aparelhos Bluetooth).
// Incluído no Widgets.inc.h. Tudo aqui só LÊ: nenhum token é renovado ou regravado, nenhum aparelho é alterado.
//
// Uso de IA: a ideia (arquivos de credencial e endpoints) segue o Codenotch (github.com/vinzdg/codenotch, MIT,
// Copyright (c) 2026 Vinz). O código abaixo foi escrito do zero para o Vizu.
//   Claude:  %USERPROFILE%\.claude\.credentials.json  -> claudeAiOauth.accessToken / expiresAt (ms)
//            GET https://api.anthropic.com/api/oauth/usage  (Authorization: Bearer, anthropic-beta: oauth-2025-04-20)
//            resposta: five_hour/seven_day { utilization (0-100), resets_at (ISO 8601) }
//   ChatGPT: %USERPROFILE%\.codex\auth.json  -> tokens.access_token / tokens.account_id
//            GET https://chatgpt.com/backend-api/wham/usage  (Authorization: Bearer, ChatGPT-Account-Id)
//            resposta: rate_limit.primary_window/secondary_window { used_percent, reset_at (s) | reset_after_seconds }
// Os dois endpoints não são documentados: se mudarem, o widget mostra "sem leitura" em vez de inventar número.
//
// Bateria: o Windows guarda o nível de bateria que o aparelho Bluetooth informa numa propriedade do dispositivo
// ({104EA319-6EE2-4701-BD47-8DDBF425BBE5}, 2). Só aparelhos que seguem o padrão Bluetooth aparecem; mouses e fones
// com receptor USB próprio (Logitech, Razer...) usam protocolos de cada marca e ficam de fora.

#include <setupapi.h>
#include <mmsystem.h>
#include <devpropdef.h>
#include <mutex>
#include <atomic>

// ---------- Uso de IA ----------

struct AiWindow { double pct = -1; ULONGLONG resetMs = 0; };          // pct < 0: sem leitura
struct AiProvider {
    std::wstring name;
    AiWindow w5h, week;
    int state = 0;            // 0 ainda lendo, 1 ok, 2 sem login, 3 login expirado, 4 sem leitura (rede/endpoint), 5 limite de consultas
    ULONGLONG fetchedMs = 0;  // última leitura boa
    ULONGLONG backoffUntil = 0;
};
static std::mutex g_aiMx;
static AiProvider g_ai[2] = {{L"Claude"}, {L"ChatGPT"}};
static std::atomic<bool> g_aiBusy{false};
static ULONGLONG g_aiNext = 0;

static ULONGLONG AiNowMs() { FILETIME f; GetSystemTimeAsFileTime(&f); return (FtU(f) - 116444736000000000ull) / 10000; }

static std::string AiReadFile(const std::wstring& path) {
    std::string s;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return s;
    LARGE_INTEGER sz{};
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart < (1 << 20)) {
        s.resize((size_t)sz.QuadPart);
        DWORD got = 0;
        if (!ReadFile(h, &s[0], (DWORD)s.size(), &got, nullptr)) got = 0;
        s.resize(got);
    }
    CloseHandle(h);
    return s;
}

static std::wstring AiHome() { wchar_t b[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"USERPROFILE", b, MAX_PATH); return n && n < MAX_PATH ? b : L""; }

// JSON mínimo: acha "key" a partir de `from` (opcionalmente só até `to`) e devolve a posição do valor.
static size_t AiJsonAt(const std::string& j, const char* key, size_t from = 0, size_t to = std::string::npos) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k, from);
    if (p == std::string::npos || (to != std::string::npos && p >= to)) return std::string::npos;
    p = j.find(':', p + k.size());
    if (p == std::string::npos) return p;
    ++p;
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == '\r' || j[p] == '\n')) ++p;
    return p;
}
static std::string AiJsonStr(const std::string& j, const char* key, size_t from = 0, size_t to = std::string::npos) {
    size_t p = AiJsonAt(j, key, from, to);
    if (p == std::string::npos || p >= j.size() || j[p] != '"') return "";
    size_t e = j.find('"', p + 1);
    return e == std::string::npos ? "" : j.substr(p + 1, e - p - 1);
}
static bool AiJsonNum(const std::string& j, const char* key, double* out, size_t from = 0, size_t to = std::string::npos) {
    size_t p = AiJsonAt(j, key, from, to);
    if (p == std::string::npos || p >= j.size()) return false;
    char* end = nullptr;
    double v = strtod(j.c_str() + p, &end);
    if (end == j.c_str() + p) return false;
    *out = v;
    return true;
}
// Fim do objeto que começa logo depois de "key": (para não ler campos do objeto vizinho).
static size_t AiJsonObjEnd(const std::string& j, size_t start) {
    if (start == std::string::npos || start >= j.size() || j[start] != '{') return std::string::npos;
    int depth = 0; bool str = false;
    for (size_t i = start; i < j.size(); ++i) {
        char c = j[i];
        if (str) { if (c == '\\') ++i; else if (c == '"') str = false; continue; }
        if (c == '"') str = true;
        else if (c == '{') ++depth;
        else if (c == '}' && --depth == 0) return i;
    }
    return std::string::npos;
}

// "2026-11-21T20:00:00.123456+00:00" ou "...Z" -> ms desde 1970 (UTC).
static ULONGLONG AiIsoMs(const std::string& s) {
    SYSTEMTIME st = {};
    int Y, M, D, h, m; double sec = 0;
    if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%lf", &Y, &M, &D, &h, &m, &sec) < 5) return 0;
    st.wYear = (WORD)Y; st.wMonth = (WORD)M; st.wDay = (WORD)D; st.wHour = (WORD)h; st.wMinute = (WORD)m; st.wSecond = (WORD)sec;
    FILETIME f;
    if (!SystemTimeToFileTime(&st, &f)) return 0;
    LONGLONG ms = (LONGLONG)((FtU(f) - 116444736000000000ull) / 10000);
    size_t t = s.find('T'), z = s.find_first_of("+-", t == std::string::npos ? 0 : t); // fuso: +03:00 / -03:00
    if (z != std::string::npos) { int oh = 0, om = 0; if (sscanf(s.c_str() + z + 1, "%d:%d", &oh, &om) >= 1) ms -= (s[z] == '-' ? -1 : 1) * (oh * 60LL + om) * 60000LL; }
    return ms > 0 ? (ULONGLONG)ms : 0;
}

// GET com cabeçalhos extras; devolve o status (0 = falha de rede) e o corpo; *retryAfter em segundos, se houver.
static DWORD AiHttpsGet(const wchar_t* host, const wchar_t* path, const std::wstring& headers, std::string* body, DWORD* retryAfter) {
    DWORD status = 0;
    HINTERNET s = WinHttpOpen(L"Vizu", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return 0;
    WinHttpSetTimeouts(s, 8000, 8000, 15000, 15000);
    HINTERNET c = WinHttpConnect(s, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (r && WinHttpSendRequest(r, headers.c_str(), (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr)) {
        DWORD sz = sizeof(status);
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        DWORD ra = 0; sz = sizeof(ra);
        if (retryAfter && WinHttpQueryHeaders(r, WINHTTP_QUERY_RETRY_AFTER | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &ra, &sz, WINHTTP_NO_HEADER_INDEX)) *retryAfter = ra;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(r, &avail) && avail && body->size() < (1u << 20)) {
            std::string chunk(avail, 0);
            DWORD got = 0;
            if (!WinHttpReadData(r, &chunk[0], avail, &got) || !got) break;
            body->append(chunk, 0, got);
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return status;
}

static std::wstring AiW(const std::string& a) { return std::wstring(a.begin(), a.end()); } // tokens são ASCII

static AiWindow AiClaudeWin(const std::string& j, const char* key) {
    AiWindow w;
    size_t p = AiJsonAt(j, key), e = AiJsonObjEnd(j, p);
    if (e == std::string::npos) return w; // null: janela sem uso
    double v;
    if (AiJsonNum(j, "utilization", &v, p, e)) w.pct = v;
    w.resetMs = AiIsoMs(AiJsonStr(j, "resets_at", p, e));
    return w;
}

static AiWindow AiCodexWin(const std::string& j, const char* key, ULONGLONG now) {
    AiWindow w;
    size_t p = AiJsonAt(j, key), e = AiJsonObjEnd(j, p);
    if (e == std::string::npos) return w;
    double v;
    if (AiJsonNum(j, "used_percent", &v, p, e)) w.pct = v;
    if (AiJsonNum(j, "reset_at", &v, p, e) && v > 0) w.resetMs = (ULONGLONG)v * 1000;
    else if (AiJsonNum(j, "reset_after_seconds", &v, p, e) && v >= 0) w.resetMs = now + (ULONGLONG)v * 1000;
    return w;
}

static void AiApply(int i, const AiProvider& got) {
    std::lock_guard<std::mutex> lk(g_aiMx);
    AiProvider& d = g_ai[i];
    d.state = got.state; d.backoffUntil = got.backoffUntil;
    if (got.state == 1) { d.w5h = got.w5h; d.week = got.week; d.fetchedMs = got.fetchedMs; } // falha: mantém a última leitura
}

static void ClaudeRenewIfNeeded(); // AiNotch.inc.h
static void AiFetchClaude() {
    AiProvider r; ULONGLONG now = AiNowMs();
    { std::lock_guard<std::mutex> lk(g_aiMx); if (now < g_ai[0].backoffUntil) return; }
    ClaudeRenewIfNeeded(); // perto de vencer: o próprio Claude Code renova antes da leitura
    std::string cred = AiReadFile(AiHome() + L"\\.claude\\.credentials.json");
    std::string tok = AiJsonStr(cred, "accessToken");
    double exp = 0;
    if (tok.empty()) { r.state = 2; AiApply(0, r); return; }
    if (AiJsonNum(cred, "expiresAt", &exp) && exp > 0 && (ULONGLONG)exp < now) { r.state = 3; AiApply(0, r); return; } // nunca manda token vencido
    std::string body; DWORD ra = 0;
    DWORD st = AiHttpsGet(L"api.anthropic.com", L"/api/oauth/usage",
                          L"Authorization: Bearer " + AiW(tok) + L"\r\nanthropic-beta: oauth-2025-04-20\r\nAccept: application/json\r\n", &body, &ra);
    if (st == 200) { r.state = 1; r.fetchedMs = now; r.w5h = AiClaudeWin(body, "five_hour"); r.week = AiClaudeWin(body, "seven_day"); }
    else if (st == 401) r.state = 3;
    else if (st == 429) { r.state = 5; r.backoffUntil = now + max<ULONGLONG>(ra, 300) * 1000; }
    else r.state = 4;
    AiApply(0, r);
}

static void AiFetchCodex() {
    AiProvider r; ULONGLONG now = AiNowMs();
    { std::lock_guard<std::mutex> lk(g_aiMx); if (now < g_ai[1].backoffUntil) return; }
    wchar_t ch[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"CODEX_HOME", ch, MAX_PATH);
    std::wstring dir = n && n < MAX_PATH ? std::wstring(ch) : AiHome() + L"\\.codex";
    std::string auth = AiReadFile(dir + L"\\auth.json");
    size_t tp = AiJsonAt(auth, "tokens"), te = AiJsonObjEnd(auth, tp);
    std::string tok = te == std::string::npos ? "" : AiJsonStr(auth, "access_token", tp, te);
    std::string acc = te == std::string::npos ? "" : AiJsonStr(auth, "account_id", tp, te);
    if (tok.empty() || acc.empty()) { r.state = 2; AiApply(1, r); return; }
    std::string body; DWORD ra = 0;
    DWORD st = AiHttpsGet(L"chatgpt.com", L"/backend-api/wham/usage",
                          L"Authorization: Bearer " + AiW(tok) + L"\r\nChatGPT-Account-Id: " + AiW(acc) + L"\r\nAccept: application/json\r\n", &body, &ra);
    if (st == 200) {
        r.state = 1; r.fetchedMs = now;
        size_t rp = AiJsonAt(body, "rate_limit"), re = AiJsonObjEnd(body, rp);
        std::string rl = re == std::string::npos ? body : body.substr(rp, re - rp + 1);
        r.w5h = AiCodexWin(rl, "primary_window", now);
        r.week = AiCodexWin(rl, "secondary_window", now);
    } else if (st == 401 || st == 403) r.state = 3;
    else if (st == 429) { r.state = 5; r.backoffUntil = now + max<ULONGLONG>(ra, 300) * 1000; }
    else r.state = 4;
    AiApply(1, r);
}

// Uma consulta a cada 5 min (como o Codex e o Codenotch); `force` no botão Atualizar, mas nunca antes de 60 s.
static void AiRefresh(HWND notify, bool force) {
    ULONGLONG now = GetTickCount64();
    if (g_aiBusy || (!force && now < g_aiNext) || (force && g_aiNext && now + 240000 < g_aiNext)) return;
    g_aiBusy = true; g_aiNext = now + 300000;
    std::thread([notify]() {
        AiFetchClaude();
        AiFetchCodex();
        g_aiBusy = false;
        if (notify) PostMessageW(notify, WM_APP + 71, 0, 0);
    }).detach();
}

// "em 2 h 10 min", "seg 14:00"
static std::wstring AiResetText(ULONGLONG resetMs) {
    if (!resetMs) return L"";
    ULONGLONG now = AiNowMs();
    if (resetMs <= now) return T(L"reiniciando", L"resetting", L"reiniciando");
    ULONGLONG mins = (resetMs - now) / 60000;
    wchar_t b[64];
    if (mins < 60) swprintf(b, 64, T(L"volta em %llu min", L"resets in %llu min", L"vuelve en %llu min"), max<ULONGLONG>(mins, 1));
    else if (mins < 24 * 60) swprintf(b, 64, T(L"volta em %llu h %02llu", L"resets in %lluh %02llu", L"vuelve en %llu h %02llu"), mins / 60, mins % 60);
    else {
        FILETIME f; ULONGLONG t = resetMs * 10000 + 116444736000000000ull; f.dwLowDateTime = (DWORD)t; f.dwHighDateTime = (DWORD)(t >> 32);
        FILETIME lf; SYSTEMTIME st; FileTimeToLocalFileTime(&f, &lf); FileTimeToSystemTime(&lf, &st);
        static const wchar_t* pt[] = {L"dom", L"seg", L"ter", L"qua", L"qui", L"sex", L"sáb"}, *en[] = {L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"}, *es[] = {L"dom", L"lun", L"mar", L"mié", L"jue", L"vie", L"sáb"};
        swprintf(b, 64, T(L"volta %ls %02d:%02d", L"resets %ls %02d:%02d", L"vuelve %ls %02d:%02d"), T(pt[st.wDayOfWeek], en[st.wDayOfWeek], es[st.wDayOfWeek]), st.wHour, st.wMinute);
    }
    return b;
}

static const wchar_t* AiStateText(int state, int i) {
    switch (state) {
    case 0: return T(L"Lendo…", L"Reading…", L"Leyendo…");
    case 2: return i == 0 ? T(L"O Claude Code ainda não está conectado.", L"Claude Code isn't connected yet.", L"Claude Code aún no está conectado.")
                          : T(L"Entre no Codex para ver o uso", L"Sign in to Codex to see usage", L"Inicia sesión en Codex para ver el uso");
    case 3: return i == 0 ? T(L"O login do Claude Code venceu.", L"Your Claude Code sign-in expired.", L"La sesión de Claude Code venció.")
                          : T(L"Login vencido: abra o Codex", L"Sign-in expired: open Codex", L"Sesión vencida: abre Codex");
    case 5: return T(L"Muitas consultas, tentando mais tarde", L"Too many requests, retrying later", L"Demasiadas consultas, reintentando");
    default: return T(L"Sem leitura agora", L"No reading right now", L"Sin lectura ahora");
    }
}

// ---------- Bateria Bluetooth ----------

struct BatDev { std::wstring name; int pct = -1; };
static std::mutex g_batMx;
static std::vector<BatDev> g_bat;
static std::atomic<bool> g_batBusy{false};

static const DEVPROPKEY kVzBtBattery = {{0x104EA319, 0x6EE2, 0x4701, {0xBD, 0x47, 0x8D, 0xDB, 0xF4, 0x25, 0xBB, 0xE5}}, 2};
static const DEVPROPKEY kVzFriendly = {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
static const DEVPROPKEY kVzName = {{0xb725f130, 0x47ef, 0x101a, {0xa5, 0xf1, 0x02, 0x60, 0x8c, 0x9e, 0xeb, 0xac}}, 10};

static std::wstring BatStrProp(HDEVINFO di, SP_DEVINFO_DATA* dd, const DEVPROPKEY& k) {
    wchar_t b[256] = {}; DEVPROPTYPE t = 0;
    if (SetupDiGetDevicePropertyW(di, dd, &k, &t, (BYTE*)b, sizeof(b) - sizeof(wchar_t), nullptr, 0) && t == DEVPROP_TYPE_STRING) return b;
    return L"";
}

static void BatScan() {
    std::vector<BatDev> out;
    HDEVINFO di = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (di != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA dd = {sizeof(dd)};
        for (DWORD i = 0; SetupDiEnumDeviceInfo(di, i, &dd); ++i) {
            BYTE lvl = 0; DEVPROPTYPE t = 0;
            if (!SetupDiGetDevicePropertyW(di, &dd, &kVzBtBattery, &t, &lvl, sizeof(lvl), nullptr, 0) || t != DEVPROP_TYPE_BYTE || lvl > 100) continue;
            std::wstring n = BatStrProp(di, &dd, kVzFriendly);
            if (n.empty()) n = BatStrProp(di, &dd, kVzName);
            if (n.empty()) continue;
            bool dup = false; // um fone aparece em vários nós (mãos-livres, áudio...): fica um por nome
            for (auto& o : out) if (!_wcsicmp(o.name.c_str(), n.c_str())) { o.pct = max(o.pct, (int)lvl); dup = true; }
            if (!dup) out.push_back({n, lvl});
        }
        SetupDiDestroyDeviceInfoList(di);
    }
    std::sort(out.begin(), out.end(), [](const BatDev& a, const BatDev& b) { return a.pct < b.pct; }); // mais fraco primeiro
    std::lock_guard<std::mutex> lk(g_batMx);
    g_bat.swap(out);
}

static void BatRefresh(HWND notify) {
    if (g_batBusy) return;
    g_batBusy = true;
    std::thread([notify]() { BatScan(); g_batBusy = false; if (notify) PostMessageW(notify, WM_APP + 71, 0, 0); }).detach();
}

// Ícone pelo nome (o Windows não diz de forma confiável o tipo do aparelho).
static const char* BatIcon(const std::wstring& n) {
    std::wstring l = n; for (auto& c : l) c = towlower(c);
    auto has = [&](const wchar_t* w) { return l.find(w) != std::wstring::npos; };
    if (has(L"mouse") || has(L"mx ") || has(L"mx master") || has(L"trackpad")) return "M12 2a6 6 0 0 0-6 6v8a6 6 0 0 0 12 0V8a6 6 0 0 0-6-6zM12 6v4";
    if (has(L"keyboard") || has(L"teclado") || has(L"keys")) return "M2 6h20v12H2zM6 10h.01M10 10h.01M14 10h.01M18 10h.01M7 14h10";
    if (has(L"controller") || has(L"gamepad") || has(L"xbox") || has(L"dualsense") || has(L"dualshock") || has(L"controle"))
        return "M6 12h4M8 10v4M15 13h.01M18 11h.01M17.32 5H6.68a4 4 0 0 0-3.98 3.59L2 15a3 3 0 0 0 5.2 2.1L9 15h6l1.8 2.1A3 3 0 0 0 22 15l-.7-6.41A4 4 0 0 0 17.32 5z";
    return "M3 18v-6a9 9 0 0 1 18 0v6M21 19a2 2 0 0 1-2 2h-1a2 2 0 0 1-2-2v-3a2 2 0 0 1 2-2h3zM3 19a2 2 0 0 0 2 2h1a2 2 0 0 0 2-2v-3a2 2 0 0 0-2-2H3z"; // fone
}
