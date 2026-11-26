// IcalAgenda.inc.h - eventos do Google Agenda (ou qualquer agenda) via link iCal secreto, SÓ LEITURA.
// Incluído no Vizu.cpp logo depois do Clock.inc.h (usa AgendaItem, DataPath, Minutes) e antes do Agenda.inc.h.
//
// - O link fica em %LOCALAPPDATA%\DesignThumbs\agenda-ical.bin, criptografado com DPAPI (só o usuário do Windows lê).
// - O último .ics baixado fica em agenda-ical.ics, para a agenda funcionar sem internet.
// - Baixa numa thread a cada 20 min; a tela nunca espera a rede.
// - Os eventos externos NÃO entram no agenda.txt: LoadAgendaView() devolve os locais primeiro (mesmos índices de
//   antes, que a edição usa) e os externos depois, com ext = true. Clique, edição e exclusão ignoram os externos.
// - Suporta: horário UTC (Z), TZID (pelo deslocamento do VTIMEZONE do próprio arquivo), horário flutuante,
//   RRULE DAILY/WEEKLY/MONTHLY/YEARLY com INTERVAL, COUNT, UNTIL, BYDAY (inclusive 2TU / -1FR no mensal),
//   EXDATE, RECURRENCE-ID (ocorrência alterada) e STATUS:CANCELLED. Eventos de dia inteiro ficam de fora (a
//   linha do tempo é por hora).

#include <wincrypt.h>
#include <set>
#include <thread>
#include <algorithm>
#include <map>
#include <atomic>

namespace ical {

static std::mutex g_mx;
static std::vector<AgendaItem> g_items;      // protegido por g_mx
static std::atomic<bool> g_busy{false};
static ULONGLONG g_lastTry = 0;               // GetTickCount64 da última tentativa
static bool g_loadedDisk = false;
static std::wstring g_status;                 // texto para as Configurações (protegido por g_mx)

static std::wstring UrlFile() { return DataPath(L"agenda-ical.bin"); }
static std::wstring CacheFile() { return DataPath(L"agenda-ical.ics"); }

// ---------- link (DPAPI) ----------
static std::wstring GetUrl() {
    std::wstring url;
    FILE* f = _wfopen(UrlFile().c_str(), L"rb");
    if (!f) return url;
    std::string blob; char b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof(b), f)) > 0 && blob.size() < (1u << 20)) blob.append(b, n);
    fclose(f);
    DATA_BLOB in{(DWORD)blob.size(), (BYTE*)blob.data()}, out{};
    if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        url.assign((const wchar_t*)out.pbData, out.cbData / sizeof(wchar_t));
        LocalFree(out.pbData);
    }
    return url;
}
static bool HasUrl() { return GetFileAttributesW(UrlFile().c_str()) != INVALID_FILE_ATTRIBUTES; }

