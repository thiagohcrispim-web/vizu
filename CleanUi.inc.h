// CleanUi.inc.h - janela Limpeza (antigo Crispy Clean), protótipo docs/design/prototipo/Limpeza-B*.
// Faixa no topo (número, barra do disco, ação) + lista em três níveis (grupo › programa › pasta) ou mapa.
// Análise e limpeza rodam numa thread; a janela só desenha. Incluído no Vizu.cpp depois do Agenda.inc.h.

#include "Clean.h"
#include <map>

enum { WM_CLEAN_SCAN = WM_APP + 60, WM_CLEAN_SCANDONE, WM_CLEAN_TICK, WM_CLEAN_DONE };
static void WidgetsRenderAll();

// ---------- textos ----------

static std::wstring CleanSize(ULONGLONG b) {
    wchar_t t[64];
    if (b >= 1000ull << 20) swprintf(t, 64, L"%.1f GB", b / 1073741824.0);
    else if (b >= 1ull << 20) swprintf(t, 64, L"%.0f MB", b / 1048576.0);
    else if (b >= 1ull << 10) swprintf(t, 64, L"%.0f KB", b / 1024.0);
    else swprintf(t, 64, L"%llu B", b);
    std::wstring s = t;
    if (i18n::Current() != i18n::kEn) for (auto& c : s) if (c == L'.') c = L',';
    return s;
}
static std::wstring CleanCount(size_t n) { // 4.210 / 4,210
    std::wstring d = std::to_wstring(n), out;
    for (size_t i = 0; i < d.size(); ++i) { if (i && (d.size() - i) % 3 == 0) out += i18n::Current() == i18n::kEn ? L',' : L'.'; out += d[i]; }
    return out;
}
static const wchar_t* CleanGroupName(int g) {
    switch (g) {
    case clean::kGSystem: return T(L"Sistema", L"System", L"Sistema");
    case clean::kGBrowsers: return T(L"Navegadores", L"Browsers", L"Navegadores");
    case clean::kGAdobe: return L"Adobe";
    case clean::kGMessengers: return T(L"Mensageiros", L"Messengers", L"Mensajería");
    case clean::kGDev: return T(L"Desenvolvimento", L"Development", L"Desarrollo");
    case clean::kGGames: return T(L"Jogos", L"Games", L"Juegos");
    default: return T(L"Outros", L"Other", L"Otros");
    }
}
// Nomes da tabela (em português) nos outros idiomas.
static std::wstring CleanName(const std::wstring& pt) {
    static const wchar_t* tr[][3] = {
        {L"Temporários do Windows", L"Windows temporary files", L"Temporales de Windows"},
        {L"Miniaturas do Windows", L"Windows thumbnails", L"Miniaturas de Windows"},
        {L"Logs do Windows", L"Windows logs", L"Registros de Windows"},
        {L"Sobras de atualizações", L"Update leftovers", L"Restos de actualizaciones"},
        {L"Versões antigas de apps", L"Old app versions", L"Versiones antiguas de apps"},
        {L"Lixeira", L"Recycle Bin", L"Papelera"},
        {L"Shaders (DirectX)", L"Shaders (DirectX)", L"Sombreadores (DirectX)"},
        {L"Temporários do usuário (mais de 24 h)", L"User temporary files (over 24 h)", L"Temporales del usuario (más de 24 h)"},
        {L"Cache de miniaturas e ícones", L"Thumbnail and icon cache", L"Caché de miniaturas e iconos"},
        {L"Cache de shaders da placa de vídeo", L"Graphics card shader cache", L"Caché de sombreadores de la tarjeta gráfica"},
        {L"Despejos de falha e relatórios de erro", L"Crash dumps and error reports", L"Volcados de bloqueo e informes de errores"},
        {L"Instaladores baixados por atualizadores", L"Installers downloaded by updaters", L"Instaladores descargados por actualizadores"},
        {L"Cache", L"Cache", L"Caché"}, {L"Cache e logs", L"Cache and logs", L"Caché y registros"}, {L"Logs", L"Logs", L"Registros"},
    };
    int l = i18n::Current();
    if (l != i18n::kPt) for (auto& r : tr) if (pt == r[0]) return r[l];
    return pt;
}
// "chrome.exe" -> "Google Chrome" (para o diálogo de programa aberto)
static std::wstring CleanProgramName(const std::wstring& exe) {
    std::wstring found; int owners = 0;
    for (int i = 0; i < clean::kTargetCount; ++i)
        for (auto& p : clean::Split(clean::kTargets[i].procs))
            if (!_wcsicmp(p.c_str(), exe.c_str()) && found != clean::kTargets[i].item) { found = clean::kTargets[i].item; ++owners; }
    if (owners == 1) return CleanName(found);
    std::wstring n = exe; // vários donos (Photoshop.exe usa Camera Raw, painéis…): o nome do programa
    if (n.size() > 4 && !_wcsicmp(n.c_str() + n.size() - 4, L".exe")) n.resize(n.size() - 4);
    if (!_wcsicmp(n.c_str(), L"AfterFX")) return L"After Effects";
    if (!n.empty()) n[0] = (wchar_t)towupper(n[0]);
    return n;
}

// ---------- estado ----------

enum { kClScan, kClResult, kClCleaning, kClReport };
enum { kClNone, kClConfirm, kClProgram };
enum {
    kClClose = 1, kClMin, kClSettings, kClRescan, kClPrimary, kClSecondary, kClList, kClMap, kClDlgCancel, kClDlgOk,
    kClRow0 = 1000,  // + linha*4 + {0 seta, 1 caixa, 2 linha}
    kClCell0 = 20000 // + índice do bloco no mapa
};

struct CleanRow { int depth, g, i, l; };
struct CleanUi {
    HWND wnd = nullptr;
    ui::Surface surf;
    int phase = kClScan, overlay = kClNone;
    bool map = false;
    clean::Result res;                       // cópia da janela
    std::mutex mu;
    clean::Result shared;                    // escrito pela thread
    int scanGroup = 0;                       // grupo sendo analisado (para a linha "procurando")
    std::thread worker;
    std::atomic<bool> stop{false};
    std::set<int> openG, openI;              // grupos (g) e programas (g*1000+i) abertos
    float scroll = 0, contentH = 0, listTop = 0, listBottom = 0;
    std::vector<std::pair<int, D2D1_RECT_F>> hits;
    std::vector<CleanRow> rows;
    std::vector<std::pair<int, int>> cells;  // mapa: (g, i)
    int hot = 0;
    std::vector<std::wstring> running;       // .exe abertos (minúsculas)
    // limpeza
    std::vector<std::wstring> askQueue, toClose, skipped; // .exe
    std::atomic<ULONGLONG> doneBytes{0};
    ULONGLONG planBytes = 0;
    std::wstring status;                     // "Fechando o Chrome…" / grupo atual
    int cleaningG = -1;
    std::map<int, clean::Outcome> out;       // por folha: g*1000000 + i*1000 + l
    std::set<int> skippedLeaves, stillOpen;
    ULONGLONG removed = 0, kept = 0, recycledBytes = 0;
    clean::Disk disk;
    float shown = 0;                         // número grande, animado
    bool closeAfter = false;                 // fechou a janela durante a limpeza: fecha quando ela parar
    ULONGLONG shownTarget = 0;
};
static CleanUi g_cl;

static int ClKey(int g, int i, int l) { return g * 1000000 + i * 1000 + l; }
static bool ClIsRunning(const clean::Item& it) {
    for (auto& p : it.procs) for (auto& r : g_cl.running) if (clean::LowerPath(p) == r) return true;
    return false;
}
static int ClAgeDays() { DWORD d = ReadDword(L"CleanAgeDays", 1); return d == 7 || d == 30 ? (int)d : 1; }
static bool ClPermanent() { return g_cfg.tempPermanent; }
static bool ClIncludeRecycle() { return ReadDword(L"CleanRecycle", 1) != 0; }

// Grupo terminal: um programa só, com uma pasta só (ex.: Lixeira) — sem seta.
static bool ClGroupLeafOnly(const clean::GroupRes& g) { return g.items.size() == 1 && g.items[0].leaves.size() == 1 && CleanName(g.items[0].name) == CleanGroupName(g.id); }
static void ClCount(const clean::GroupRes& g, int i, int& on, int& all) { // i < 0: grupo inteiro; "só informa" não conta
    on = all = 0;
    for (int k = 0; k < (int)g.items.size(); ++k) if (i < 0 || i == k) for (auto& l : g.items[k].leaves) if (clean::Actionable(l)) { ++all; if (l.on) ++on; }
}
static void ClSet(clean::GroupRes& g, int i, bool v) { for (int k = 0; k < (int)g.items.size(); ++k) if (i < 0 || i == k) for (auto& l : g.items[k].leaves) l.on = v && clean::Actionable(l); }
// Selo do grupo: Revisar se algo dele vai para a Lixeira; Só informa se nada pode ser limpo.
static int ClGroupRisk(const clean::GroupRes& g) {
    int r = -1;
    for (auto& it : g.items) for (auto& l : it.leaves) { int k = clean::TargetOf(l).risk; if (k != clean::kInform) r = max(r, k); }
    return r < 0 ? clean::kInform : r;
}

static void ClRefreshRunning() { g_cl.running = clean::RunningExes(); }
static void ClRefreshDisk() { g_cl.disk = clean::SystemDisk(); }

// ---------- trabalho em segundo plano ----------

static void ClJoin() { g_cl.stop = true; if (g_cl.worker.joinable()) g_cl.worker.join(); g_cl.stop = false; }

static void ClStartScan() {
    ClJoin();
    g_cl.phase = kClScan; g_cl.overlay = kClNone; g_cl.scroll = 0; g_cl.map = false;
    g_cl.res = clean::Result(); g_cl.out.clear(); g_cl.skippedLeaves.clear(); g_cl.stillOpen.clear(); g_cl.skipped.clear(); g_cl.toClose.clear();
    g_cl.scanGroup = 0; g_cl.shownTarget = 0; g_cl.shown = 0;
    { std::lock_guard<std::mutex> lk(g_cl.mu); g_cl.shared = clean::Result(); }
    ClRefreshDisk();
    HWND w = g_cl.wnd; int age = ClAgeDays(); bool rb = ClIncludeRecycle();
    g_cl.worker = std::thread([w, age, rb]() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        clean::Result r;
        clean::Scan(r, age, rb, g_cl.stop, [&](int g) {
            std::lock_guard<std::mutex> lk(g_cl.mu);
            g_cl.shared.groups[g] = r.groups[g]; g_cl.shared.total += r.groups[g].bytes;
            PostMessageW(w, WM_CLEAN_SCAN, g + 1, 0);
        });
        PostMessageW(w, WM_CLEAN_SCANDONE, 0, 0);
        CoUninitialize();
    });
}

static void ClPullScan() {
    std::lock_guard<std::mutex> lk(g_cl.mu);
    for (int g = 0; g < clean::kGroupCount; ++g) if (g_cl.shared.groups[g].done && !g_cl.res.groups[g].done) g_cl.res.groups[g] = g_cl.shared.groups[g];
    g_cl.res.total = g_cl.shared.total;
}

