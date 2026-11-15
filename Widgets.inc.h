// Widgets.inc.h - widgets flutuantes da Limpeza: espaço em disco e desempenho ("o PC está lento? por quê?").
// Protótipo: docs/design/prototipo/Limpeza-B9 (disco) e Limpeza-B11 (desempenho).
// Janelas em camadas como o relógio: arrastam para qualquer lugar, abrem ao passar o mouse, nunca roubam o foco.
// Incluído no Vizu.cpp depois do CleanUi.inc.h.

#include <pdh.h>
#include <psapi.h>
#include <map>

enum { kWDisk, kWPerf, kWCount };

struct ProcUse { std::wstring exe; double cpu = 0; ULONGLONG mem = 0, io = 0; bool window = false; };

struct Widget {
    HWND wnd = nullptr;
    bool open = false, pressed = false, dragging = false;
    POINT down{};
    int hot = 0;                      // 1 botão principal, 2 "Fechar" do maior culpado
    D2D1_RECT_F rBtn{}, rClose{};
    LONG ax = 0, ay = 0;              // posição do cartão fechado (canto de cima, à esquerda)
    UINT w = 0, h = 0;
    ULONGLONG leaveAt = 0;
};
static Widget g_wd[kWCount];

// ---------- medição do desempenho ----------

struct Perf {
    double cpu = 0, mem = 0, disk = 0;   // %
    int hiCpu = 0, hiMem = 0, hiDisk = 0, calm = 0;
    bool slow = false;
    int cause = 0;                        // 1 CPU, 2 memória, 3 disco
    std::vector<ProcUse> top;             // ordenado pela causa
    ULONGLONG lastIdle = 0, lastKernel = 0, lastUser = 0;
    PDH_HQUERY q = nullptr; PDH_HCOUNTER diskIdle = nullptr;
    std::map<DWORD, std::pair<ULONGLONG, ULONGLONG>> prev; // pid -> (tempo de CPU, bytes de E/S)
    ULONGLONG prevTick = 0;
};
static Perf g_perf;

static ULONGLONG FtU(const FILETIME& f) { return (ULONGLONG)f.dwHighDateTime << 32 | f.dwLowDateTime; }

// Processos mais pesados, agrupados pelo .exe (o Chrome tem dezenas de processos).
static void PerfProcesses() {
    std::set<DWORD> withWindow;
    EnumWindows([](HWND w, LPARAM lp) -> BOOL { if (IsWindowVisible(w) && !GetWindow(w, GW_OWNER)) { DWORD pid = 0; GetWindowThreadProcessId(w, &pid); ((std::set<DWORD>*)lp)->insert(pid); } return TRUE; }, (LPARAM)&withWindow);
    ULONGLONG now = GetTickCount64(), dt = g_perf.prevTick ? now - g_perf.prevTick : 0;
    g_perf.prevTick = now;
    SYSTEM_INFO si; GetSystemInfo(&si);
    std::map<std::wstring, ProcUse> by;
    std::map<DWORD, std::pair<ULONGLONG, ULONGLONG>> cur;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe = {sizeof(pe)};
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (pe.th32ProcessID <= 4) continue; // Ocioso e Sistema
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) continue;
        FILETIME c, e, k, u; IO_COUNTERS io = {}; PROCESS_MEMORY_COUNTERS_EX pm = {}; pm.cb = sizeof(pm);
        ULONGLONG t = GetProcessTimes(h, &c, &e, &k, &u) ? FtU(k) + FtU(u) : 0;
        ULONGLONG iob = GetProcessIoCounters(h, &io) ? io.ReadTransferCount + io.WriteTransferCount : 0;
        K32GetProcessMemoryInfo(h, (PROCESS_MEMORY_COUNTERS*)&pm, sizeof(pm));
        CloseHandle(h);
        cur[pe.th32ProcessID] = {t, iob};
        std::wstring exe = clean::LowerPath(pe.szExeFile);
        ProcUse& p = by[exe];
        p.exe = pe.szExeFile;
        p.mem += pm.PrivateUsage;
        p.window |= withWindow.count(pe.th32ProcessID) > 0;
        auto f = g_perf.prev.find(pe.th32ProcessID);
        if (dt && f != g_perf.prev.end()) {
            if (t >= f->second.first) p.cpu += (double)(t - f->second.first) / (dt * 10000.0) / si.dwNumberOfProcessors * 100.0;
            if (iob >= f->second.second) p.io += iob - f->second.second;
        }
    }
    CloseHandle(snap);
    g_perf.prev.swap(cur);
    g_perf.top.clear();
    for (auto& x : by) g_perf.top.push_back(x.second);
    int cause = g_perf.cause ? g_perf.cause : 1;
    std::sort(g_perf.top.begin(), g_perf.top.end(), [cause](const ProcUse& a, const ProcUse& b) {
        return cause == 2 ? a.mem > b.mem : cause == 3 ? a.io > b.io : a.cpu > b.cpu;
    });
    if (g_perf.top.size() > 3) g_perf.top.resize(3);
}