// webcal:// vira https://; só aceita https (o link do Google é https). Vazio = desconectar.
static std::wstring NormalizeUrl(std::wstring u) {
    while (!u.empty() && iswspace(u.back())) u.pop_back();
    while (!u.empty() && iswspace(u.front())) u.erase(u.begin());
    if (_wcsnicmp(u.c_str(), L"webcal://", 9) == 0) u = L"https://" + u.substr(9);
    return u;
}
static bool SetUrl(const std::wstring& raw) {
    std::wstring u = NormalizeUrl(raw);
    {
        std::lock_guard<std::mutex> lk(g_mx);
        g_items.clear(); g_status.clear();
    }
    g_loadedDisk = true; g_lastTry = 0;
    if (u.empty()) { DeleteFileW(UrlFile().c_str()); DeleteFileW(CacheFile().c_str()); return true; }
    if (_wcsnicmp(u.c_str(), L"https://", 8) != 0) return false;
    DATA_BLOB in{(DWORD)(u.size() * sizeof(wchar_t)), (BYTE*)u.data()}, out{};
    if (!CryptProtectData(&in, L"Vizu iCal", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
    std::wstring tmp = UrlFile() + L".tmp";
    FILE* f = _wfopen(tmp.c_str(), L"wb");
    bool ok = f && fwrite(out.pbData, 1, out.cbData, f) == out.cbData;
    if (f && fclose(f) != 0) ok = false;
    LocalFree(out.pbData);
    ok = ok && MoveFileExW(tmp.c_str(), UrlFile().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) DeleteFileW(tmp.c_str());
    DeleteFileW(CacheFile().c_str());
    return ok;
}

// ---------- download ----------
static std::string Download(const std::wstring& url, DWORD* statusOut) {
    std::string body; *statusOut = 0;
    URL_COMPONENTS uc = {sizeof(uc)};
    wchar_t host[256] = {}, path[4096] = {};
    uc.lpszHostName = host; uc.dwHostNameLength = ARRAYSIZE(host);
    uc.lpszUrlPath = path; uc.dwUrlPathLength = ARRAYSIZE(path);
    wchar_t extra[4096] = {}; uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = ARRAYSIZE(extra);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS) return body;
    std::wstring full = std::wstring(path) + extra;
    HINTERNET s = WinHttpOpen(L"Vizu-Agenda", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return body;
    WinHttpSetTimeouts(s, 8000, 8000, 15000, 15000);
    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", full.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (r && WinHttpSendRequest(r, L"Accept: text/calendar\r\n", (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr)) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        *statusOut = status;
        if (status == 200) {
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(r, &avail) && avail && body.size() < (16u << 20)) {
                std::string chunk(avail, 0); DWORD got = 0;
                if (!WinHttpReadData(r, &chunk[0], avail, &got) || !got) break;
                body.append(chunk, 0, got);
            }
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return body;
}

// ---------- datas ----------
// Dia civil <-> número de dias (algoritmo de Howard Hinnant), sem depender do fuso do Windows.
static long long DaysFromCivil(int y, int m, int d) {
    y -= m <= 2; long long era = (y >= 0 ? y : y - 399) / 400; unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}
static void CivilFromDays(long long z, int& y, int& m, int& d) {
    z += 719468; long long era = (z >= 0 ? z : z - 146096) / 146097; unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; long long yy = (long long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100); unsigned mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1); m = (int)(mp < 10 ? mp + 3 : mp - 9); y = (int)(yy + (m <= 2));
}
static int WeekdayOf(long long days) { long long w = (days + 4) % 7; return (int)(w < 0 ? w + 7 : w); } // 0 = domingo
static int DaysInMonth(int y, int m) { return (int)(DaysFromCivil(m == 12 ? y + 1 : y, m == 12 ? 1 : m + 1, 1) - DaysFromCivil(y, m, 1)); }

struct Stamp { long long day = 0; int min = 0; bool valid = false, allDay = false, utc = false; std::wstring tzid; };

// "20261125T143000Z", "20261125T143000", "20261125"
static Stamp ParseStamp(const std::wstring& v, const std::wstring& params) {
    Stamp s; int y, mo, d, h = 0, mi = 0, se = 0;
    if (v.size() < 8 || swscanf(v.c_str(), L"%4d%2d%2d", &y, &mo, &d) != 3) return s;
    s.day = DaysFromCivil(y, mo, d); s.valid = true;
    if (v.size() >= 15 && v[8] == L'T' && swscanf(v.c_str() + 9, L"%2d%2d%2d", &h, &mi, &se) >= 2) { s.min = h * 60 + mi; s.utc = v.back() == L'Z'; }
    else s.allDay = true;
    size_t p = params.find(L"TZID=");
    if (p != std::wstring::npos) { size_t e = params.find(L';', p); s.tzid = params.substr(p + 5, e == std::wstring::npos ? std::wstring::npos : e - p - 5); if (!s.tzid.empty() && s.tzid[0] == L'"') s.tzid = s.tzid.substr(1, s.tzid.size() - 2); }
    return s;
}

// Minutos UTC desde 1970 -> SYSTEMTIME no fuso do Windows.
static bool UtcToLocal(long long utcMin, SYSTEMTIME* out) {
    long long day = utcMin / 1440, rem = utcMin % 1440; if (rem < 0) { rem += 1440; --day; }
    int y, m, d; CivilFromDays(day, y, m, d);
    SYSTEMTIME u = {}; u.wYear = (WORD)y; u.wMonth = (WORD)m; u.wDay = (WORD)d; u.wHour = (WORD)(rem / 60); u.wMinute = (WORD)(rem % 60);
    return SystemTimeToTzSpecificLocalTime(nullptr, &u, out) != FALSE;
}
static long long LocalNowUtcOffsetMin() { // deslocamento atual do Windows (para horário flutuante)
    SYSTEMTIME l, u; GetLocalTime(&l); GetSystemTime(&u);
    return (DaysFromCivil(l.wYear, l.wMonth, l.wDay) - DaysFromCivil(u.wYear, u.wMonth, u.wDay)) * 1440 + (l.wHour - u.wHour) * 60 + (l.wMinute - u.wMinute);
}

// ---------- leitura do .ics ----------
struct Prop { std::wstring name, params, value; };
struct Event { std::map<std::wstring, Prop> p; std::vector<Prop> exdates; };

static std::wstring Unescape(const std::wstring& v) {
    std::wstring o; o.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == L'\\' && i + 1 < v.size()) { wchar_t n = v[++i]; o += (n == L'n' || n == L'N') ? L' ' : n; }
        else o += v[i];
    }
    return o;
}
static std::map<std::wstring, std::wstring> RuleParts(const std::wstring& r) {
    std::map<std::wstring, std::wstring> m; size_t i = 0;
    while (i < r.size()) {
        size_t e = r.find(L';', i); if (e == std::wstring::npos) e = r.size();
        std::wstring kv = r.substr(i, e - i); size_t q = kv.find(L'=');
        if (q != std::wstring::npos) m[kv.substr(0, q)] = kv.substr(q + 1);
        i = e + 1;
    }
    return m;
}
static int DayCode(const std::wstring& c) {
    static const wchar_t* k[] = {L"SU", L"MO", L"TU", L"WE", L"TH", L"FR", L"SA"};
    for (int i = 0; i < 7; ++i) if (c == k[i]) return i;
    return -1;
}