static void ClStartClean() {
    ClJoin();
    g_cl.phase = kClCleaning; g_cl.overlay = kClNone; g_cl.scroll = 0; g_cl.map = false;
    g_cl.out.clear(); g_cl.skippedLeaves.clear(); g_cl.stillOpen.clear();
    g_cl.doneBytes = 0; g_cl.planBytes = 0; g_cl.removed = g_cl.kept = g_cl.recycledBytes = 0;
    // folhas de programas que o usuário mandou pular
    for (int g = 0; g < clean::kGroupCount; ++g) for (int i = 0; i < (int)g_cl.res.groups[g].items.size(); ++i) {
        auto& it = g_cl.res.groups[g].items[i];
        bool skip = false;
        for (auto& p : it.procs) for (auto& s : g_cl.skipped) if (!_wcsicmp(p.c_str(), s.c_str())) skip = true;
        for (int l = 0; l < (int)it.leaves.size(); ++l) if (it.leaves[l].on) { if (skip) g_cl.skippedLeaves.insert(ClKey(g, i, l)); else g_cl.planBytes += it.leaves[l].bytes; }
    }
    clean::Result plan = g_cl.res;
    std::set<int> skippedLeaves = g_cl.skippedLeaves;
    std::vector<std::wstring> toClose = g_cl.toClose;
    HWND w = g_cl.wnd; int age = ClAgeDays(); bool perm = ClPermanent();
    g_cl.worker = std::thread([w, age, perm, plan, skippedLeaves, toClose]() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); // a Lixeira (IFileOperation) precisa de COM
        std::set<int> still;
        for (auto& exe : toClose) {
            { std::lock_guard<std::mutex> lk(g_cl.mu); g_cl.status = std::wstring(T(L"Fechando ", L"Closing ", L"Cerrando ")) + CleanProgramName(exe) + L"…"; }
            PostMessageW(w, WM_CLEAN_TICK, 0, 0);
            if (!clean::CloseProgram(exe)) // continuou aberto (pode estar perguntando se quer salvar): pula
                for (int g = 0; g < clean::kGroupCount; ++g) for (int i = 0; i < (int)plan.groups[g].items.size(); ++i)
                    for (auto& p : plan.groups[g].items[i].procs) if (!_wcsicmp(p.c_str(), exe.c_str())) for (int l = 0; l < (int)plan.groups[g].items[i].leaves.size(); ++l) still.insert(ClKey(g, i, l));
        }
        { std::lock_guard<std::mutex> lk(g_cl.mu); g_cl.stillOpen = still; }
        for (int g = 0; g < clean::kGroupCount && !g_cl.stop; ++g) {
            const auto& gr = plan.groups[g];
            for (int i = 0; i < (int)gr.items.size() && !g_cl.stop; ++i) for (int l = 0; l < (int)gr.items[i].leaves.size() && !g_cl.stop; ++l) {
                const auto& lf = gr.items[i].leaves[l];
                int key = ClKey(g, i, l);
                if (!lf.on) continue;
                clean::Outcome o;
                if (skippedLeaves.count(key) || still.count(key)) o.kept = lf.bytes;
                else {
                    { std::lock_guard<std::mutex> lk(g_cl.mu); g_cl.status = CleanName(gr.items[i].name); g_cl.cleaningG = g; }
                    PostMessageW(w, WM_CLEAN_TICK, 0, 0);
                    ULONGLONG lastPost = 0;
                    o = clean::CleanLeaf(lf, age, perm, g_cl.stop, [&](ULONGLONG b) {
                        g_cl.doneBytes += b;
                        ULONGLONG now = GetTickCount64();
                        if (now - lastPost > 60) { lastPost = now; PostMessageW(w, WM_CLEAN_TICK, 0, 0); }
                    });
                }
                std::lock_guard<std::mutex> lk(g_cl.mu);
                g_cl.out[key] = o;
            }
        }
        PostMessageW(w, WM_CLEAN_DONE, 0, 0);
        CoUninitialize();
    });
}

// Depois de confirmar: pergunta, um por um, sobre os programas abertos que usam pastas marcadas.
static void ClAfterConfirm() {
    ClRefreshRunning();
    g_cl.askQueue.clear(); g_cl.toClose.clear(); g_cl.skipped.clear();
    for (auto& g : g_cl.res.groups) for (auto& it : g.items) {
        bool any = false; for (auto& l : it.leaves) if (l.on) any = true;
        if (!any) continue;
        for (auto& p : it.procs) {
            std::wstring lp = clean::LowerPath(p);
            bool run = false; for (auto& r : g_cl.running) if (r == lp) run = true;
            bool dup = false; for (auto& q : g_cl.askQueue) if (q == lp) dup = true;
            if (run && !dup) g_cl.askQueue.push_back(lp);
        }
    }
    if (g_cl.askQueue.empty()) ClStartClean(); else g_cl.overlay = kClProgram;
}

// ---------- desenho ----------

static void ClText(ui::Painter& p, const std::wstring& t, D2D1_RECT_F r, float size, D2D1_COLOR_F c, DWRITE_FONT_WEIGHT w = DWRITE_FONT_WEIGHT_NORMAL,
                   DWRITE_TEXT_ALIGNMENT a = DWRITE_TEXT_ALIGNMENT_LEADING, const wchar_t* family = nullptr) {
    IDWriteTextFormat* f = p.Format(size, w, a, family);
    ID2D1SolidColorBrush* b = nullptr;
    if (f && SUCCEEDED(p.rt->CreateSolidColorBrush(c, &b))) { p.rt->DrawText(t.c_str(), (UINT32)t.size(), f, r, b); b->Release(); }
    if (f) f->Release();
}
static float ClMeasure(ui::Painter& p, const std::wstring& t, float size, DWRITE_FONT_WEIGHT w = DWRITE_FONT_WEIGHT_NORMAL, const wchar_t* family = nullptr) {
    IDWriteTextFormat* f = p.Format(size, w, DWRITE_TEXT_ALIGNMENT_LEADING, family);
    IDWriteTextLayout* lay = nullptr; float width = 0;
    if (f && SUCCEEDED(g_dw->CreateTextLayout(t.c_str(), (UINT32)t.size(), f, 10000, 100, &lay))) { DWRITE_TEXT_METRICS m; lay->GetMetrics(&m); width = m.widthIncludingTrailingWhitespace; lay->Release(); }
    if (f) f->Release();
    return width;
}

static const char* kIcoChevR = "M9 6l6 6-6 6";
static const char* kIcoChevD = "M6 9l6 6 6-6";
static const char* kIcoCheck = "M20 6L9 17l-5-5";
static const char* kIcoAlert = "M12 2a10 10 0 1 0 0 20 10 10 0 1 0 0-20zM12 8v4M12 16h.01";
static const char* kIcoSkip = "M12 2a10 10 0 1 0 0 20 10 10 0 1 0 0-20zM8 12h8";
static const char* kIcoTrash = "M3 6h18M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6M10 11v6M14 11v6";
static const char* kIcoRefresh = "M23 4v6h-6M1 20v-6h6M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15";
static const char* kIcoGear = "M12 9a3 3 0 1 0 0 6 3 3 0 1 0 0-6zM19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z";
static const char* kIcoMin = "M5 12h14";
static const char* kIcoInfo = "M12 2a10 10 0 1 0 0 20 10 10 0 1 0 0-20zM12 16v-4M12 8h.01";
static const char* kIcoX = "M6 6l12 12M18 6L6 18";

static bool g_clClosing, g_clDown;            // fechando (animação) / botão do mouse apertado
static POINT g_clMouse = {-100000, -100000};  // luz do cursor no cartão
// Cores derivadas do tema (protótipo: Classic, Claro, Escuro, Crispy, Windows).
struct ClColors { D2D1_COLOR_F panel, line, used, track, accent, onAccent, warn, box, hover; };
static ClColors ClPalette(const ui::Theme& th) {
    ClColors c;
    c.accent = th.accent; c.onAccent = th.onAccent;
    c.line = th.classic ? ui::Rgba(0x4A4F57) : th.neon ? ui::Rgba(0x232326) : th.dark ? ui::Rgba(0xFFFFFF, 0.10f) : ui::Rgba(0xD5D9DF);
    c.panel = th.classic ? ui::Rgba(0x3A3F47) : th.neon ? ui::Rgba(0xFFFFFF, 0.045f) : th.dark ? ui::Rgba(0xFFFFFF, 0.05f) : ui::Rgba(0xF5F5F7);
    c.used = th.classic ? ui::Rgba(0x8A919C) : th.dark ? ui::Rgba(0x5A5A62) : ui::Rgba(0xAEB4BD);
    c.track = th.classic ? ui::Rgba(0x2A2F36) : th.dark ? ui::Rgba(0xFFFFFF, 0.08f) : ui::Rgba(0xECEDF0);
    c.warn = th.dark ? ui::Rgba(0xFFB199) : ui::Rgba(0xB23A0E);
    c.box = th.classic ? ui::Rgba(0xB5BAC2) : th.dark ? ui::Rgba(0xA1A1A8) : ui::Rgba(0x8A909A);
    c.hover = th.dark ? ui::Rgba(0xFFFFFF, 0.06f) : ui::Rgba(0x000000, 0.04f);
    return c;
}

static void ClCheckbox(ui::Painter& p, const ClColors& c, D2D1_RECT_F r, int state, float s) { // 0 vazio, 1 marcado, 2 parcial
    if (state) {
        if (p.th.neon) p.Glow(r, 5 * s);
        p.Fill(r, p.tk.r_control, c.accent);
        if (state == 1) p.Feather(kIcoCheck, r, 13 * s, c.onAccent, 3);
        else { float cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2; p.Fill(D2D1::RectF(cx - 4.5f * s, cy - 1.25f * s, cx + 4.5f * s, cy + 1.25f * s), 1 * s, c.onAccent); }
    } else p.Stroke(r, p.tk.r_control, c.box, 1.5f * s);
}

static void ClSpinner(ui::Painter& p, const ClColors& c, D2D1_RECT_F r, float s) {
    float cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, rad = (r.right - r.left) / 2 - 1.5f * s;
    ID2D1SolidColorBrush* b = nullptr;
    if (SUCCEEDED(p.rt->CreateSolidColorBrush(c.track, &b))) { p.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad), b, 2.5f * s); b->Release(); }
    float a0 = (GetTickCount64() % 1000) / 1000.0f * 6.2832f;
    ID2D1Factory* f = nullptr; p.rt->GetFactory(&f);
    ID2D1PathGeometry* g = nullptr; ID2D1GeometrySink* k = nullptr;
    if (SUCCEEDED(f->CreatePathGeometry(&g)) && SUCCEEDED(g->Open(&k))) {
        k->BeginFigure(D2D1::Point2F(cx + rad * cosf(a0), cy + rad * sinf(a0)), D2D1_FIGURE_BEGIN_HOLLOW);
        k->AddArc(D2D1::ArcSegment(D2D1::Point2F(cx + rad * cosf(a0 + 1.6f), cy + rad * sinf(a0 + 1.6f)), D2D1::SizeF(rad, rad), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
        k->EndFigure(D2D1_FIGURE_END_OPEN); k->Close(); k->Release();
        ID2D1StrokeStyle* st = nullptr;
        f->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND), nullptr, 0, &st);
        if (SUCCEEDED(p.rt->CreateSolidColorBrush(c.accent, &b))) { p.rt->DrawGeometry(g, b, 2.5f * s, st); b->Release(); }
        if (st) st->Release();
    }
    if (g) g->Release();
    f->Release();
}