// Uma amostra por segundo. "Lento" só depois de 5 s seguidos acima do limite (um pico não conta); volta depois de 5 s calmo.
static void PerfSample() {
    FILETIME idle, kern, user;
    if (GetSystemTimes(&idle, &kern, &user)) {
        ULONGLONG i = FtU(idle), k = FtU(kern), u = FtU(user);
        if (g_perf.lastKernel) {
            ULONGLONG di = i - g_perf.lastIdle, dk = k - g_perf.lastKernel, du = u - g_perf.lastUser, tot = dk + du;
            g_perf.cpu = tot ? 100.0 * (double)(tot - di) / tot : 0;
        }
        g_perf.lastIdle = i; g_perf.lastKernel = k; g_perf.lastUser = u;
    }
    MEMORYSTATUSEX ms = {sizeof(ms)};
    if (GlobalMemoryStatusEx(&ms)) g_perf.mem = ms.dwMemoryLoad;
    if (!g_perf.q && PdhOpenQueryW(nullptr, 0, &g_perf.q) == ERROR_SUCCESS)
        PdhAddEnglishCounterW(g_perf.q, L"\\PhysicalDisk(_Total)\\% Idle Time", 0, &g_perf.diskIdle);
    if (g_perf.q && PdhCollectQueryData(g_perf.q) == ERROR_SUCCESS && g_perf.diskIdle) {
        PDH_FMT_COUNTERVALUE v;
        if (PdhGetFormattedCounterValue(g_perf.diskIdle, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, nullptr, &v) == ERROR_SUCCESS) g_perf.disk = max(0.0, min(100.0, 100.0 - v.doubleValue));
    }
    g_perf.hiCpu = g_perf.cpu >= 85 ? g_perf.hiCpu + 1 : 0;
    g_perf.hiMem = g_perf.mem >= 90 ? g_perf.hiMem + 1 : 0;
    g_perf.hiDisk = g_perf.disk >= 90 ? g_perf.hiDisk + 1 : 0;
    int cause = g_perf.hiCpu >= 5 ? 1 : g_perf.hiMem >= 5 ? 2 : g_perf.hiDisk >= 5 ? 3 : 0;
    if (cause) { g_perf.slow = true; g_perf.cause = cause; g_perf.calm = 0; }
    else if (g_perf.slow && g_perf.cpu < 70 && g_perf.mem < 85 && g_perf.disk < 70 && ++g_perf.calm >= 5) { g_perf.slow = false; g_perf.cause = 0; }
    static int every = 0;
    if (g_perf.slow || g_wd[kWPerf].open || ++every % 5 == 0) PerfProcesses(); // ninguém olhando: só a cada 5 s
}

// ---------- desenho ----------

static std::wstring PerfCulprit() {
    if (!g_perf.slow || g_perf.top.empty()) return L"";
    const ProcUse& p = g_perf.top[0];
    wchar_t b[160];
    std::wstring n = CleanProgramName(p.exe);
    if (g_perf.cause == 2) swprintf(b, 160, T(L"%ls · %ls de memória", L"%ls · %ls of memory", L"%ls · %ls de memoria"), n.c_str(), CleanSize(p.mem).c_str());
    else if (g_perf.cause == 3) swprintf(b, 160, T(L"%ls · usando o disco", L"%ls · using the disk", L"%ls · usando el disco"), n.c_str());
    else swprintf(b, 160, T(L"%ls · %.0f%% da CPU", L"%ls · %.0f%% of CPU", L"%ls · %.0f%% de la CPU"), n.c_str(), p.cpu);
    return b;
}