static std::vector<AgendaItem> Parse(const std::string& raw) {
    std::vector<AgendaItem> out;
    // UTF-8 -> UTF-16
    int wn = MultiByteToWideChar(CP_UTF8, 0, raw.data(), (int)raw.size(), nullptr, 0);
    std::wstring w(wn > 0 ? wn : 0, 0);
    if (wn > 0) MultiByteToWideChar(CP_UTF8, 0, raw.data(), (int)raw.size(), &w[0], wn);
    // linhas, desfazendo a "dobra" (linha que começa com espaço/tab continua a anterior)
    std::vector<std::wstring> lines; std::wstring cur;
    for (size_t i = 0; i <= w.size(); ++i) {
        if (i == w.size() || w[i] == L'\n') {
            if (!cur.empty() && cur.back() == L'\r') cur.pop_back();
            if (!cur.empty() && (cur[0] == L' ' || cur[0] == L'\t') && !lines.empty()) lines.back() += cur.substr(1);
            else lines.push_back(cur);
            cur.clear();
        } else cur += w[i];
    }
    std::map<std::wstring, int> tzOffset;     // TZID -> deslocamento (min) do bloco STANDARD
    std::vector<Event> events;
    Event ev; bool inEv = false, inTz = false, inStd = false, inDay = false; std::wstring tzid; int dayOff = INT_MIN, stdOff = INT_MIN;
    auto offMin = [](const std::wstring& v) { int sign = v.size() && v[0] == L'-' ? -1 : 1, hh = 0, mm = 0; if (v.size() >= 5) swscanf(v.c_str() + 1, L"%2d%2d", &hh, &mm); return sign * (hh * 60 + mm); };
    for (auto& ln : lines) {
        size_t colon = std::wstring::npos; bool q = false;
        for (size_t i = 0; i < ln.size(); ++i) { if (ln[i] == L'"') q = !q; else if (ln[i] == L':' && !q) { colon = i; break; } }
        if (colon == std::wstring::npos) continue;
        std::wstring head = ln.substr(0, colon), value = ln.substr(colon + 1), name = head, params;
        size_t sc = head.find(L';'); if (sc != std::wstring::npos) { name = head.substr(0, sc); params = head.substr(sc + 1); }
        for (auto& ch : name) ch = towupper(ch);
        if (name == L"BEGIN") {
            if (value == L"VEVENT") { ev = Event(); inEv = true; }
            else if (value == L"VTIMEZONE") { inTz = true; tzid.clear(); stdOff = dayOff = INT_MIN; }
            else if (value == L"STANDARD") inStd = true; else if (value == L"DAYLIGHT") inDay = true;
            continue;
        }
        if (name == L"END") {
            if (value == L"VEVENT" && inEv) { events.push_back(ev); inEv = false; }
            else if (value == L"VTIMEZONE") { if (!tzid.empty()) tzOffset[tzid] = stdOff != INT_MIN ? stdOff : dayOff != INT_MIN ? dayOff : 0; inTz = false; }
            else if (value == L"STANDARD") inStd = false; else if (value == L"DAYLIGHT") inDay = false;
            continue;
        }
        if (inTz) {
            if (name == L"TZID") tzid = value;
            else if (name == L"TZOFFSETTO") { if (inStd) stdOff = offMin(value); else if (inDay) dayOff = offMin(value); }
            continue;
        }
        if (!inEv) continue;
        if (name == L"EXDATE") ev.exdates.push_back({name, params, value});
        else if (!ev.p.count(name)) ev.p[name] = {name, params, value};
    }

    const long long floatOff = LocalNowUtcOffsetMin();
    auto toUtc = [&](const Stamp& s, long long day, int min) -> long long { // dia/hora "de parede" do evento -> minutos UTC
        long long m = day * 1440 + min;
        if (s.utc) return m;
        if (!s.tzid.empty()) { auto it = tzOffset.find(s.tzid); if (it != tzOffset.end()) return m - it->second; }
        return m - floatOff;
    };
    auto stampUtc = [&](const Prop& pr) -> long long { Stamp s = ParseStamp(pr.value, pr.params); return s.valid ? toUtc(s, s.day, s.min) : LLONG_MIN; };

    SYSTEMTIME nowSt; GetSystemTime(&nowSt);
    const long long today = DaysFromCivil(nowSt.wYear, nowSt.wMonth, nowSt.wDay);
    const long long winA = (today - 45) * 1440, winB = (today + 180) * 1440; // janela mostrada: 45 dias atrás a 6 meses à frente

    std::set<std::wstring> overridden; // "UID|minUTC" de ocorrências trocadas por RECURRENCE-ID
    for (auto& e : events) if (e.p.count(L"RECURRENCE-ID")) { long long m = stampUtc(e.p[L"RECURRENCE-ID"]); if (m != LLONG_MIN) overridden.insert(e.p[L"UID"].value + L"|" + std::to_wstring(m)); }

    auto emit = [&](const std::wstring& title, long long utcMin, int dur) {
        if (utcMin < winA || utcMin > winB) return;
        AgendaItem a; a.when = {};
        if (!UtcToLocal(utcMin, &a.when)) return;
        a.title = title; a.dur = max(5, min(600, dur)); a.alerts = kAlert10 | kAlert5; a.ext = true;
        out.push_back(a);
    };

    for (auto& e : events) {
        if (!e.p.count(L"DTSTART")) continue;
        if (e.p.count(L"STATUS") && _wcsicmp(e.p[L"STATUS"].value.c_str(), L"CANCELLED") == 0) continue;
        Stamp st = ParseStamp(e.p[L"DTSTART"].value, e.p[L"DTSTART"].params);
        if (!st.valid || st.allDay) continue; // dia inteiro fica de fora
        long long startUtc = toUtc(st, st.day, st.min);
        int dur = 30;
        if (e.p.count(L"DTEND")) { long long en = stampUtc(e.p[L"DTEND"]); if (en != LLONG_MIN && en > startUtc) dur = (int)min(600LL, en - startUtc); }
        else if (e.p.count(L"DURATION")) { // PT1H30M, P1D...
            const std::wstring& d = e.p[L"DURATION"].value; long long n = 0, tot = 0;
            for (wchar_t ch : d) { if (iswdigit(ch)) n = n * 10 + (ch - L'0'); else { if (ch == L'W') tot += n * 10080; else if (ch == L'D') tot += n * 1440; else if (ch == L'H') tot += n * 60; else if (ch == L'M') tot += n; n = 0; } }
            if (tot > 0) dur = (int)min(600LL, tot);
        }
        std::wstring title = e.p.count(L"SUMMARY") ? Unescape(e.p[L"SUMMARY"].value) : std::wstring();
        if (title.empty()) title = T(L"(sem título)", L"(untitled)", L"(sin título)");
        std::wstring uid = e.p.count(L"UID") ? e.p[L"UID"].value : std::wstring();

        if (!e.p.count(L"RRULE") || e.p.count(L"RECURRENCE-ID")) { emit(title, startUtc, dur); continue; }

        std::set<long long> ex;
        for (auto& x : e.exdates) { // EXDATE pode ter vários valores separados por vírgula
            size_t i = 0; const std::wstring& v = x.value;
            while (i <= v.size()) { size_t c = v.find(L',', i); if (c == std::wstring::npos) c = v.size();
                Stamp s = ParseStamp(v.substr(i, c - i), x.params); if (s.valid) ex.insert(s.allDay ? toUtc(st, s.day, st.min) : toUtc(s, s.day, s.min)); i = c + 1; }
        }
        auto R = RuleParts(e.p[L"RRULE"].value);
        std::wstring freq = R[L"FREQ"];
        int interval = R.count(L"INTERVAL") ? max(1, _wtoi(R[L"INTERVAL"].c_str())) : 1;
        int count = R.count(L"COUNT") ? _wtoi(R[L"COUNT"].c_str()) : 0;
        long long untilUtc = LLONG_MAX;
        if (R.count(L"UNTIL")) { Stamp u = ParseStamp(R[L"UNTIL"], L""); if (u.valid) untilUtc = u.allDay ? toUtc(st, u.day, 1439) : (u.utc ? u.day * 1440 + u.min : toUtc(st, u.day, u.min)); }
        // BYDAY: lista de (n, dia). n = 0 vale todo aquele dia da semana; 2TU = segunda terça; -1FR = última sexta.
        std::vector<std::pair<int, int>> byday;
        if (R.count(L"BYDAY")) {
            std::wstring v = R[L"BYDAY"]; size_t i = 0;
            while (i < v.size()) { size_t c = v.find(L',', i); if (c == std::wstring::npos) c = v.size();
                std::wstring t = v.substr(i, c - i); int n = 0; if (t.size() > 2) n = _wtoi(t.substr(0, t.size() - 2).c_str());
                int dc = t.size() >= 2 ? DayCode(t.substr(t.size() - 2)) : -1; if (dc >= 0) byday.push_back({n, dc}); i = c + 1; }
        }
        int y0, m0, d0; CivilFromDays(st.day, y0, m0, d0);
        long long weekStart0 = st.day - ((WeekdayOf(st.day) + 6) % 7); // segunda-feira da semana do início
        long long lastDay = min(today + 181, (untilUtc == LLONG_MAX ? LLONG_MAX : untilUtc / 1440 + 2));
        if (count > 0) lastDay = today + 181; // com COUNT precisa contar desde o início
        int produced = 0;
        for (long long day = st.day; day <= lastDay && day - st.day < 365LL * 30; ++day) {
            int y, m, d; CivilFromDays(day, y, m, d);
            int wd = WeekdayOf(day);
            bool match = false;
            if (freq == L"DAILY") match = (day - st.day) % interval == 0 && (byday.empty() || std::any_of(byday.begin(), byday.end(), [&](auto& b) { return b.second == wd; }));
            else if (freq == L"WEEKLY") {
                long long wk = (day - ((wd + 6) % 7) - weekStart0) / 7;
                match = wk % interval == 0 && (byday.empty() ? wd == WeekdayOf(st.day) : std::any_of(byday.begin(), byday.end(), [&](auto& b) { return b.second == wd; }));
            } else if (freq == L"MONTHLY") {
                int months = (y - y0) * 12 + (m - m0);
                if (months % interval == 0) {
                    if (byday.empty()) match = d == d0;
                    else for (auto& b : byday) {
                        if (b.second != wd) continue;
                        int nth = (d - 1) / 7 + 1, fromEnd = (DaysInMonth(y, m) - d) / 7 + 1;
                        if (b.first == 0 || b.first == nth || b.first == -fromEnd) { match = true; break; }
                    }
                }
            } else if (freq == L"YEARLY") match = (y - y0) % interval == 0 && m == m0 && d == d0;
            else break; // frequência não suportada: só a primeira ocorrência
            if (!match) continue;
            long long occ = toUtc(st, day, st.min);
            if (occ < startUtc) continue;
            if (occ > untilUtc) break;
            ++produced;
            if (!ex.count(occ) && !overridden.count(uid + L"|" + std::to_wstring(occ))) emit(title, occ, dur);
            if (count > 0 && produced >= count) break;
        }
        if (freq != L"DAILY" && freq != L"WEEKLY" && freq != L"MONTHLY" && freq != L"YEARLY") emit(title, startUtc, dur);
    }
    return out;
}