// Barra do disco: usado (sem o que vai sair) · a limpar · liberado agora · livre.
static void ClDiskBar(ui::Painter& p, const ClColors& c, D2D1_RECT_F r, ULONGLONG total, ULONGLONG free, ULONGLONG toClean, ULONGLONG freed) {
    float h = r.bottom - r.top, w = r.right - r.left;
    p.Fill(r, p.tk.r_chip, c.track);
    if (!total) return;
    ULONGLONG used = total > free ? total - free : 0;
    ULONGLONG rest = used > toClean ? used - toClean : 0;
    float x = r.left;
    auto seg = [&](ULONGLONG v, D2D1_COLOR_F col, float minW) {
        float sw = (float)((double)v / total * w);
        if (v && sw < minW) sw = minW;
        if (sw <= 0) return;
        if (x + sw > r.right) sw = r.right - x;
        p.rt->PushAxisAlignedClip(D2D1::RectF(x, r.top, x + sw, r.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
        p.Fill(r, p.tk.r_chip, col);
        p.rt->PopAxisAlignedClip();
        x += sw;
    };
    seg(rest, c.used, 0);
    seg(toClean, c.accent, 4 * s);
    D2D1_COLOR_F tint = c.accent; tint.a = 0.30f;
    seg(freed, tint, freed ? 4.0f * s : 0);
}

static void ClLegend(ui::Painter& p, const ClColors& c, float x, float y, float maxX, float s, const std::vector<std::pair<D2D1_COLOR_F, std::wstring>>& items) {
    for (auto& it : items) {
        float tw = ClMeasure(p, it.second, 12) + 16 * s;
        if (x + tw > maxX) break;
        D2D1_RECT_F d = D2D1::RectF(x, y + 5 * s, x + 8 * s, y + 13 * s);
        p.Fill(d, p.tk.r_chip, it.first);
        if (it.first.a < 0.5f || (it.first.r == c.track.r && it.first.g == c.track.g)) p.Stroke(d, p.tk.r_chip, c.box, 1 * s);
        ClText(p, it.second, D2D1::RectF(x + 13 * s, y, x + tw + 20 * s, y + 18 * s), 12, p.th.text2);
        x += tw + 12 * s;
    }
}

static void ClButton(ui::Painter& p, const ClColors& c, D2D1_RECT_F r0, const std::wstring& label, bool primary, bool hot, float s, bool enabled = true, const char* icon = nullptr) {
    int key = 0x5000 + (int)(r0.left / 4) * 977 + (int)(r0.top / 4); // pela posição: o fundo acende (~200 ms) e afunda ao clicar
    D2D1_RECT_F r = p.Press(r0, key, hot && enabled);
    float hk = p.A(key * 4 + 1, hot && enabled ? 1.0f : 0.0f, 200);
    float rad = p.tk.r_control;
    if (p.th.neon) { // Crispy: pílula; secundário acende em laranja com neon ao passar o mouse
        rad = (r.bottom - r.top) / 2;
        if (primary) { if (enabled) p.Neon(r, rad); D2D1_COLOR_F f = c.accent; if (!enabled) f.a = 0.35f; p.Fill(r, rad, f); if (hot && enabled) p.Fill(r, rad, ui::Rgba(0xFFFFFF, 0.14f)); }
        else { p.Fill(r, rad, ui::Rgba(0xFFFFFF, 0.04f)); p.Neon(r, rad, hk); p.Fill(r, rad, ui::Rgba(ui::NeonColor(), 0.16f * hk)); p.Stroke(r, rad, ui::Mix(ui::Rgba(0xFFFFFF, 0.12f), c.accent, hk), 1 * s); }
    } else if (primary) {
        D2D1_COLOR_F f = c.accent; if (!enabled) f.a = 0.35f;
        if (p.th.neon && enabled) p.Glow(r, rad);
        p.Fill(r, rad, f);
        if (hot && enabled) p.Fill(r, rad, ui::Rgba(0xFFFFFF, 0.14f));
    } else {
        D2D1_COLOR_F hv = c.hover; hv.a *= hk; p.Fill(r, rad, hv);
        p.Stroke(r, rad, c.line, 1 * s);
    }
    D2D1_COLOR_F tc = primary ? c.onAccent : p.th.neon ? ui::Mix(p.th.text, c.accent, hk) : p.th.text;
    float tw = ClMeasure(p, label, 14, DWRITE_FONT_WEIGHT_SEMI_BOLD), iw = icon ? 22 * s : 0;
    float x0 = (r.left + r.right) / 2 - (tw + iw) / 2;
    if (icon) p.Feather(icon, D2D1::RectF(x0, r.top, x0 + 16 * s, r.bottom), 15 * s, tc);
    ClText(p, label, D2D1::RectF(x0 + iw, r.top, x0 + iw + tw + 4 * s, r.bottom), 14, tc, DWRITE_FONT_WEIGHT_SEMI_BOLD);
}

// Treemap simples: divide a área ao meio pela soma, alternando o sentido.
static void ClTreemap(std::vector<std::pair<ULONGLONG, int>>& items, size_t a, size_t b, D2D1_RECT_F r, std::vector<std::pair<int, D2D1_RECT_F>>& out) {
    if (a >= b) return;
    if (b - a == 1) { out.push_back({items[a].second, r}); return; }
    ULONGLONG total = 0; for (size_t i = a; i < b; ++i) total += items[i].first;
    ULONGLONG acc = 0; size_t mid = a;
    while (mid < b - 1 && acc + items[mid].first <= total / 2) acc += items[mid++].first;
    if (mid == a) acc += items[mid++].first;
    float f = total ? (float)((double)acc / total) : 0.5f;
    bool wide = (r.right - r.left) >= (r.bottom - r.top);
    D2D1_RECT_F r1 = r, r2 = r;
    if (wide) { r1.right = r2.left = r.left + (r.right - r.left) * f; } else { r1.bottom = r2.top = r.top + (r.bottom - r.top) * f; }
    ClTreemap(items, a, mid, r1, out); ClTreemap(items, mid, b, r2, out);
}

static void PaintClean(HWND h) {
    g_cl.surf.hwnd = h;
    if (!g_cl.surf.Begin(g_d2d)) return;
    float s = WndScale(h);
    ID2D1RenderTarget* rt = g_cl.surf.rt;
    D2D1_SIZE_F sz = rt->GetSize();
    const ui::Theme& th = g_theme;
    ui::Tokens tk = ui::MakeTokens(th);
    ui::g_tokens = tk;
    ClColors c = ClPalette(th);
    rt->Clear(th.tint);
    ui::Backdrop(rt, th, sz.width, sz.height);
    ui::Painter p(rt, g_dw, th, tk, s);
    static ui::Anim an; static HWND last; an.Frame(); p.an = &an;
    if (last != h) { last = h; an.m.clear(); an.Start(0x3000, 0, 1, 220); } // janela nova: fade + escala 0,98 -> 1
    p.mx = (float)g_clMouse.x; p.my = (float)g_clMouse.y; p.down = g_clDown;
    float openK = an.Get(0x3000, g_clClosing ? 0.0f : 1.0f, g_clClosing ? 150.0f : 220.0f, g_clClosing); // fechar: o inverso, mais rápido
    if (g_clClosing && openK <= 0.001f) { g_cl.surf.End(); PostMessage(h, WM_CLOSE, 1, 0); return; }
    float openSc = an.reduced ? 1.0f : 0.98f + 0.02f * openK;
    rt->SetTransform(D2D1::Matrix3x2F::Scale(openSc, openSc, D2D1::Point2F(sz.width / 2, sz.height / 2)));
    bool openLayer = openK < 0.999f;
    if (openLayer) rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), openK), nullptr);
    g_cl.hits.clear();
    auto hit = [&](int id, D2D1_RECT_F r) { g_cl.hits.push_back({id, r}); return g_cl.hot == id && !(g_cl.overlay && id < kClDlgCancel); };
    const wchar_t* HF = L"Space Grotesk";
    bool narrow = sz.width < 700 * s;
    float pad = (narrow ? p.tk.sp_4 : p.tk.sp_6) * s;

    // barra de título
    ClText(p, T(L"Limpeza", L"Cleanup", L"Limpieza"), D2D1::RectF(pad, 0, sz.width / 2, p.tk.sp_6 * s * 3.25f), p.tk.t_title.size * s, th.text, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, HF);
    {
        float bx = sz.width - p.tk.sp_3 * s - p.tk.sp_6 * s * 0.9f;
        struct B { int id; const char* ico; bool show; } bs[] = {{kClClose, kIcoX, true}, {kClMin, kIcoMin, true}, {kClSettings, kIcoGear, true},
                                                                 {kClRescan, kIcoRefresh, g_cl.phase == kClResult || g_cl.phase == kClReport}};
        for (auto& b : bs) {
            if (!b.show) continue;
            D2D1_RECT_F r = D2D1::RectF(bx, p.tk.sp_2 * s, bx + p.tk.sp_6 * s * 0.9f, p.tk.sp_6 * s * 1.1f);
            if (hit(b.id, r)) p.Fill(r, p.tk.r_control, c.hover);
            p.Feather(b.ico, r, p.tk.r_control * s, th.text2);
            bx -= p.tk.sp_6 * s * 1.0f;
        }
    }

    // ---- faixa ----
    ULONGLONG sel = clean::Selected(g_cl.res);
    std::wstring kicker, diskTitle, sub, primary, secondary;
    ULONGLONG barClean = 0, barFreed = 0;
    std::vector<std::pair<D2D1_COLOR_F, std::wstring>> legend;
    D2D1_COLOR_F tint = c.accent; tint.a = 0.30f;
    const clean::Disk& dk = g_cl.disk;
    ULONGLONG used = dk.total > dk.free ? dk.total - dk.free : 0;
    wchar_t buf[256];
    if (g_cl.phase == kClScan) {
        int done = 0; for (auto& g : g_cl.res.groups) if (g.done) ++done;
        swprintf(buf, 256, T(L"Analisando · %d de %d grupos", L"Scanning · %d of %d groups", L"Analizando · %d de %d grupos"), done, clean::kGroupCount);
        kicker = buf; g_cl.shownTarget = g_cl.res.total; barClean = g_cl.res.total;
        legend = {{c.used, std::wstring(T(L"Usado ", L"Used ", L"Usado ")) + CleanSize(used)}, {c.accent, T(L"Encontrado até agora", L"Found so far", L"Encontrado hasta ahora")}};
        secondary = T(L"Parar", L"Stop", L"Detener"); sub = T(L"Nada é apagado ainda", L"Nothing is deleted yet", L"Aún no se borra nada");
    } else if (g_cl.phase == kClResult) {
        kicker = T(L"Pronto para liberar", L"Ready to free up", L"Listo para liberar"); g_cl.shownTarget = sel; barClean = sel;
        legend = {{c.used, std::wstring(T(L"Usado ", L"Used ", L"Usado ")) + CleanSize(used > sel ? used - sel : 0)}, {c.accent, std::wstring(T(L"A limpar ", L"To clean ", L"A limpiar ")) + CleanSize(sel)},
                  {c.track, std::wstring(T(L"Livre ", L"Free ", L"Libre ")) + CleanSize(dk.free)}};
        primary = std::wstring(T(L"Limpar ", L"Clean ", L"Limpiar ")) + CleanSize(sel);
        bool anyReview = false;
        for (auto& g : g_cl.res.groups) for (auto& it : g.items) for (auto& l : it.leaves) if (l.on && clean::TargetOf(l).risk == clean::kReview) anyReview = true;
        sub = ClPermanent() || !anyReview ? T(L"Exclui de vez", L"Deletes permanently", L"Elimina definitivamente") : T(L"Revisar vai para a Lixeira", L"Review items go to the Recycle Bin", L"Revisar va a la Papelera");
    } else if (g_cl.phase == kClCleaning) {
        ULONGLONG d = g_cl.doneBytes;
        int pct = (int)(an.Get(4, g_cl.planBytes ? (float)min(100ull, d * 100 / g_cl.planBytes) : 0.0f, an.reduced ? 0.0f : 300.0f) + 0.5f); // percentual em contagem rápida
        swprintf(buf, 256, T(L"Limpando · %d%%", L"Cleaning · %d%%", L"Limpiando · %d%%"), pct);
        kicker = buf; g_cl.shownTarget = d; barClean = g_cl.planBytes > d ? g_cl.planBytes - d : 0; barFreed = d;
        legend = {{c.accent, std::wstring(T(L"Falta ", L"Left ", L"Falta ")) + CleanSize(barClean)}, {tint, std::wstring(T(L"Saiu ", L"Removed ", L"Salió ")) + CleanSize(d)}};
        secondary = T(L"Parar", L"Stop", L"Detener"); sub = T(L"O que já saiu continua limpo", L"What's done stays clean", L"Lo que ya salió queda limpio");
    } else {
        bool inBin = g_cl.recycledBytes > 0;
        kicker = g_cl.removed ? (inBin && g_cl.recycledBytes == g_cl.removed ? T(L"Pronto · na Lixeira", L"Done · in the Recycle Bin", L"Listo · en la Papelera") : T(L"Pronto · liberado", L"Done · freed", L"Listo · liberado"))
                              : T(L"Nada saiu", L"Nothing was removed", L"No salió nada");
        g_cl.shownTarget = g_cl.removed;
        ULONGLONG freedNow = g_cl.removed - g_cl.recycledBytes;
        barFreed = freedNow;
        legend = {{c.used, std::wstring(T(L"Usado ", L"Used ", L"Usado ")) + CleanSize(used)}, {tint, std::wstring(T(L"Liberado agora ", L"Freed now ", L"Liberado ahora ")) + CleanSize(freedNow)},
                  {c.track, std::wstring(T(L"Livre ", L"Free ", L"Libre ")) + CleanSize(dk.free)}};
        primary = T(L"Concluir", L"Done", L"Listo");
        if (inBin) { secondary = T(L"Esvaziar a Lixeira", L"Empty Recycle Bin", L"Vaciar la Papelera"); swprintf(buf, 256, T(L"%ls na Lixeira: esvazie para liberar", L"%ls in the Recycle Bin: empty it to free space", L"%ls en la Papelera: vacíala para liberar"), CleanSize(g_cl.recycledBytes).c_str()); sub = buf; }
        else if (g_cl.kept) { swprintf(buf, 256, T(L"%ls em uso ficaram", L"%ls in use stayed", L"%ls en uso se quedaron"), CleanSize(g_cl.kept).c_str()); sub = buf; }
    }
    swprintf(buf, 256, T(L"Disco %lc: · %ls livres de %ls", L"Disk %lc: · %ls free of %ls", L"Disco %lc: · %ls libres de %ls"), dk.letter, CleanSize(dk.free).c_str(), CleanSize(dk.total).c_str());
    diskTitle = buf;

    float bandTop = 56 * s, bandH = narrow ? 172 * s : 108 * s;
    D2D1_RECT_F band = D2D1::RectF(pad, bandTop, sz.width - pad, bandTop + bandH);
    if (th.neon) p.Card(band, p.tk.r_card, 0x2001); else p.Fill(band, p.tk.r_card, c.panel);
    {
        float numW = 190 * s;
        float bx = band.left + p.tk.sp_6 * s, by = band.top + (narrow ? p.tk.sp_4 * s * 1.125f : p.tk.sp_6 * s * 1.25f);
        ClText(p, kicker, D2D1::RectF(bx, by, band.right - p.tk.sp_6 * s, by + p.tk.sp_4 * s * 1.125f), p.tk.t_caption.size * s, th.text2);
        ClText(p, CleanSize((ULONGLONG)g_cl.shown), D2D1::RectF(bx, by + p.tk.sp_6 * s * 1.375f, bx + 260 * s, by + p.tk.t_title.size * s * 3.5f), p.tk.t_title.size * s, th.text, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, HF);
        // ação
        float aw = 0, ah = p.tk.sp_6 * s * 1.1f;
        float pw = primary.empty() ? 0 : ClMeasure(p, primary, p.tk.t_body.size * s, DWRITE_FONT_WEIGHT_SEMI_BOLD) + p.tk.sp_6 * s * 1.1f;
        float sw2 = secondary.empty() ? 0 : ClMeasure(p, secondary, p.tk.t_body.size * s, DWRITE_FONT_WEIGHT_SEMI_BOLD) + p.tk.sp_6 * s * 0.9f;
        aw = pw + sw2 + (pw && sw2 ? p.tk.sp_2 * s : 0);
        float subW = ClMeasure(p, sub, p.tk.t_caption.size);
        float colW = max(aw, subW);
        float ax = band.right - p.tk.sp_6 * s - colW + (colW - aw) / 2, ay = narrow ? by + p.tk.sp_3 * s * 0.875f : band.top + p.tk.sp_4 * s * 1.125f;
        if (!secondary.empty()) { D2D1_RECT_F r = D2D1::RectF(ax, ay, ax + sw2, ay + ah); ClButton(p, c, r, secondary, false, hit(kClSecondary, r), s); ax += sw2 + p.tk.sp_2 * s; }
        if (!primary.empty()) { bool en = g_cl.phase != kClResult || sel > 0; D2D1_RECT_F r = D2D1::RectF(ax, ay, ax + pw, ay + ah); bool hv = hit(en ? kClPrimary : 0, r); ClButton(p, c, r, primary, true, hv, s, en); }
        ClText(p, sub, D2D1::RectF(band.right - p.tk.sp_6 * s - colW, ay + ah + p.tk.sp_1 * s, band.right - p.tk.sp_6 * s, ay + ah + p.tk.sp_6 * s * 1.375f), p.tk.t_caption.size * s, th.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        // disco
        float dx0 = narrow ? bx : bx + numW, dx1 = narrow ? band.right - p.tk.sp_6 * s : band.right - p.tk.sp_6 * s - colW - 28 * s;
        float dy = narrow ? by + p.tk.sp_6 * s * 5.75f : band.top + p.tk.sp_6 * s * 1.375f;
        ClText(p, diskTitle, D2D1::RectF(dx0, dy, dx1, dy + p.tk.sp_4 * s * 1.125f), p.tk.t_caption.size * s, th.text, DWRITE_FONT_WEIGHT_MEDIUM);
        { // a largura das barras anima (~300 ms) quando o valor muda
        float kc = an.Get(1, (float)barClean, 300, true, true), kf = an.Get(2, (float)barFreed, 300, true, true);
        ClDiskBar(p, c, D2D1::RectF(dx0, dy + p.tk.sp_6 * s * 1.625f, dx1, dy + p.tk.sp_6 * s * 2.25f), dk.total, dk.free, (ULONGLONG)kc, (ULONGLONG)kf);
    }
        ClLegend(p, c, dx0, dy + p.tk.sp_6 * s * 2.75f, dx1, s, legend);
    }

    // ---- cabeçalho da lista ----
    float ly = band.bottom + p.tk.sp_4 * s * 1.125f;
    std::wstring head;
    int groupsWith = 0; for (auto& g : g_cl.res.groups) if (!g.items.empty()) ++groupsWith;
    if (g_cl.phase == kClScan) {
        int cur = -1; for (int g = 0; g < clean::kGroupCount; ++g) if (!g_cl.res.groups[g].done) { cur = g; break; }
        head = std::wstring(T(L"PROCURANDO · ", L"SEARCHING · ", L"BUSCANDO · ")) + (cur >= 0 ? CleanGroupName(cur) : L"") + L"…";
    } else if (g_cl.phase == kClResult) {
        swprintf(buf, 256, narrow ? T(L"ENCONTRADO · %ls", L"FOUND · %ls", L"ENCONTRADO · %ls") : T(L"ENCONTRADO · %ls EM %d GRUPOS", L"FOUND · %ls IN %d GROUPS", L"ENCONTRADO · %ls EN %d GRUPOS"), CleanSize(g_cl.res.total).c_str(), groupsWith);
        head = buf;
    } else if (g_cl.phase == kClCleaning) {
        std::wstring st; { std::lock_guard<std::mutex> lk(g_cl.mu); st = g_cl.status; } // escrito pela thread da limpeza
        head = st.empty() ? T(L"LIMPANDO", L"CLEANING", L"LIMPIANDO") : std::wstring(T(L"LIMPANDO · ", L"CLEANING · ", L"LIMPIANDO · ")) + st;
    }
    else { swprintf(buf, 256, T(L"RELATÓRIO · %ls ARQUIVOS SAÍRAM", L"REPORT · %ls FILES REMOVED", L"INFORME · %ls ARCHIVOS ELIMINADOS"), CleanCount([] { size_t n = 0; for (auto& o : g_cl.out) n += o.second.filesRemoved; return n; }()).c_str()); head = buf; }
    for (auto& ch : head) ch = (wchar_t)towupper(ch);
    { // letras espaçadas, alinhado à esquerda (TextSpaced centraliza no retângulo: ele tem a largura exata do texto)
        float hw = ClMeasure(p, head, p.tk.t_section.size * s, DWRITE_FONT_WEIGHT_SEMI_BOLD) + head.size() * p.tk.t_section.size * s * 0.1f + p.tk.sp_1 * s, maxW = sz.width - 2 * pad - (g_cl.phase == kClResult ? p.tk.sp_6 * s * 6.66f : 0);
        if (hw > maxW) hw = maxW;
        p.TextSpaced(head, D2D1::RectF(pad, ly, pad + hw, ly + p.tk.t_section.size * s * 2.9f), p.tk.t_section.size * s, th.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD, 0.1f);
    }
    if (g_cl.phase == kClResult && groupsWith) {
        D2D1_RECT_F sr = D2D1::RectF(sz.width - pad - p.tk.sp_6 * s * 6.16f, ly, sz.width - pad, ly + p.tk.t_section.size * s * 2.9f), parts[2];
        std::wstring l[2] = {T(L"Lista", L"List", L"Lista"), T(L"Mapa", L"Map", L"Mapa")};
        int hv = g_cl.hot == kClList ? 0 : g_cl.hot == kClMap ? 1 : -1;
        p.Segmented(sr, l, 2, g_cl.map ? 1 : 0, hv, parts);
        hit(kClList, parts[0]); hit(kClMap, parts[1]);
    }
    bool footer = g_cl.phase == kClResult && !g_cl.map && g_cl.res.total;
    float top = ly + p.tk.sp_6 * s * 1.66f, bottom = sz.height - (footer ? p.tk.sp_6 * s * 1.35f : p.tk.sp_2 * s);
    g_cl.listTop = top; g_cl.listBottom = bottom;
    D2D1_RECT_F view = D2D1::RectF(pad, top, sz.width - pad, bottom);

    // ---- mapa ----
    if (g_cl.phase == kClResult && g_cl.map) {
        std::vector<std::pair<ULONGLONG, int>> items;
        g_cl.cells.clear();
        for (int g = 0; g < clean::kGroupCount; ++g) for (int i = 0; i < (int)g_cl.res.groups[g].items.size(); ++i) {
            int on0, all0; ClCount(g_cl.res.groups[g], i, on0, all0);
            if (!all0) continue; // só informa: fica na lista, fora do mapa
            items.push_back({max(g_cl.res.groups[g].items[i].bytes, 1ull), (int)g_cl.cells.size()}); g_cl.cells.push_back({g, i});
        }
        std::sort(items.begin(), items.end(), [](const std::pair<ULONGLONG, int>& a, const std::pair<ULONGLONG, int>& b) { return a.first > b.first; });
        std::vector<std::pair<int, D2D1_RECT_F>> rects;
        D2D1_RECT_F area = D2D1::RectF(view.left, view.top, view.right, view.bottom - p.tk.sp_6 * s * 1.083f);
        ClTreemap(items, 0, items.size(), area, rects);
        for (auto& rc : rects) {
            D2D1_RECT_F r = D2D1::RectF(rc.second.left + p.tk.sp_1 * s, rc.second.top + p.tk.sp_1 * s, rc.second.right - p.tk.sp_1 * s, rc.second.bottom - p.tk.sp_1 * s);
            if (r.right - r.left < p.tk.sp_1 * s || r.bottom - r.top < p.tk.sp_1 * s) continue;
            auto& gi = g_cl.cells[rc.first];
            auto& g = g_cl.res.groups[gi.first]; auto& it = g.items[gi.second];
            int on, all; ClCount(g, gi.second, on, all);
            bool hv = hit(kClCell0 + rc.first, r);
            D2D1_COLOR_F fill = on ? c.accent : c.panel;
            if (on && on < all) fill.a = 0.55f;
            if (on && th.neon) p.Glow(r, p.tk.sp_2 * s);
            p.Fill(r, p.tk.r_chip, fill);
            if (!on) p.Stroke(r, p.tk.r_chip, c.line, 1 * s);
            if (hv) p.Fill(r, p.tk.r_chip, ui::Rgba(th.dark || on ? 0xFFFFFF : 0x000000, 0.12f));
            D2D1_COLOR_F tc = on ? c.onAccent : th.text, t2 = on ? c.onAccent : th.text2;
            if (r.bottom - r.top > p.tk.sp_6 * s * 1.666f && r.right - r.left > p.tk.sp_6 * s * 2.5f) {
                ClText(p, CleanName(it.name), D2D1::RectF(r.left + p.tk.sp_3 * s, r.top + p.tk.sp_2 * s, r.right - p.tk.sp_2 * s, r.top + p.tk.sp_6 * s * 1.083f), p.tk.t_body.size * s, tc, DWRITE_FONT_WEIGHT_SEMI_BOLD);
                std::wstring sz2 = CleanSize(it.bytes); if (it.risk == clean::kReview) sz2 += std::wstring(L" · ") + T(L"Revisar", L"Review", L"Revisar");
                ClText(p, sz2, D2D1::RectF(r.left + p.tk.sp_3 * s, r.top + p.tk.sp_6 * s * 1.083f, r.right - p.tk.sp_2 * s, r.top + p.tk.sp_6 * s * 1.75f), p.tk.t_caption.size * s, t2);
                if (ClIsRunning(it) && r.bottom - r.top > p.tk.sp_6 * s * 2.916f) {
                    p.Feather(kIcoAlert, D2D1::RectF(r.left + p.tk.sp_3 * s, r.bottom - p.tk.sp_6 * s * 1.083f, r.left + p.tk.sp_6 * s * 1.083f, r.bottom - p.tk.sp_2 * s * 0.416f), p.tk.sp_3 * s * 0.541f, tc);
                    ClText(p, T(L"Aberto", L"Open", L"Abierto"), D2D1::RectF(r.left + p.tk.sp_6 * s * 1.25f, r.bottom - p.tk.sp_6 * s * 1.125f, r.right - p.tk.sp_2 * s, r.bottom - p.tk.sp_2 * s * 0.375f), p.tk.t_caption.size * s, tc);
                }
            }
        }
        ClText(p, T(L"Clique num bloco para marcar ou desmarcar.", L"Click a block to select or clear it.", L"Haz clic en un bloque para marcarlo o desmarcarlo."),
               D2D1::RectF(view.left, view.bottom - p.tk.sp_4 * s * 0.833f, view.right, view.bottom), p.tk.t_caption.size * s, th.text2);
    } else {
        // ---- lista ----
        g_cl.rows.clear();
        bool report = g_cl.phase == kClReport, cleaning = g_cl.phase == kClCleaning;
        for (int g = 0; g < clean::kGroupCount; ++g) {
            auto& gr = g_cl.res.groups[g];
            int on, all; ClCount(gr, -1, on, all);
            if (g_cl.phase == kClScan) { g_cl.rows.push_back({0, g, -1, -1}); continue; }
            if (gr.items.empty()) continue;
            if (cleaning && !on) continue;
            g_cl.rows.push_back({0, g, -1, -1});
            if (!g_cl.openG.count(g) || ClGroupLeafOnly(gr)) continue;
            for (int i = 0; i < (int)gr.items.size(); ++i) {
                int io, ia; ClCount(gr, i, io, ia);
                if ((cleaning || report) && !io && on) continue;
                g_cl.rows.push_back({1, g, i, -1});
                if (gr.items[i].leaves.size() > 1 && g_cl.openI.count(g * 1000 + i))
                    for (int l = 0; l < (int)gr.items[i].leaves.size(); ++l) if (!(cleaning || report) || gr.items[i].leaves[l].on || !io) g_cl.rows.push_back({2, g, i, l});
            }
        }
        if (g_cl.phase == kClResult && g_cl.res.total == 0) {
            ClText(p, T(L"Nada para limpar agora.", L"Nothing to clean right now.", L"Nada que limpiar ahora."), D2D1::RectF(view.left, view.top + p.tk.sp_6 * s * 1.25f, view.right, view.top + p.tk.sp_6 * s * 2.333f), p.tk.t_title.size * s, th.text, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, HF);
            ClText(p, T(L"O que é recente ou está em uso fica até poder sair.", L"Recent files and files in use stay until they can go.", L"Lo reciente o en uso se queda hasta poder salir."), D2D1::RectF(view.left, view.top + p.tk.sp_6 * s * 2.5f, view.right, view.top + p.tk.sp_6 * s * 3.333f), p.tk.t_body.size * s, th.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        float total = 0;
        for (auto& r : g_cl.rows) total += (r.depth == 0 ? p.tk.sp_6 * s * 1.2f : p.tk.sp_6 * s * 1.05f);
        g_cl.contentH = total;
        float maxScroll = max(0.0f, total - (view.bottom - view.top));
        g_cl.scroll = min(max(g_cl.scroll, 0.0f), maxScroll);
        rt->PushAxisAlignedClip(view, D2D1_ANTIALIAS_MODE_ALIASED);
        float y = view.top - g_cl.scroll;
        std::lock_guard<std::mutex> lk(g_cl.mu);
        for (size_t ri = 0; ri < g_cl.rows.size(); ++ri) {
            const CleanRow& row = g_cl.rows[ri];
            float rh = (row.depth == 0 ? p.tk.sp_6 * s * 1.2f : p.tk.sp_6 * s * 1.05f);
            D2D1_RECT_F r = D2D1::RectF(view.left, y, view.right, y + rh);
            y += rh;
            if (r.bottom < view.top || r.top > view.bottom) continue;
            auto& gr = g_cl.res.groups[row.g];
            int id0 = kClRow0 + (int)ri * 4;
            bool interactive = g_cl.phase == kClResult || g_cl.phase == kClReport;
            bool rowHot = interactive && hit(id0 + 2, D2D1::RectF(r.left, max(r.top, view.top), r.right, min(r.bottom, view.bottom)));
            bool runningItem = row.depth >= 1 && ClIsRunning(gr.items[row.i]);
            if (row.depth == 1 && runningItem && g_cl.phase == kClResult) p.Fill(r, p.tk.r_chip, c.panel);
            else if (rowHot) p.Fill(r, p.tk.r_chip, c.hover);
            if (ri + 1 < g_cl.rows.size()) p.Fill(D2D1::RectF(r.left, r.bottom - 1 * s, r.right, r.bottom), 0, c.line);
            float x = r.left + p.tk.sp_4 * s + row.depth * 28 * s, cy = (r.top + r.bottom) / 2;
            // seta
            bool expandable = row.depth == 0 ? !gr.items.empty() && !ClGroupLeafOnly(gr) : row.depth == 1 && gr.items[row.i].leaves.size() > 1;
            bool opened = row.depth == 0 ? g_cl.openG.count(row.g) > 0 : g_cl.openI.count(row.g * 1000 + row.i) > 0;
            if (expandable && g_cl.phase != kClScan) {
                D2D1_RECT_F ar = D2D1::RectF(x - 6 * s, cy - 14 * s, x + 22 * s, cy + 14 * s);
                hit(id0, ar);
                p.Feather(opened ? kIcoChevD : kIcoChevR, D2D1::RectF(x, cy - 8 * s, x + 16 * s, cy + 8 * s), 16 * s, th.text2);
            }
            x += 28 * s;
            // marcador: caixa, girando, feito, pulado
            D2D1_RECT_F bx = D2D1::RectF(x, cy - 9 * s, x + 18 * s, cy + 9 * s);
            int on = 0, all = 0;
            if (row.depth == 2) { on = gr.items[row.i].leaves[row.l].on; all = 1; } else ClCount(gr, row.depth == 0 ? -1 : row.i, on, all);
            bool dim = false;
            std::wstring name, size, note; D2D1_COLOR_F noteColor = th.text2; const char* noteIcon = nullptr;
            ULONGLONG bytes = row.depth == 0 ? gr.bytes : row.depth == 1 ? gr.items[row.i].bytes : gr.items[row.i].leaves[row.l].bytes;
            name = row.depth == 0 ? CleanGroupName(row.g) : row.depth == 1 ? CleanName(gr.items[row.i].name) : CleanName(gr.items[row.i].leaves[row.l].name);
            size = CleanSize(bytes);
            // resultado por linha (limpando e relatório)
            ULONGLONG rem = 0, kep = 0; bool anyOut = false, allDone = true, anySkip = false, anyRecycled = false;
            for (int i = 0; i < (int)gr.items.size(); ++i) if (row.depth == 0 || i == row.i)
                for (int l = 0; l < (int)gr.items[i].leaves.size(); ++l) if ((row.depth < 2 || l == row.l) && gr.items[i].leaves[l].on) {
                    int k = ClKey(row.g, i, l);
                    auto f = g_cl.out.find(k);
                    if (f == g_cl.out.end()) { allDone = false; continue; }
                    anyOut = true; rem += f->second.removed; kep += f->second.kept; anyRecycled |= f->second.recycled && f->second.removed;
                    if (g_cl.skippedLeaves.count(k) || g_cl.stillOpen.count(k)) anySkip = true;
                }
            if (g_cl.phase == kClScan) {
                bool cur = !gr.done && (row.g == 0 || g_cl.res.groups[row.g - 1].done);
                if (gr.done) ClCheckbox(p, c, bx, on == all && all ? 1 : on ? 2 : 0, s);
                else if (cur) ClSpinner(p, c, bx, s);
                else p.Fill(bx, p.tk.r_chip, c.track);
                dim = !gr.done;
                size = gr.done ? (gr.items.empty() ? T(L"nada", L"nothing", L"nada") : CleanSize(gr.bytes)) : L"—";
                if (cur) note = T(L"Procurando…", L"Searching…", L"Buscando…");
            } else if (g_cl.phase == kClCleaning) {
                if (allDone && anyOut) p.Feather(anySkip ? kIcoSkip : kIcoCheck, bx, 16 * s, anySkip ? c.warn : th.text, anySkip ? 2.0f : 2.5f);
                else if (g_cl.cleaningG == row.g && (row.depth == 0 || anyOut || true)) ClSpinner(p, c, bx, s);
                else p.Stroke(bx, p.tk.r_control * s * 0.9f, c.line, 1.5f * s);
                if (allDone && anyOut) { size = CleanSize(rem); note = anySkip ? T(L"Pulado · programa aberto", L"Skipped · app open", L"Omitido · programa abierto") : T(L"Limpo", L"Clean", L"Limpio"); }
            } else if (report) {
                if (!on) { p.Feather(kIcoSkip, bx, 16 * s, th.text2); dim = true; note = T(L"Não marcado", L"Not selected", L"No marcado"); }
                else {
                    p.Feather(anySkip ? kIcoSkip : kIcoCheck, bx, 16 * s, anySkip ? c.warn : th.text, anySkip ? 2.0f : 2.5f);
                    size = CleanSize(rem);
                    if (anySkip && !rem) { note = T(L"Pulado · programa aberto", L"Skipped · app open", L"Omitido · programa abierto"); noteColor = c.warn; }
                    else if (anyRecycled) note = T(L"Na Lixeira", L"In the Recycle Bin", L"En la Papelera");
                    else if (kep >= (1u << 20)) { note = CleanSize(kep) + T(L" em uso ficaram", L" in use stayed", L" en uso se quedaron"); }
                }
            } else {
                bool boxHot = hit(id0 + 1, D2D1::RectF(bx.left - p.tk.sp_2 * s * 1.5f, bx.top - p.tk.sp_2 * s * 1.5f, bx.right + p.tk.sp_2 * s * 1.5f, bx.bottom + p.tk.sp_2 * s * 1.5f));
                if (boxHot) p.Fill(D2D1::RectF(bx.left - p.tk.sp_1 * s * 1.25f, bx.top - p.tk.sp_1 * s * 1.25f, bx.right + p.tk.sp_1 * s * 1.25f, bx.bottom + p.tk.sp_1 * s * 1.25f), p.tk.r_chip, c.hover);
                if (all) ClCheckbox(p, c, bx, on == all ? 1 : on ? 2 : 0, s);
                else { p.Feather(kIcoInfo, bx, 16 * s, th.text2); if (row.depth) note = T(L"Só informa", L"Info only", L"Solo informa"); }
                if (runningItem && row.depth == 1) { note = T(L"Aberto · feche para limpar tudo", L"Open · close it to clean everything", L"Abierto · ciérralo para limpiar todo"); noteColor = c.warn; noteIcon = kIcoAlert; }
                if (row.depth == 2 && clean::TargetOf(gr.items[row.i].leaves[row.l]).risk == clean::kReview && !ClPermanent()) note = T(L"Vai para a Lixeira", L"Goes to the Recycle Bin", L"Va a la Papelera");
            }
            x += p.tk.sp_6 * s * 0.75f;
            float sizeW = 84 * s, right = r.right - p.tk.sp_4 * s;
            ClText(p, size, D2D1::RectF(right - sizeW, r.top, right, r.bottom), row.depth == 0 ? p.tk.t_section.size : p.tk.t_body.size, row.depth == 0 && !dim ? th.text : th.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            right -= sizeW + p.tk.sp_3 * s;
            // selo Seguro / Revisar (grupo, na análise)
            if (row.depth == 0 && (g_cl.phase == kClResult || (g_cl.phase == kClScan && gr.done && !gr.items.empty()))) {
                int risk = ClGroupRisk(gr);
                bool rev = risk == clean::kReview;
                std::wstring bl = rev ? T(L"Revisar", L"Review", L"Revisar") : risk == clean::kInform ? T(L"Só informa", L"Info only", L"Solo informa") : T(L"Seguro", L"Safe", L"Seguro");
                float bw = ClMeasure(p, bl, p.tk.t_caption.size) + (rev ? p.tk.sp_6 * s * 0.75f : p.tk.sp_4 * s * 1.125f);
                D2D1_RECT_F br = D2D1::RectF(right - bw, cy - p.tk.sp_3 * s * 0.91f, right, cy + p.tk.sp_3 * s * 0.91f);
                p.Stroke(br, p.tk.r_chip * s * 0.68f, rev ? c.accent : c.line, 1 * s);
                float tx = br.left + 9 * s;
                if (rev) { p.Fill(D2D1::RectF(tx, cy - p.tk.sp_1 * s * 0.75f, tx + p.tk.sp_1 * s * 1.5f, cy + p.tk.sp_1 * s * 0.75f), p.tk.sp_1 * s * 0.75f, c.accent); tx += 12 * s; }
                ClText(p, bl, D2D1::RectF(tx, br.top, br.right, br.bottom), p.tk.t_caption.size, rev ? th.text : th.text2);
                right = br.left - p.tk.sp_3 * s;
            }
            if (!note.empty()) {
                float nw = min(ClMeasure(p, note, p.tk.t_caption.size * 1.04f) + (noteIcon ? p.tk.sp_6 * s * 0.5f : 0), max(0.0f, right - x - p.tk.sp_6 * s * 5));
                if (nw > p.tk.sp_6 * s * 1.0f) {
                    D2D1_RECT_F nr = D2D1::RectF(right - nw, r.top, right, r.bottom);
                    if (noteIcon) p.Feather(noteIcon, D2D1::RectF(nr.left, cy - p.tk.sp_1 * s * 1.75f, nr.left + p.tk.sp_3 * s * 1.16f, cy + p.tk.sp_1 * s * 1.75f), p.tk.sp_3 * s * 1.16f, noteColor);
                    ClText(p, note, D2D1::RectF(nr.left + (noteIcon ? p.tk.sp_6 * s * 0.5f : 0), nr.top, nr.right, nr.bottom), p.tk.t_caption.size * 1.04f, noteColor);
                    right = nr.left - p.tk.sp_3 * s;
                }
            }
            ClText(p, name, D2D1::RectF(x, r.top, right, r.bottom), row.depth == 0 ? p.tk.t_section.size : p.tk.t_body.size, dim ? th.text2 : th.text, row.depth == 0 ? DWRITE_FONT_WEIGHT_MEDIUM : DWRITE_FONT_WEIGHT_NORMAL);
        }
        rt->PopAxisAlignedClip();
        if (maxScroll > 0) { // barra de rolagem fina
            float vh = view.bottom - view.top, th2 = max(p.tk.sp_6 * s * 0.75f, vh * vh / total), ty = view.top + (vh - th2) * (g_cl.scroll / maxScroll);
            if (th.neon) p.ScrollBar(D2D1::RectF(view.right + p.tk.sp_1 * s * 1.5f, view.top, view.right + p.tk.sp_2 * s * 1.25f, view.bottom), g_cl.scroll / maxScroll, vh / total);
            else p.Fill(D2D1::RectF(view.right + p.tk.sp_1 * s * 1.5f, ty, view.right + p.tk.sp_2 * s * 1.25f, ty + th2), p.tk.sp_1 * s * 0.5f, c.line);
        }
        if (footer) { // o que acontece ao limpar o item sob o mouse (consequência do targets.json)
            std::wstring tip;
            int hr = g_cl.hot >= kClRow0 && g_cl.hot < kClCell0 ? (g_cl.hot - kClRow0) / 4 : -1;
            if (hr >= 0 && hr < (int)g_cl.rows.size() && g_cl.rows[hr].depth > 0) {
                const CleanRow& rw = g_cl.rows[hr];
                const auto& it = g_cl.res.groups[rw.g].items[rw.i];
                const clean::Leaf& lf = it.leaves[rw.depth == 2 ? rw.l : 0];
                const wchar_t* n = clean::TargetOf(lf).note;
                if (n && *n && i18n::Current() == i18n::kPt) tip = n; // as consequências só existem em português
                if (lf.protectedFiles) {
                    wchar_t pb[160]; swprintf(pb, 160, T(L" %ls em arquivos de trabalho ficam.", L" %ls of work files stay.", L" %ls de archivos de trabajo se quedan."), CleanSize(lf.protectedBytes).c_str());
                    tip += pb;
                }
            }
            if (tip.empty()) tip = T(L"Passe o mouse num item para ver o que muda ao limpar.", L"Hover an item to see what changes when it's cleaned.", L"Pasa el ratón sobre un elemento para ver qué cambia al limpiarlo.");
            p.Paragraph(tip, view.left + p.tk.sp_1 * s, view.bottom + p.tk.sp_3 * s, view.right - view.left - p.tk.sp_2 * s, p.tk.t_caption.size * 1.04f, th.text2, true, 1.35f);
        }
    }

    // ---- diálogos ----
    if (g_cl.overlay) {
        g_cl.hits.erase(std::remove_if(g_cl.hits.begin(), g_cl.hits.end(), [](const std::pair<int, D2D1_RECT_F>& h2) { return h2.first != kClClose && h2.first != kClMin; }), g_cl.hits.end());
        p.Fill(D2D1::RectF(0, 0, sz.width, sz.height), 0, ui::Rgba(0x000000, 0.48f));
        std::wstring title, text; std::vector<std::pair<std::wstring, std::wstring>> list; std::wstring cancel, ok; const char* icon;
        if (g_cl.overlay == kClConfirm) {
            icon = kIcoTrash;
            title = std::wstring(T(L"Limpar ", L"Clean ", L"¿Limpiar ")) + CleanSize(sel) + (i18n::Current() == i18n::kEs ? L"?" : L"?");
            bool anyReview = false, bin = false;
            for (int g = 0; g < clean::kGroupCount; ++g) {
                int on, all; ClCount(g_cl.res.groups[g], -1, on, all);
                if (!on) continue;
                ULONGLONG b = 0; for (auto& it : g_cl.res.groups[g].items) for (auto& l : it.leaves) if (l.on) { b += l.bytes; if (clean::TargetOf(l).risk == clean::kReview) anyReview = true; if (clean::TargetOf(l).kind == clean::kRecycleBin) bin = true; }
                list.push_back({CleanGroupName(g), CleanSize(b)});
            }
            text = ClPermanent() ? T(L"Tudo é excluído de vez, sem passar pela Lixeira. Arquivos de trabalho (PSD, AI, INDD, PDF…) e o que estiver em uso ficam.",
                                     L"Everything is deleted permanently, skipping the Recycle Bin. Work files (PSD, AI, INDD, PDF…) and files in use stay.",
                                     L"Todo se elimina definitivamente, sin pasar por la Papelera. Los archivos de trabajo (PSD, AI, INDD, PDF…) y lo que esté en uso se quedan.")
                 : anyReview ? T(L"Os itens seguros são excluídos de vez; os de Revisar vão para a Lixeira. Arquivos de trabalho e o que estiver em uso ficam.",
                                 L"Safe items are deleted permanently; Review items go to the Recycle Bin. Work files and files in use stay.",
                                 L"Lo seguro se elimina definitivamente; lo de Revisar va a la Papelera. Los archivos de trabajo y lo que esté en uso se quedan.")
                             : T(L"São caches e temporários que os programas recriam sozinhos. Arquivos de trabalho e o que estiver em uso ficam.",
                                 L"These are caches and temporary files that apps recreate on their own. Work files and files in use stay.",
                                 L"Son cachés y temporales que los programas vuelven a crear solos. Los archivos de trabajo y lo que esté en uso se quedan.");
            if (bin) text += T(L" A Lixeira será esvaziada.", L" The Recycle Bin will be emptied.", L" La Papelera se vaciará.");
            cancel = T(L"Cancelar", L"Cancel", L"Cancelar"); ok = T(L"Limpar", L"Clean", L"Limpiar");
        } else {
            icon = kIcoAlert;
            std::wstring exe = g_cl.askQueue.empty() ? L"" : g_cl.askQueue.front(), name = CleanProgramName(exe);
            swprintf(buf, 256, T(L"%ls está aberto", L"%ls is open", L"%ls está abierto"), name.c_str()); title = buf;
            bool browser = false; ULONGLONG b = 0;
            for (auto& g : g_cl.res.groups) for (auto& it : g.items) for (auto& pr : it.procs) if (clean::LowerPath(pr) == exe) {
                for (auto& l : it.leaves) if (l.on) b += l.bytes;
                if (g.id == clean::kGBrowsers) browser = true;
            }
            text = browser ? T(L"Parte do cache está em uso. Feche para limpar tudo; as abas voltam quando você abrir de novo.",
                               L"Part of the cache is in use. Close it to clean everything; your tabs come back when you reopen it.",
                               L"Parte de la caché está en uso. Ciérralo para limpiar todo; las pestañas vuelven al abrirlo de nuevo.")
                           : T(L"Parte dos arquivos está em uso. Salve seu trabalho: o programa vai perguntar antes de fechar se houver algo aberto.",
                               L"Some files are in use. Save your work: the app will ask before closing if anything is unsaved.",
                               L"Parte de los archivos está en uso. Guarda tu trabajo: el programa preguntará antes de cerrar si hay algo sin guardar.");
            list.push_back({name + T(L" · em uso", L" · in use", L" · en uso"), CleanSize(b)});
            swprintf(buf, 256, T(L"Pular o %ls", L"Skip %ls", L"Omitir %ls"), name.c_str()); cancel = buf;
            swprintf(buf, 256, T(L"Fechar e limpar", L"Close and clean", L"Cerrar y limpiar")); ok = buf;
        }
        float dw = min(480 * s, sz.width - 32 * s), ix = 24 * s;
        float textH = p.Paragraph(text, 0, 0, dw - 2 * ix - p.tk.sp_6 * s * 1.35f, p.tk.t_section.size * 1.125f, th.text2, false);
        float dh = p.tk.sp_6 * s + max(p.tk.sp_6 * s * 1.0f, p.tk.sp_6 * s * 0.75f + textH) + p.tk.sp_4 * s + list.size() * p.tk.sp_6 * s * 0.9f + p.tk.sp_6 * s * 0.5f + p.tk.sp_6 * s * 1.0f + p.tk.sp_6 * s;
        D2D1_RECT_F d = D2D1::RectF((sz.width - dw) / 2, (sz.height - dh) / 2, (sz.width + dw) / 2, (sz.height + dh) / 2);
        p.Shadow(d, p.tk.r_card * s);
        D2D1_COLOR_F surf = th.classic ? ui::Rgba(0x353A42) : th.neon ? ui::Rgba(0x1A1A1E) : th.dark ? ui::Rgba(0x1F1F22) : ui::Rgba(0xFFFFFF);
        p.Fill(d, p.tk.r_card * s, surf);
        p.Stroke(d, p.tk.r_card * s, c.line, 1 * s);
        float yy = d.top + p.tk.sp_6 * s;
        D2D1_RECT_F ib = D2D1::RectF(d.left + ix, yy, d.left + ix + p.tk.sp_6 * s * 1.0f, yy + p.tk.sp_6 * s * 1.0f);
        D2D1_COLOR_F ibg = c.accent; ibg.a = th.dark && !th.classic && !th.neon && g_cfg.themeMode == ui::kThemeDark ? 0.14f : 0.18f;
        p.Fill(ib, p.tk.r_control * s * 0.625f, ibg);
        p.Feather(icon, ib, p.tk.sp_6 * s * 0.5f, th.dark && g_cfg.themeMode == ui::kThemeDark ? th.text : c.accent);
        float tx = ib.right + p.tk.sp_3 * s * 1.166f;
        ClText(p, title, D2D1::RectF(tx, yy, d.right - ix, yy + p.tk.sp_6 * s * 1.625f), p.tk.t_title.size * 0.95f, th.text, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, HF);
        p.Paragraph(text, tx, yy + p.tk.sp_6 * s * 1.875f, d.right - ix - tx, p.tk.t_section.size * 1.125f, th.text2);
        yy += max(p.tk.sp_6 * s * 1.0f, p.tk.sp_6 * s * 0.75f + textH) + p.tk.sp_4 * s;
        for (auto& li : list) {
            ClText(p, li.first, D2D1::RectF(d.left + ix, yy, d.right - ix - p.tk.sp_6 * s * 2.25f, yy + p.tk.sp_6 * s * 0.9f), p.tk.t_body.size, th.text);
            ClText(p, li.second, D2D1::RectF(d.right - ix - p.tk.sp_6 * s * 2.25f, yy, d.right - ix, yy + p.tk.sp_6 * s * 0.9f), p.tk.t_body.size, th.text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            p.Fill(D2D1::RectF(d.left + ix, yy + p.tk.sp_6 * s * 0.9f - 1 * s, d.right - ix, yy + p.tk.sp_6 * s * 0.9f), 0, c.line);
            yy += p.tk.sp_6 * s * 0.9f;
        }
        yy += p.tk.sp_6 * s * 0.5f;
        float okW = ClMeasure(p, ok, p.tk.t_body.size, DWRITE_FONT_WEIGHT_SEMI_BOLD) + p.tk.sp_6 * s * 1.0f, caW = ClMeasure(p, cancel, p.tk.t_body.size, DWRITE_FONT_WEIGHT_SEMI_BOLD) + p.tk.sp_6 * s * 0.9f;
        D2D1_RECT_F okR = D2D1::RectF(d.right - ix - okW, yy, d.right - ix, yy + p.tk.sp_6 * s * 1.0f), caR = D2D1::RectF(okR.left - p.tk.sp_2 * s * 1.25f - caW, yy, okR.left - p.tk.sp_2 * s * 1.25f, yy + p.tk.sp_6 * s * 1.0f);
        g_cl.hits.push_back({kClDlgCancel, caR}); g_cl.hits.push_back({kClDlgOk, okR});
        ClButton(p, c, caR, cancel, false, g_cl.hot == kClDlgCancel, s);
        ClButton(p, c, okR, ok, true, g_cl.hot == kClDlgOk, s);
    }
    if (th.neon) p.WindowFrame(D2D1::RectF(0, 0, sz.width, sz.height), th.glass ? 8 * s : 20 * s);
    if (openLayer) rt->PopLayer();
    rt->SetTransform(D2D1::IdentityMatrix());
    if (!g_cl.surf.End()) InvalidateRect(h, nullptr, FALSE);
    an.Pump(h, ui::kAnimTick);
    // número grande e indicador girando: o timer só existe enquanto há o que mover
    if (g_cl.phase == kClScan || g_cl.phase == kClCleaning || fabsf((float)g_cl.shownTarget - g_cl.shown) > 1) SetTimer(h, 1, 16, nullptr); else KillTimer(h, 1);
}

// ---------- ações ----------

static void OpenSettings();
// Fecha com fade + escala (~150 ms); a janela some de verdade no fim (WM_CLOSE com wp = 1).
static void CloseCleanWindow() {
    if (!g_cl.wnd) return;
    if (g_clClosing) return;
    if (!IsWindowVisible(g_cl.wnd) || IsIconic(g_cl.wnd)) { DestroyWindow(g_cl.wnd); return; }
    g_clClosing = true; InvalidateRect(g_cl.wnd, nullptr, FALSE);
}

static void ClAction(int id) {
    if (id == kClClose) { CloseCleanWindow(); return; }
    if (id == kClMin) { ShowWindow(g_cl.wnd, SW_MINIMIZE); return; }
    if (id == kClSettings) { OpenSettings(); return; }
    if (g_cl.overlay) {
        if (id == kClDlgCancel) {
            if (g_cl.overlay == kClConfirm) g_cl.overlay = kClNone;
            else { g_cl.skipped.push_back(g_cl.askQueue.front()); g_cl.askQueue.erase(g_cl.askQueue.begin()); if (g_cl.askQueue.empty()) ClStartClean(); }
        } else if (id == kClDlgOk) {
            if (g_cl.overlay == kClConfirm) { g_cl.overlay = kClNone; ClAfterConfirm(); }
            else { g_cl.toClose.push_back(g_cl.askQueue.front()); g_cl.askQueue.erase(g_cl.askQueue.begin()); if (g_cl.askQueue.empty()) ClStartClean(); }
        }
        return;
    }
    if (id == kClRescan) { ClStartScan(); return; }
    if (id == kClList) { g_cl.map = false; return; }
    if (id == kClMap) { g_cl.map = true; return; }
    if (id == kClSecondary) {
        if (g_cl.phase == kClScan || g_cl.phase == kClCleaning) { g_cl.stop = true; return; } // Parar: a thread avisa quando terminar
        if (g_cl.phase == kClReport) { // esvaziar a Lixeira
            HCURSOR old = SetCursor(LoadCursor(nullptr, IDC_WAIT));
            SHEmptyRecycleBinW(g_cl.wnd, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
            SetCursor(old);
            g_cl.recycledBytes = 0; ClRefreshDisk();
        }
        return;
    }
    if (id == kClPrimary) {
        if (g_cl.phase == kClResult && clean::Selected(g_cl.res)) g_cl.overlay = kClConfirm;
        else if (g_cl.phase == kClReport) CloseCleanWindow();
        return;
    }
    if (id >= kClCell0) {
        int ci = id - kClCell0;
        if (ci < (int)g_cl.cells.size()) { auto& g = g_cl.res.groups[g_cl.cells[ci].first]; int on, all; ClCount(g, g_cl.cells[ci].second, on, all); ClSet(g, g_cl.cells[ci].second, on < all); }
        return;
    }
    if (id >= kClRow0) {
        int ri = (id - kClRow0) / 4, part = (id - kClRow0) % 4;
        if (ri >= (int)g_cl.rows.size()) return;
        CleanRow row = g_cl.rows[ri];
        auto& gr = g_cl.res.groups[row.g];
        bool expandable = row.depth == 0 ? !gr.items.empty() && !ClGroupLeafOnly(gr) : row.depth == 1 && gr.items[row.i].leaves.size() > 1;
        if ((part == 0 || (part == 2 && expandable)) && expandable) {
            if (row.depth == 0) { if (!g_cl.openG.erase(row.g)) g_cl.openG.insert(row.g); }
            else { int k = row.g * 1000 + row.i; if (!g_cl.openI.erase(k)) g_cl.openI.insert(k); }
            return;
        }
        if (g_cl.phase != kClResult) return;
        if (row.depth == 2) { auto& l = gr.items[row.i].leaves[row.l]; l.on = !l.on && clean::Actionable(l); }
        else { int on, all; ClCount(gr, row.depth == 0 ? -1 : row.i, on, all); ClSet(gr, row.depth == 0 ? -1 : row.i, on < all); }
    }
}

static LRESULT CALLBACK CleanProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCHITTEST: {
        LRESULT r = DefWindowProcW(h, msg, wp, lp);
        if (r != HTCLIENT) return r;
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(h, &pt);
        RECT rc; GetClientRect(h, &rc);
        int e = (int)(6 * WndScale(h));
        if (!IsZoomed(h)) { // bordas para redimensionar (janela sem moldura)
            bool L = pt.x < e, R = pt.x >= rc.right - e, Tp = pt.y < e, B = pt.y >= rc.bottom - e;
            if (Tp && L) return HTTOPLEFT; if (Tp && R) return HTTOPRIGHT; if (B && L) return HTBOTTOMLEFT; if (B && R) return HTBOTTOMRIGHT;
            if (L) return HTLEFT; if (R) return HTRIGHT; if (Tp) return HTTOP; if (B) return HTBOTTOM;
        }
        for (auto& x : g_cl.hits) if (ui::Hit(x.second, pt)) return HTCLIENT;
        return pt.y < 52 * WndScale(h) ? HTCAPTION : HTCLIENT;
    }
    case WM_GETMINMAXINFO: { auto* mm = (MINMAXINFO*)lp; float s = WndScale(h); mm->ptMinTrackSize.x = (LONG)(560 * s); mm->ptMinTrackSize.y = (LONG)(520 * s); return 0; }
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int hot = 0;
        for (auto& x : g_cl.hits) if (ui::Hit(x.second, pt)) hot = x.first;
        g_clMouse = pt;
        if (hot != g_cl.hot || g_theme.neon) { g_cl.hot = hot; InvalidateRect(h, nullptr, FALSE); } // Crispy: a luz segue o cursor
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, h, 0}; TrackMouseEvent(&tme);
        return 0;
    }
    case WM_SETCURSOR: if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursor(nullptr, g_cl.hot ? IDC_HAND : IDC_ARROW)); return TRUE; } break;
    case WM_MOUSELEAVE: g_cl.hot = 0; g_clMouse = {-100000, -100000}; g_clDown = false; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONUP: if (g_clDown) { g_clDown = false; InvalidateRect(h, nullptr, FALSE); } return 0;
    case WM_LBUTTONDOWN: {
        g_clDown = true;
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int id = 0;
        for (auto& x : g_cl.hits) if (ui::Hit(x.second, pt)) id = x.first;
        if (id) ClAction(id);
        if (g_cl.wnd) InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEWHEEL: g_cl.scroll -= GET_WHEEL_DELTA_WPARAM(wp) / 120.0f * 84 * WndScale(h); InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) { if (g_cl.overlay) ClAction(kClDlgCancel); else if (g_cl.phase != kClCleaning) CloseCleanWindow(); }
        else if (wp == VK_RETURN) { if (g_cl.overlay) ClAction(kClDlgOk); else if (g_cl.phase == kClResult || g_cl.phase == kClReport) ClAction(kClPrimary); }
        else if (wp == VK_F5 && (g_cl.phase == kClResult || g_cl.phase == kClReport) && !g_cl.overlay) ClStartScan();
        else if (wp == VK_NEXT || wp == VK_DOWN) g_cl.scroll += (wp == VK_NEXT ? 400 : 42) * WndScale(h);
        else if (wp == VK_PRIOR || wp == VK_UP) g_cl.scroll -= (wp == VK_PRIOR ? 400 : 42) * WndScale(h);
        if (g_cl.wnd) InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_CLEAN_SCAN: ClPullScan(); InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_CLEAN_SCANDONE:
        ClPullScan(); ClRefreshRunning();
        if (g_cl.worker.joinable()) g_cl.worker.join();
        g_cl.stop = false;
        g_cl.phase = kClResult;
        WriteDword(L"CleanLastFound", (DWORD)min(clean::Selected(g_cl.res) >> 20, 0xFFFFFFFFull)); // em MB, para o widget de disco
        WidgetsRenderAll();
        InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_CLEAN_TICK: InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_CLEAN_DONE: {
        if (g_cl.worker.joinable()) g_cl.worker.join();
        g_cl.stop = false;
        std::lock_guard<std::mutex> lk(g_cl.mu);
        g_cl.removed = g_cl.kept = g_cl.recycledBytes = 0;
        for (auto& o : g_cl.out) { g_cl.removed += o.second.removed; g_cl.kept += o.second.kept; if (o.second.recycled) g_cl.recycledBytes += o.second.removed; }
        g_cl.phase = kClReport; g_cl.status.clear(); g_cl.cleaningG = -1;
        ClRefreshDisk();
        { ULONGLONG left = clean::Selected(g_cl.res); left = left > g_cl.removed ? left - g_cl.removed : 0; WriteDword(L"CleanLastFound", (DWORD)min(left >> 20, 0xFFFFFFFFull)); }
        WidgetsRenderAll();
        if (g_cl.closeAfter) { CloseCleanWindow(); return 0; }
        InvalidateRect(h, nullptr, FALSE); return 0;
    }
    case WM_TIMER: { // número animado e girando
        float target = (float)g_cl.shownTarget, d = target - g_cl.shown;
        bool busy = g_cl.phase == kClScan || g_cl.phase == kClCleaning;
        if (wp == ui::kAnimTick) { KillTimer(h, wp); InvalidateRect(h, nullptr, FALSE); return 0; }
        if (fabsf(d) > 1 && !ui::ReducedMotion()) g_cl.shown += d * 0.22f; else g_cl.shown = target; // ~300 ms; sem contagem ao reduzir movimento
        if (busy || fabsf(d) > 1) InvalidateRect(h, nullptr, FALSE); else KillTimer(h, 1);
        return 0;
    }
    case WM_ACTIVATE: if (LOWORD(wp) != WA_INACTIVE && g_cl.phase == kClResult) { ClRefreshRunning(); InvalidateRect(h, nullptr, FALSE); } break; // fechou o Chrome? some o aviso
    case WM_SETTINGCHANGE: InvalidateRect(h, nullptr, FALSE); break;
    case WM_SIZE: g_cl.surf.Resize(LOWORD(lp), HIWORD(lp)); InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(h, &ps); PaintClean(h); EndPaint(h, &ps); return 0; }
    case WM_CLOSE: if (g_cl.phase == kClCleaning) { g_cl.stop = true; g_cl.closeAfter = true; return 0; } if (wp == 1) DestroyWindow(h); else CloseCleanWindow(); return 0; // limpando: para entre um arquivo e outro e fecha
    case WM_DESTROY: g_clClosing = false; KillTimer(h, ui::kAnimTick); ClJoin(); g_cl.surf.Release(); g_cl.hits.clear(); g_cl.wnd = nullptr; return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void OpenCleanWindow() {
    if (g_lite) return; // Lite: sem Limpeza
    if (g_cl.wnd) { ShowWindow(g_cl.wnd, SW_RESTORE); SetForegroundWindow(g_cl.wnd); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc = {}; wc.lpfnWndProc = CleanProc; wc.hInstance = g_inst; wc.lpszClassName = L"VizuClean";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIcon(g_inst, MAKEINTRESOURCE(1));
        RegisterClassW(&wc); reg = true;
    }
    POINT pt; GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(mon, &mi);
    UINT dx = 96, dy = 96; GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy);
    float s = dx / 96.0f;
    int w = min((int)(960 * s), (int)(mi.rcWork.right - mi.rcWork.left - 40)), h = min((int)(680 * s), (int)(mi.rcWork.bottom - mi.rcWork.top - 40));
    g_cl.hot = 0; g_cl.openG = {clean::kGBrowsers}; g_cl.closeAfter = false;
    g_cl.wnd = CreateWindowExW(WS_EX_APPWINDOW, L"VizuClean", T(L"Limpeza — Vizu", L"Cleanup — Vizu", L"Limpieza — Vizu"), WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_THICKFRAME,
                               mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2, mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2, w, h,
                               nullptr, nullptr, g_inst, nullptr);
    ui::EnableGlass(g_cl.wnd, g_theme.dark);
    ShowWindow(g_cl.wnd, SW_SHOW);
    SetForegroundWindow(g_cl.wnd);
    SetTimer(g_cl.wnd, 1, 16, nullptr);
    ClStartScan();
}