static bool DiskLow(const clean::Disk& d) { return d.total && ReadDword(L"DiskWarn", 1) && d.free * 100 < d.total * min(50ul, max(1ul, (unsigned long)ReadDword(L"DiskLimit", 10))); }

static void WidgetRender(int k) {
    Widget& wd = g_wd[k];
    if (!wd.wnd) return;
    const ui::Theme& th = g_theme;
    ClColors c = ClPalette(th);
    float s = GetDpiForWindow(wd.wnd) / 96.0f;
    D2D1_COLOR_F surface = th.classic ? ui::Rgba(0x353A42) : th.neon ? ui::Rgba(0x121215, 0.96f) : th.dark ? ui::Rgba(0x1F1F22, 0.97f) : ui::Rgba(0xFFFFFF, 0.97f);
    D2D1_COLOR_F hi = g_cfg.themeMode == ui::kThemeDark ? th.text : c.accent;
    float M = 14 * s; // margem para a sombra
    std::vector<clean::Disk> disks;
    if (k == kWDisk) { disks = ReadDword(L"DiskAll", 0) ? clean::AllDisks() : std::vector<clean::Disk>{clean::SystemDisk()}; if (disks.empty()) disks.push_back(clean::SystemDisk()); }
    bool alert = g_perf.slow;
    if (k == kWDisk) { alert = false; for (auto& d : disks) alert |= DiskLow(d); }
    float W = (wd.open ? (k == kWPerf ? 280 : 260) : 220) * s, H;
    if (!wd.open) H = (k == kWDisk ? 56 + (disks.size() - 1) * 30 : 60) * s; // "Todos": uma linha por disco
    else if (k == kWDisk) H = (16 + disks.size() * 44 + 12 + 22 + 10 + 36 + 16) * s;
    else H = (16 + 48 + 4 * 40 + 22 + g_perf.top.size() * 32 + 14 + 36 + 16) * s;
    UINT Wt = (UINT)(W + 2 * M), Ht = (UINT)(H + 2 * M);
    fmt::Canvas cv(Wt, Ht);
    if (!cv.rt) return;
    ID2D1RenderTarget* rt = cv.rt;
    rt->Clear(D2D1::ColorF(0, 0));
    rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ui::Painter p(rt, g_dw, th, s);
    D2D1_RECT_F card = D2D1::RectF(M, M, M + W, M + H);
    float rad = (wd.open ? 14 : 12) * s;
    p.Shadow(card, rad);
    p.Fill(card, rad, surface);
    if (alert) { if (th.neon) p.Glow(card, rad); p.Stroke(card, rad, hi, 1.5f * s); } else p.Stroke(card, rad, c.line, 1 * s);
    auto bar = [&](D2D1_RECT_F r, double pct, D2D1_COLOR_F col) {
        float hh = r.bottom - r.top; p.Fill(r, hh / 2, c.track);
        float wv = (float)((r.right - r.left) * max(0.0, min(1.0, pct / 100)));
        if (wv > 0) { p.rt->PushAxisAlignedClip(D2D1::RectF(r.left, r.top, r.left + max(wv, hh), r.bottom), D2D1_ANTIALIAS_MODE_ALIASED); p.Fill(r, hh / 2, col); p.rt->PopAxisAlignedClip(); }
    };
    float x = card.left + 14 * s, xr = card.right - 14 * s, y = card.top;
    wd.rBtn = wd.rClose = D2D1::RectF(0, 0, 0, 0);
    const char* icoDisk = "M22 12H2M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11zM6 16h.01M10 16h.01";
    const char* icoAct = "M22 12h-4l-3 9L9 3l-3 9H2";
    wchar_t b[160];
    if (k == kWDisk && !wd.open) {
        p.Feather(icoDisk, D2D1::RectF(x, card.top, x + 16 * s, card.top + 56 * s), 16 * s, alert ? hi : th.text2);
        float tx = x + 28 * s;
        for (size_t i = 0; i < disks.size(); ++i) {
            const clean::Disk& d = disks[i];
            bool low = DiskLow(d);
            float ry = card.top + i * 30 * s;
            swprintf(b, 160, L"%lc:", d.letter);
            ClText(p, b, D2D1::RectF(tx, ry + 11 * s, xr, ry + 29 * s), 13, th.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            swprintf(b, 160, T(L"%ls livres", L"%ls free", L"%ls libres"), CleanSize(d.free).c_str());
            ClText(p, b, D2D1::RectF(tx, ry + 11 * s, xr, ry + 29 * s), 13, low ? th.text : th.text2, low ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            bar(D2D1::RectF(tx, ry + 35 * s, xr, ry + 41 * s), d.total ? 100.0 * (d.total - d.free) / d.total : 0, low ? hi : c.used);
        }
    } else if (k == kWDisk) {
        y += 16 * s;
        for (auto& d : disks) {
            swprintf(b, 160, L"%lc:", d.letter);
            ClText(p, b, D2D1::RectF(x, y, xr, y + 18 * s), 13, th.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            swprintf(b, 160, T(L"%ls livres de %ls", L"%ls free of %ls", L"%ls libres de %ls"), CleanSize(d.free).c_str(), CleanSize(d.total).c_str());
            ClText(p, b, D2D1::RectF(x, y, xr, y + 18 * s), 13, th.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            bar(D2D1::RectF(x, y + 24 * s, xr, y + 30 * s), d.total ? 100.0 * (d.total - d.free) / d.total : 0, DiskLow(d) ? hi : c.used);
            y += 44 * s;
        }
        y += 2 * s; p.Fill(D2D1::RectF(x, y, xr, y + 1 * s), 0, c.line); y += 10 * s;
        DWORD found = ReadDword(L"CleanLastFound", 0xFFFFFFFF);
        if (found == 0xFFFFFFFF) wcscpy_s(b, T(L"Ainda sem análise", L"Not scanned yet", L"Aún sin análisis"));
        else if (found) swprintf(b, 160, T(L"Última análise: %ls para liberar", L"Last scan: %ls to free up", L"Último análisis: %ls para liberar"), CleanSize((ULONGLONG)found << 20).c_str());
        else wcscpy_s(b, T(L"Tudo limpo na última análise", L"All clean at the last scan", L"Todo limpio en el último análisis"));
        ClText(p, b, D2D1::RectF(x, y, xr, y + 20 * s), 12.5f, th.text2);
        y += 32 * s;
        wd.rBtn = D2D1::RectF(x, y, xr, y + 36 * s);
        ClButton(p, c, wd.rBtn, T(L"Abrir a Limpeza", L"Open Cleanup", L"Abrir Limpieza"), true, wd.hot == 1, s);
    } else if (!wd.open) {
        p.Feather(icoAct, D2D1::RectF(x, card.top, x + 16 * s, card.bottom), 16 * s, alert ? hi : th.text2);
        float tx = x + 28 * s;
        p.Fill(D2D1::RectF(tx, card.top + 17 * s, tx + 8 * s, card.top + 25 * s), 4 * s, alert ? hi : th.text2);
        ClText(p, alert ? T(L"Lento", L"Slow", L"Lento") : T(L"Tudo leve", L"All smooth", L"Todo fluido"), D2D1::RectF(tx + 14 * s, card.top + 11 * s, xr, card.top + 31 * s), 13, th.text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        std::wstring sub = alert ? PerfCulprit() : L"";
        if (sub.empty()) { swprintf(b, 160, T(L"CPU %.0f%% · Memória %.0f%%", L"CPU %.0f%% · Memory %.0f%%", L"CPU %.0f%% · Memoria %.0f%%"), g_perf.cpu, g_perf.mem); sub = b; }
        ClText(p, sub, D2D1::RectF(tx, card.top + 31 * s, xr, card.top + 49 * s), 12.5f, th.text2);
    } else {
        y += 16 * s;
        p.Fill(D2D1::RectF(x, y + 6 * s, x + 8 * s, y + 14 * s), 4 * s, alert ? hi : th.text2);
        ClText(p, alert ? T(L"O PC está lento", L"Your PC is slow", L"El PC está lento") : T(L"Tudo leve", L"All smooth", L"Todo fluido"),
               D2D1::RectF(x + 16 * s, y, xr, y + 20 * s), 16, th.text, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, L"Space Grotesk");
        const wchar_t* why = !alert ? T(L"Nada pesando agora.", L"Nothing heavy right now.", L"Nada pesado ahora.")
                           : g_perf.cause == 2 ? T(L"A memória está quase cheia.", L"Memory is almost full.", L"La memoria está casi llena.")
                           : g_perf.cause == 3 ? T(L"O disco está ocupado o tempo todo.", L"The disk is busy all the time.", L"El disco está ocupado todo el tiempo.")
                                               : T(L"Um programa está usando quase toda a CPU.", L"An app is using almost all the CPU.", L"Un programa usa casi toda la CPU.");
        ClText(p, why, D2D1::RectF(x, y + 24 * s, xr, y + 42 * s), 12.5f, th.text2);
        y += 48 * s;
        clean::Disk d = clean::SystemDisk();
        struct M2 { const wchar_t* n; std::wstring v; double pct; bool hot; } ms[] = {
            {L"CPU", std::to_wstring((int)(g_perf.cpu + 0.5)) + L"%", g_perf.cpu, alert && g_perf.cause == 1},
            {T(L"Memória", L"Memory", L"Memoria"), std::to_wstring((int)(g_perf.mem + 0.5)) + L"%", g_perf.mem, alert && g_perf.cause == 2},
            {T(L"Disco (atividade)", L"Disk (activity)", L"Disco (actividad)"), std::to_wstring((int)(g_perf.disk + 0.5)) + L"%", g_perf.disk, alert && g_perf.cause == 3},
            {T(L"Espaço livre em C:", L"Free space on C:", L"Espacio libre en C:"), CleanSize(d.free), d.total ? 100.0 * d.free / d.total : 0, DiskLow(d)},
        };
        for (auto& m : ms) {
            ClText(p, m.n, D2D1::RectF(x, y, xr, y + 18 * s), 13, th.text, m.hot ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_MEDIUM);
            ClText(p, m.v, D2D1::RectF(x, y, xr, y + 18 * s), 13, m.hot ? th.text : th.text2, m.hot ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            bar(D2D1::RectF(x, y + 24 * s, xr, y + 30 * s), m.pct, m.hot ? hi : c.used);
            y += 40 * s;
        }
        p.TextSpaced(T(L"O QUE ESTÁ PESANDO", L"WHAT'S HEAVY", L"LO QUE PESA"), D2D1::RectF(x, y, x + 150 * s, y + 18 * s), 11, th.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD, 0.08f);
        y += 22 * s;
        for (size_t i = 0; i < g_perf.top.size(); ++i) {
            const ProcUse& pu = g_perf.top[i];
            p.Fill(D2D1::RectF(x, y, xr, y + 1 * s), 0, c.line);
            int cause = g_perf.cause ? g_perf.cause : 1;
            std::wstring v = cause == 2 ? CleanSize(pu.mem) : cause == 3 ? CleanSize(pu.io) + L"/s" : std::to_wstring((int)(pu.cpu + 0.5)) + L"% CPU";
            float vr = xr;
            static const wchar_t* never[] = {L"explorer.exe", L"dwm.exe", L"csrss.exe", L"winlogon.exe", L"svchost.exe", L"lsass.exe", L"services.exe",
                                             L"sihost.exe", L"ShellExperienceHost.exe", L"StartMenuExperienceHost.exe", L"SearchHost.exe", L"Taskmgr.exe", L"Vizu.exe", L"MsMpEng.exe"};
            bool closable = pu.window;
            for (auto n : never) if (!_wcsicmp(pu.exe.c_str(), n)) closable = false; // o Explorer tem janelas, mas fechar derrubaria a área de trabalho
            if (i == 0 && alert && closable) { // só programas com janela, nunca processos do Windows
                float bw = ClMeasure(p, T(L"Fechar", L"Close", L"Cerrar"), 12.5f) + 20 * s;
                wd.rClose = D2D1::RectF(xr - bw, y + 5 * s, xr, y + 27 * s);
                if (wd.hot == 2) p.Fill(wd.rClose, 7 * s, c.hover);
                p.Stroke(wd.rClose, 7 * s, c.line, 1 * s);
                ClText(p, T(L"Fechar", L"Close", L"Cerrar"), wd.rClose, 12.5f, th.text, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
                vr = wd.rClose.left - 10 * s;
            }
            float vw = ClMeasure(p, v, 13) + 4 * s;
            ClText(p, v, D2D1::RectF(vr - vw, y, vr, y + 32 * s), 13, th.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            ClText(p, CleanProgramName(pu.exe), D2D1::RectF(x, y, vr - vw - 8 * s, y + 32 * s), 13, th.text);
            y += 32 * s;
        }
        y += 14 * s;
        wd.rBtn = D2D1::RectF(x, y, xr, y + 36 * s);
        ClButton(p, c, wd.rBtn, T(L"Abrir a Limpeza", L"Open Cleanup", L"Abrir Limpieza"), true, wd.hot == 1, s);
    }
    // botões em coordenadas da janela (a margem da sombra conta)
    HBITMAP bmp = nullptr;
    if (FAILED(cv.Finish(&bmp)) || !bmp) return;
    // aberto: cresce para baixo; se não couber, para cima (o cartão fechado fica no lugar)
    LONG px = wd.ax - (LONG)M, py = wd.ay - (LONG)M;
    POINT apt = {wd.ax, wd.ay};
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(MonitorFromPoint(apt, MONITOR_DEFAULTTONEAREST), &mi);
    if (py + (LONG)Ht > mi.rcWork.bottom) py = max((LONG)mi.rcWork.top, mi.rcWork.bottom - (LONG)Ht);
    if (px + (LONG)Wt > mi.rcWork.right) px = max((LONG)mi.rcWork.left, mi.rcWork.right - (LONG)Wt);
    wd.w = Wt; wd.h = Ht;
    HDC screen = GetDC(nullptr), mem = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(mem, bmp);
    POINT pos = {px, py}, zero = {0, 0};
    SIZE size = {(LONG)Wt, (LONG)Ht};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(wd.wnd, screen, &pos, &size, mem, &zero, 0, &bf, ULW_ALPHA);
    SelectObject(mem, old); DeleteDC(mem); ReleaseDC(nullptr, screen); DeleteObject(bmp);
}

static void WidgetsRenderAll() { for (int k = 0; k < kWCount; ++k) WidgetRender(k); }

// ---------- janela ----------

static int WidgetKind(HWND h) { for (int k = 0; k < kWCount; ++k) if (g_wd[k].wnd == h) return k; return -1; }

static LRESULT CALLBACK WidgetProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    int k = WidgetKind(h);
    if (k < 0) return DefWindowProcW(h, msg, wp, lp);
    Widget& wd = g_wd[k];
    switch (msg) {
    case WM_TIMER:
        if (wp == 1) { // 1 s
            if (k == kWPerf) PerfSample();
            if (wd.open && wd.leaveAt && GetTickCount64() > wd.leaveAt) { wd.open = false; wd.leaveAt = 0; wd.hot = 0; }
            static int tick = 0;
            if (k == kWPerf || wd.open || ++tick % 30 == 0) WidgetRender(k); // disco: muda devagar
        }
        return 0;
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (wd.pressed && !wd.dragging && (abs(pt.x - wd.down.x) > 4 || abs(pt.y - wd.down.y) > 4)) {
            wd.dragging = true; wd.open = false; WidgetRender(k);
            ReleaseCapture(); SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, 0); // arrastar
            wd.pressed = wd.dragging = false;
            return 0;
        }
        int hot = ui::Hit(wd.rBtn, pt) ? 1 : ui::Hit(wd.rClose, pt) ? 2 : 0;
        bool changed = hot != wd.hot || !wd.open;
        wd.hot = hot; wd.leaveAt = 0;
        if (!wd.open) { wd.open = true; if (k == kWPerf) PerfProcesses(); SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); } // aberto fica por cima do outro widget
        if (changed) WidgetRender(k);
        TRACKMOUSEEVENT te = {sizeof(te), TME_LEAVE, h, 0}; TrackMouseEvent(&te);
        return 0;
    }
    case WM_MOUSELEAVE: wd.leaveAt = GetTickCount64() + 400; wd.hot = 0; return 0; // fecha com um respiro
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, wd.hot ? IDC_HAND : IDC_ARROW)); return TRUE;
    case WM_LBUTTONDOWN: wd.pressed = true; wd.down = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; SetCapture(h); return 0;
    case WM_LBUTTONUP: {
        bool was = wd.pressed; wd.pressed = false; ReleaseCapture();
        if (!was) return 0;
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (ui::Hit(wd.rClose, pt) && !g_perf.top.empty()) {
            std::wstring exe = g_perf.top[0].exe;
            std::thread([exe]() { clean::CloseProgram(exe); }).detach(); // pede para fechar; nunca força
        } else if (ui::Hit(wd.rBtn, pt) || (k == kWDisk && !wd.open)) OpenCleanWindow();
        return 0;
    }
    case WM_EXITSIZEMOVE: {
        RECT r; GetWindowRect(h, &r);
        float M = 14 * GetDpiForWindow(h) / 96.0f;
        wd.ax = r.left + (LONG)M; wd.ay = r.top + (LONG)M;
        WriteDword(k == kWDisk ? L"DiskWidgetX" : L"PerfWidgetX", (DWORD)wd.ax); WriteDword(k == kWDisk ? L"DiskWidgetY" : L"PerfWidgetY", (DWORD)wd.ay);
        return 0;
    }
    case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: case WM_DPICHANGED: WidgetRender(k); break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static bool WidgetOn(int k) { return !g_lite && ReadDword(k == kWDisk ? L"DiskWidget" : L"PerfWidget", 0) != 0; }

static void WidgetEnable(int k, bool on) {
    Widget& wd = g_wd[k];
    if (!on || g_lite) { if (wd.wnd) { KillTimer(wd.wnd, 1); DestroyWindow(wd.wnd); wd.wnd = nullptr; } return; }
    if (wd.wnd) { WidgetRender(k); return; }
    static bool reg = false;
    if (!reg) { WNDCLASSW wc = {}; wc.lpfnWndProc = WidgetProc; wc.hInstance = g_inst; wc.lpszClassName = L"VizuWidget"; RegisterClassW(&wc); reg = true; }
    wd = Widget();
    wd.wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE, L"VizuWidget", L"Vizu", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, g_inst, nullptr);
    DWORD x = ReadDword(k == kWDisk ? L"DiskWidgetX" : L"PerfWidgetX", 0x80000000), y = ReadDword(k == kWDisk ? L"DiskWidgetY" : L"PerfWidgetY", 0x80000000);
    POINT pt = {(LONG)x, (LONG)y};
    if (x == 0x80000000 || !MonitorFromPoint(pt, MONITOR_DEFAULTTONULL)) { // canto de cima, à direita; o de desempenho logo abaixo
        MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
        float s = GetDpiForSystem() / 96.0f;
        size_t n = ReadDword(L"DiskAll", 0) ? max((size_t)1, clean::AllDisks().size()) : 1; // o de desempenho fica logo abaixo do de disco
        float below = WidgetOn(kWDisk) ? 20 + 56 + (n - 1) * 30 + 16 : 20;
        pt.x = mi.rcWork.right - (LONG)(240 * s); pt.y = mi.rcWork.top + (LONG)((k == kWDisk ? 20 : below) * s);
    }
    wd.ax = pt.x; wd.ay = pt.y;
    if (k == kWPerf) PerfSample();
    WidgetRender(k);
    ShowWindow(wd.wnd, SW_SHOWNOACTIVATE);
    SetTimer(wd.wnd, 1, 1000, nullptr);
}

static void WidgetsStart() { for (int k = 0; k < kWCount; ++k) WidgetEnable(k, WidgetOn(k)); }