static std::string ReadCache() {
    std::string s; FILE* f = _wfopen(CacheFile().c_str(), L"rb");
    if (!f) return s;
    char b[16384]; size_t n; while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
    fclose(f); return s;
}

// Dispara o download se passaram 20 min (ou 2 min depois de uma falha). Nunca bloqueia.
static void Refresh(bool force = false) {
    if (!g_loadedDisk) { // primeira vez: mostra o que estava salvo, sem esperar a rede
        g_loadedDisk = true;
        if (HasUrl()) { std::string c = ReadCache(); if (!c.empty()) { auto v = Parse(c); std::lock_guard<std::mutex> lk(g_mx); g_items = std::move(v); } }
    }
    if (!HasUrl() || g_busy) return;
    ULONGLONG now = GetTickCount64();
    bool failed; { std::lock_guard<std::mutex> lk(g_mx); failed = !g_status.empty() && g_status[0] == L'!'; }
    if (!force && g_lastTry && now - g_lastTry < (failed ? 120000ull : 1200000ull)) return;
    g_lastTry = now; g_busy = true;
    std::thread([] {
        std::wstring url = GetUrl();
        DWORD status = 0;
        std::string body = url.empty() ? std::string() : Download(url, &status);
        bool ok = body.find("BEGIN:VCALENDAR") != std::string::npos;
        std::vector<AgendaItem> v;
        if (ok) {
            v = Parse(body);
            std::wstring tmp = CacheFile() + L".tmp"; FILE* f = _wfopen(tmp.c_str(), L"wb");
            bool w = f && fwrite(body.data(), 1, body.size(), f) == body.size(); if (f && fclose(f) != 0) w = false;
            if (w) MoveFileExW(tmp.c_str(), CacheFile().c_str(), MOVEFILE_REPLACE_EXISTING); else DeleteFileW(tmp.c_str());
        }
        {
            std::lock_guard<std::mutex> lk(g_mx);
            if (ok) { g_items = std::move(v); g_status = std::to_wstring(g_items.size()); }
            else g_status = L"!" + std::to_wstring(status); // "!" = falhou (o resto é o código HTTP; 0 = sem conexão)
        }
        g_busy = false;
    }).detach();
}

static std::vector<AgendaItem> Items() { Refresh(); std::lock_guard<std::mutex> lk(g_mx); return g_items; }

// Linha de status para as Configurações.
static std::wstring StatusText() {
    if (!HasUrl()) return T(L"Mostra seus eventos do Google Agenda (só leitura)", L"Shows your Google Calendar events (read-only)", L"Muestra tus eventos de Google Calendar (solo lectura)");
    std::wstring st; { std::lock_guard<std::mutex> lk(g_mx); st = g_status; }
    if (st.empty()) return T(L"Conectada · sincronizando…", L"Connected · syncing…", L"Conectada · sincronizando…");
    if (st[0] == L'!') {
        if (st == L"!404" || st == L"!403" || st == L"!401") return T(L"Link inválido ou redefinido. Cole o link de novo", L"Link is invalid or was reset. Paste it again", L"Enlace inválido o restablecido. Pégalo de nuevo");
        return T(L"Sem conexão agora · usando a última cópia", L"Offline right now · using the last copy", L"Sin conexión ahora · usando la última copia");
    }
    wchar_t b[128]; swprintf(b, 128, T(L"Conectada · %ls eventos · atualiza a cada 20 min", L"Connected · %ls events · refreshes every 20 min", L"Conectada · %ls eventos · se actualiza cada 20 min"), st.c_str());
    return b;
}

} // namespace ical

// Locais primeiro (índices iguais aos do agenda.txt, usados na edição), externos depois (ext = true, só leitura).
static std::vector<AgendaItem> LoadAgendaView(int* nLocal) {
    std::vector<AgendaItem> v = LoadAgenda();
    if (nLocal) *nLocal = (int)v.size();
    auto e = ical::Items();
    v.insert(v.end(), e.begin(), e.end());
    return v;
}
