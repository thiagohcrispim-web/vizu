// AiNotch.inc.h - widget "Uso de IA" no formato de trilho preso à borda da tela (ideia visual do Codenotch).
// Um anel de progresso por IA (limite de 5 h) com a porcentagem embaixo; passar o mouse abre o cartão de detalhes ao lado.
// Arrastar move o trilho ao longo da borda; levar até outra borda (esquerda, direita, topo, base) troca de lado.
// Incluído no Widgets.inc.h depois do WidgetPresent. Os dados vêm do AiBattery.inc.h.

enum { kEdgeRight, kEdgeLeft, kEdgeTop, kEdgeBottom };

struct Notch {
    HWND wnd = nullptr;
    int edge = kEdgeRight;
    LONG pos = 0;              // centro do trilho ao longo da borda (coordenada de tela)
    POINT mon{};               // um ponto do monitor escolhido
    int hot = -1;              // IA sob o mouse (0 Claude, 1 ChatGPT); -1 nenhuma
    int open = -1;             // cartão aberto
    bool btnHot = false;
    bool pressed = false, dragging = false;
    POINT down{};
    ULONGLONG leaveAt = 0;
    D2D1_RECT_F rItem[2]{}, rBtn{}, rCard{};
};
static Notch g_nt;

static bool NotchVertical() { return g_nt.edge == kEdgeRight || g_nt.edge == kEdgeLeft; }

static RECT NotchWork() {
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromPoint(g_nt.mon, MONITOR_DEFAULTTOPRIMARY), &mi);
    return mi.rcWork;
}

// ---------- Claude Code: instalar/entrar e renovar o login ----------

static std::wstring ClaudeExe() {
    std::wstring local = AiHome() + L"\\.local\\bin\\claude.exe"; // instalador oficial
    if (GetFileAttributesW(local.c_str()) != INVALID_FILE_ATTRIBUTES) return local;
    wchar_t b[MAX_PATH];
    if (SearchPathW(nullptr, L"claude.exe", nullptr, MAX_PATH, b, nullptr)) return b;
    if (SearchPathW(nullptr, L"claude.cmd", nullptr, MAX_PATH, b, nullptr)) return b; // instalação antiga pelo npm
    return L"";
}

// Abre um terminal que instala o Claude Code (instalador oficial da Anthropic, se ainda não estiver instalado)
// e roda o `claude` para o login. Quem digita e entra é o usuário; o Vizu não vê nada disso.
static void ClaudeInstallAndLogin() {
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    std::wstring bat = std::wstring(tmp) + L"vizu-claude-code.bat";
    const char* body =
        "@echo off\r\n"
        "chcp 65001 >nul\r\n"
        "title Vizu - Claude Code\r\n"
        "set \"PATH=%USERPROFILE%\\.local\\bin;%PATH%\"\r\n"
        "where claude >nul 2>nul\r\n"
        "if errorlevel 1 (\r\n"
        "  echo Instalando o Claude Code com o instalador oficial da Anthropic...\r\n"
        "  powershell -NoProfile -ExecutionPolicy Bypass -Command \"irm https://claude.ai/install.ps1 | iex\"\r\n"
        "  set \"PATH=%USERPROFILE%\\.local\\bin;%PATH%\"\r\n"
        ")\r\n"
        "where claude >nul 2>nul\r\n"
        "if errorlevel 1 (\r\n"
        "  echo.\r\n"
        "  echo Nao foi possivel instalar o Claude Code. Veja https://claude.com/claude-code\r\n"
        "  pause\r\n"
        "  exit /b 1\r\n"
        ")\r\n"
        "echo.\r\n"
        "echo Entre com a sua conta do Claude no navegador que vai abrir.\r\n"
        "echo Quando o Claude Code abrir aqui, digite /exit e aperte Enter para fechar.\r\n"
        "echo O widget do Vizu atualiza sozinho em seguida.\r\n"
        "echo.\r\n"
        "claude\r\n";
    HANDLE h = CreateFileW(bat.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0; WriteFile(h, body, (DWORD)strlen(body), &w, nullptr); CloseHandle(h);
    std::wstring args = L"/c \"\"" + bat + L"\"\"";
    ShellExecuteW(nullptr, L"open", L"cmd.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
}

// O token do Claude Code dura poucas horas e só o CLI renova o arquivo. Perto de vencer, roda `claude -p` escondido
// com a entrada vazia: o próprio Claude Code renova o login e grava o arquivo; o Vizu só lê depois.
static ULONGLONG g_claudeRenewAt = 0;
static void ClaudeRenewIfNeeded() {
    ULONGLONG now = AiNowMs();
    if (now < g_claudeRenewAt) return;
    std::string cred = AiReadFile(AiHome() + L"\\.claude\\.credentials.json");
    double exp = 0;
    if (AiJsonStr(cred, "refreshToken").empty() || !AiJsonNum(cred, "expiresAt", &exp) || exp <= 0) return; // sem login: nada a renovar
    if ((ULONGLONG)exp > now + 10 * 60 * 1000) return; // ainda vale por mais de 10 min
    std::wstring exe = ClaudeExe();
    if (exe.empty()) return;
    g_claudeRenewAt = now + 15 * 60 * 1000; // no máximo uma tentativa a cada 15 min
    bool cmd = exe.size() > 4 && !_wcsicmp(exe.c_str() + exe.size() - 4, L".cmd");
    std::wstring line = cmd ? L"cmd.exe /c \"\"" + exe + L"\" -p\"" : L"\"" + exe + L"\" -p";
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    si.hStdInput = si.hStdOutput = si.hStdError = nul;
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> buf(line.begin(), line.end()); buf.push_back(0);
    if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        if (WaitForSingleObject(pi.hProcess, 60000) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
}

// ---------- desenho ----------

static D2D1_COLOR_F NotchLevel(double pct) { return pct >= 90 ? ui::Rgba(0xFF4D3D) : pct >= 75 ? ui::Rgba(0xFFB020) : ui::Rgba(0x2BD97C); }

static void NotchRing(ID2D1RenderTarget* rt, D2D1_POINT_2F c, float r, float w, double pct, D2D1_COLOR_F track, D2D1_COLOR_F col) {
    ID2D1SolidColorBrush* b = nullptr;
    if (FAILED(rt->CreateSolidColorBrush(track, &b))) return;
    rt->DrawEllipse(D2D1::Ellipse(c, r, r), b, w);
    double f = max(0.0, min(1.0, pct / 100));
    if (f > 0.002) {
        b->SetColor(col);
        ID2D1Factory* fac = nullptr; rt->GetFactory(&fac);
        ID2D1PathGeometry* g = nullptr; ID2D1GeometrySink* k = nullptr;
        if (fac && SUCCEEDED(fac->CreatePathGeometry(&g)) && SUCCEEDED(g->Open(&k))) {
            const float PI = 3.14159265f;
            float a0 = -PI / 2, a1 = a0 + (float)(f >= 0.999 ? 0.9999 : f) * 2 * PI;
            k->BeginFigure(D2D1::Point2F(c.x + r * cosf(a0), c.y + r * sinf(a0)), D2D1_FIGURE_BEGIN_HOLLOW);
            k->AddArc(D2D1::ArcSegment(D2D1::Point2F(c.x + r * cosf(a1), c.y + r * sinf(a1)), D2D1::SizeF(r, r), 0,
                                       D2D1_SWEEP_DIRECTION_CLOCKWISE, f > 0.5 ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
            k->EndFigure(D2D1_FIGURE_END_OPEN);
            k->Close();
            ID2D1StrokeStyle* st = nullptr;
            fac->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND), nullptr, 0, &st);
            rt->DrawGeometry(g, b, w, st);
            if (st) st->Release();
        }
        if (k) k->Release();
        if (g) g->Release();
        if (fac) fac->Release();
    }
    b->Release();
}

static void NotchRender() {
    Notch& n = g_nt;
    if (!n.wnd) return;
    float s = GetDpiForWindow(n.wnd) / 96.0f;
    AiProvider ai[2];
    { std::lock_guard<std::mutex> lk(g_aiMx); ai[0] = g_ai[0]; ai[1] = g_ai[1]; }
    bool vert = NotchVertical();
    const float item = 74 * s, thick = 62 * s, pad = 10 * s, cardW = 262 * s, gap = 10 * s, M = 14 * s;
    float railLen = pad * 2 + item * 2;
    // cartão: altura conforme o conteúdo
    float cardH = 0;
    if (n.open >= 0) {
        const AiProvider& a = ai[n.open];
        cardH = 16 + 24;                                   // título
        if (a.fetchedMs) cardH += (a.w5h.pct >= 0 ? 52 : 0) + (a.week.pct >= 0 ? 52 : 0);
        if (a.state != 1) cardH += 36;                     // aviso
        if (n.open == 0 && (a.state == 2 || a.state == 3)) cardH += 44; // botão do Claude Code
        cardH += 10;
        cardH *= s;
    }
    // janela: trilho + (cartão ao lado, para dentro da tela)
    float railW = vert ? thick : railLen, railH = vert ? railLen : thick;
    float W = railW, H = railH;
    if (n.open >= 0) {
        if (vert) { W = railW + gap + cardW + M; H = max(railH, cardH + 2 * M); }
        else { W = max(railW, cardW + 2 * M); H = railH + gap + cardH + M; }
    }
    UINT Wt = (UINT)W, Ht = (UINT)H;
    fmt::Canvas cv(Wt, Ht);
    if (!cv.rt) return;
    ID2D1RenderTarget* rt = cv.rt;
    rt->Clear(D2D1::ColorF(0, 0));
    rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ui::Theme th = g_theme; // o trilho é sempre escuro, como um notch
    th.dark = true;
    ui::Painter p(rt, g_dw, th, s);
    D2D1_COLOR_F bg = ui::Rgba(0x0E0E10, 0.97f), line = ui::Rgba(0xFFFFFF, 0.10f), text = ui::Rgba(0xFFFFFF, 0.96f), text2 = ui::Rgba(0xFFFFFF, 0.58f), track = ui::Rgba(0xFFFFFF, 0.12f);
    // trilho: grudado na borda (os cantos do lado da borda ficam fora da janela)
    D2D1_RECT_F rail;
    float R = 22 * s;
    if (n.edge == kEdgeRight) rail = D2D1::RectF(W - railW, (H - railH) / 2, W + R, (H + railH) / 2);
    else if (n.edge == kEdgeLeft) rail = D2D1::RectF(-R, (H - railH) / 2, railW, (H + railH) / 2);
    else if (n.edge == kEdgeTop) rail = D2D1::RectF((W - railW) / 2, -R, (W + railW) / 2, railH);
    else rail = D2D1::RectF((W - railW) / 2, H - railH, (W + railW) / 2, H + R);
    p.Fill(rail, R, bg);
    p.Stroke(rail, R, line, 1 * s);
    for (int i = 0; i < 2; ++i) {
        D2D1_RECT_F it;
        if (vert) { float y0 = rail.top + pad + i * item; it = D2D1::RectF(rail.left + (n.edge == kEdgeLeft ? R : 0), y0, rail.right - (n.edge == kEdgeRight ? R : 0), y0 + item); }
        else { float x0 = rail.left + pad + i * item; it = D2D1::RectF(x0, rail.top + (n.edge == kEdgeTop ? R : 0), x0 + item, rail.bottom - (n.edge == kEdgeBottom ? R : 0)); }
        n.rItem[i] = it;
        const AiProvider& a = ai[i];
        bool ok = a.fetchedMs != 0 && a.w5h.pct >= 0;
        if (n.hot == i || n.open == i) p.Fill(D2D1::RectF(it.left + 5 * s, it.top + 3 * s, it.right - 5 * s, it.bottom - 3 * s), 12 * s, ui::Rgba(0xFFFFFF, 0.06f));
        D2D1_POINT_2F c = D2D1::Point2F((it.left + it.right) / 2, it.top + 27 * s);
        p.Fill(D2D1::RectF(c.x - 17 * s, c.y - 17 * s, c.x + 17 * s, c.y + 17 * s), 17 * s, ui::Rgba(0xFFFFFF, 0.07f));
        NotchRing(rt, c, 20 * s, 3 * s, ok ? a.w5h.pct : 0, track, ok ? NotchLevel(a.w5h.pct) : track);
        ClText(p, i == 0 ? L"C" : L"G", D2D1::RectF(c.x - 17 * s, c.y - 11 * s, c.x + 17 * s, c.y + 11 * s), 15, ok ? text : text2, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, L"Space Grotesk");
        wchar_t b[32];
        if (ok) swprintf(b, 32, L"%.0f%%", a.w5h.pct); else wcscpy_s(b, a.state == 0 ? L"…" : L"—");
        ClText(p, b, D2D1::RectF(it.left, c.y + 24 * s, it.right, c.y + 42 * s), 13, ok ? text : text2, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    n.rBtn = n.rCard = D2D1::RectF(0, 0, 0, 0);
    if (n.open >= 0) {
        const AiProvider& a = ai[n.open];
        D2D1_RECT_F it = n.rItem[n.open];
        float cx, cy;
        if (n.edge == kEdgeRight) { cx = rail.left - gap - cardW; cy = (it.top + it.bottom) / 2 - cardH / 2; }
        else if (n.edge == kEdgeLeft) { cx = rail.right + gap; cy = (it.top + it.bottom) / 2 - cardH / 2; }
        else if (n.edge == kEdgeTop) { cx = (it.left + it.right) / 2 - cardW / 2; cy = rail.bottom + gap; }
        else { cx = (it.left + it.right) / 2 - cardW / 2; cy = rail.top - gap - cardH; }
        cx = max(M * 0.5f, min(W - cardW - M * 0.5f, cx)); cy = max(M * 0.5f, min(H - cardH - M * 0.5f, cy));
        D2D1_RECT_F card = D2D1::RectF(cx, cy, cx + cardW, cy + cardH);
        n.rCard = card;
        p.Fill(card, 14 * s, bg);
        p.Stroke(card, 14 * s, line, 1 * s);
        float x = card.left + 16 * s, xr = card.right - 16 * s, y = card.top + 16 * s;
        ClText(p, n.open == 0 ? T(L"Uso do Claude", L"Claude usage", L"Uso de Claude") : T(L"Uso do ChatGPT", L"ChatGPT usage", L"Uso de ChatGPT"),
               D2D1::RectF(x, y, xr, y + 22 * s), 15, text, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, L"Space Grotesk");
        y += 30 * s;
        if (a.fetchedMs) {
            struct Row { const wchar_t* n; const AiWindow* w; } rows[] = {
                {n.open == 0 ? T(L"Sessão atual (5 h)", L"Current session (5h)", L"Sesión actual (5 h)") : T(L"Limite de 5 h", L"5-hour limit", L"Límite de 5 h"), &a.w5h},
                {T(L"Semanal", L"Weekly", L"Semanal"), &a.week}};
            for (auto& r : rows) {
                if (r.w->pct < 0) continue;
                ClText(p, r.n, D2D1::RectF(x, y, xr, y + 16 * s), 12.5f, text, DWRITE_FONT_WEIGHT_SEMI_BOLD);
                ClText(p, AiResetText(r.w->resetMs), D2D1::RectF(x, y, xr, y + 16 * s), 11.5f, text2, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
                D2D1_RECT_F bar = D2D1::RectF(x, y + 22 * s, xr, y + 26 * s);
                p.Fill(bar, 2 * s, track);
                float wv = (float)((bar.right - bar.left) * max(0.0, min(1.0, r.w->pct / 100)));
                if (wv > 0) p.Fill(D2D1::RectF(bar.left, bar.top, bar.left + max(wv, 4 * s), bar.bottom), 2 * s, NotchLevel(r.w->pct));
                wchar_t b[96]; swprintf(b, 96, T(L"%.0f%% usado · %.0f%% restante", L"%.0f%% used · %.0f%% left", L"%.0f%% usado · %.0f%% restante"), r.w->pct, max(0.0, 100 - r.w->pct));
                ClText(p, b, D2D1::RectF(x, y + 30 * s, xr, y + 46 * s), 11.5f, text2);
                y += 52 * s;
            }
        }
        if (a.state != 1) { ClText(p, AiStateText(a.state, n.open), D2D1::RectF(x, y + 4 * s, xr, y + 32 * s), 12, a.fetchedMs ? text2 : text); y += 36 * s; }
        if (n.open == 0 && (a.state == 2 || a.state == 3)) {
            n.rBtn = D2D1::RectF(x, y, xr, y + 34 * s);
            bool inst = ClaudeExe().empty();
            p.Fill(n.rBtn, 9 * s, n.btnHot ? ui::Rgba(0xFFFFFF, 0.92f) : ui::Rgba(0xFFFFFF, 0.85f));
            ClText(p, inst ? T(L"Instalar o Claude Code e entrar", L"Install Claude Code and sign in", L"Instalar Claude Code e iniciar sesión") : T(L"Entrar no Claude Code", L"Sign in to Claude Code", L"Iniciar sesión en Claude Code"),
                   n.rBtn, 12.5f, ui::Rgba(0x0E0E10), DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        }
    }
    HBITMAP bmp = nullptr;
    if (FAILED(cv.Finish(&bmp)) || !bmp) return;
    // posição: encostado na borda da área de trabalho, centrado em `pos`
    RECT wk = NotchWork();
    LONG px, py;
    if (vert) {
        LONG c = max(wk.top + (LONG)(railH / 2), min(wk.bottom - (LONG)(railH / 2), n.pos));
        py = c - (LONG)(H / 2);
        py = max((LONG)wk.top, min(wk.bottom - (LONG)Ht, py));
        px = n.edge == kEdgeRight ? wk.right - (LONG)Wt : wk.left;
    } else {
        LONG c = max(wk.left + (LONG)(railW / 2), min(wk.right - (LONG)(railW / 2), n.pos));
        px = c - (LONG)(W / 2);
        px = max((LONG)wk.left, min(wk.right - (LONG)Wt, px));
        py = n.edge == kEdgeTop ? wk.top : wk.bottom - (LONG)Ht;
    }
    // as coordenadas de clique são da janela: guarda o deslocamento do trilho para o teste de clique
    Widget tmp; tmp.wnd = n.wnd;
    WidgetPresent(tmp, bmp, px, py, Wt, Ht);
}

// ---------- janela ----------

static void NotchSave() {
    WriteDword(L"AiEdge", (DWORD)g_nt.edge); WriteDword(L"AiPos", (DWORD)g_nt.pos);
    WriteDword(L"AiMonX", (DWORD)g_nt.mon.x); WriteDword(L"AiMonY", (DWORD)g_nt.mon.y);
}

// Arrastando: o trilho vai para a borda mais perto do cursor, no monitor do cursor.
static void NotchDragTo(POINT c) {
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromPoint(c, MONITOR_DEFAULTTONEAREST), &mi);
    RECT w = mi.rcWork;
    LONG d[4] = {w.right - c.x, c.x - w.left, c.y - w.top, w.bottom - c.y};
    int e = 0; for (int i = 1; i < 4; ++i) if (d[i] < d[e]) e = i;
    g_nt.edge = e; g_nt.mon = c;
    g_nt.pos = (e == kEdgeRight || e == kEdgeLeft) ? c.y : c.x;
    NotchRender();
}

static LRESULT CALLBACK NotchProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Notch& n = g_nt;
    switch (msg) {
    case WM_TIMER:
        if (wp == 1) {
            AiRefresh(h, false);
            if (n.open >= 0 && n.leaveAt && GetTickCount64() > n.leaveAt) { n.open = n.hot = -1; n.leaveAt = 0; n.btnHot = false; NotchRender(); }
            static int tick = 0;
            if (++tick % 30 == 0) NotchRender(); // "volta em…" muda com o tempo
        }
        return 0;
    case WM_APP + 71: NotchRender(); return 0;
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (n.pressed && !n.dragging && (abs(pt.x - n.down.x) > 5 || abs(pt.y - n.down.y) > 5)) { n.dragging = true; n.open = n.hot = -1; }
        if (n.dragging) { POINT c; GetCursorPos(&c); NotchDragTo(c); return 0; }
        int hot = -1;
        for (int i = 0; i < 2; ++i) if (ui::Hit(n.rItem[i], pt)) hot = i;
        bool overCard = n.open >= 0 && ui::Hit(n.rCard, pt);
        bool btn = overCard && ui::Hit(n.rBtn, pt);
        bool changed = hot != n.hot || btn != n.btnHot;
        n.hot = hot; n.btnHot = btn;
        if (hot >= 0 && hot != n.open) { n.open = hot; changed = true; SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); }
        n.leaveAt = (hot < 0 && !overCard && n.open >= 0) ? GetTickCount64() + 400 : 0; // fora do trilho e do cartão: fecha com um respiro
        if (changed) NotchRender();
        TRACKMOUSEEVENT te = {sizeof(te), TME_LEAVE, h, 0}; TrackMouseEvent(&te);
        return 0;
    }
    case WM_MOUSELEAVE: if (n.open >= 0) n.leaveAt = GetTickCount64() + 400; n.hot = -1; n.btnHot = false; return 0;
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, n.btnHot ? IDC_HAND : n.dragging ? IDC_SIZEALL : IDC_ARROW)); return TRUE;
    case WM_LBUTTONDOWN: n.pressed = true; n.down = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; SetCapture(h); return 0;
    case WM_LBUTTONUP: {
        bool was = n.pressed, drag = n.dragging;
        n.pressed = n.dragging = false; ReleaseCapture();
        if (drag) { NotchSave(); NotchRender(); return 0; }
        if (!was) return 0;
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (n.btnHot && ui::Hit(n.rBtn, pt)) { ClaudeInstallAndLogin(); return 0; }
        for (int i = 0; i < 2; ++i) if (ui::Hit(n.rItem[i], pt)) AiRefresh(h, true); // clique no anel: atualiza agora
        return 0;
    }
    case WM_RBUTTONUP: {
        POINT pt; GetCursorPos(&pt);
        n.open = -1; NotchRender();
        HMENU m = CreatePopupMenu();
        enum { kRefresh = 1, kHide };
        AppendMenuW(m, MF_STRING, kRefresh, T(L"Atualizar agora", L"Refresh now", L"Actualizar ahora"));
        AppendMenuW(m, MF_STRING, kHide, T(L"Ocultar widget", L"Hide widget", L"Ocultar widget"));
        SetForegroundWindow(h);
        int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
        DestroyMenu(m);
        if (cmd == kRefresh) AiRefresh(h, true);
        else if (cmd == kHide) { WriteDword(L"AiWidget", 0); PostMessageW(h, WM_APP + 70, kWAi, 0); }
        return 0;
    }
    case WM_APP + 70: WidgetEnable(kWAi, false); return 0;
    case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: case WM_DPICHANGED: NotchRender(); break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void NotchEnable(bool on) {
    Notch& n = g_nt;
    if (!on) { if (n.wnd) { KillTimer(n.wnd, 1); DestroyWindow(n.wnd); n.wnd = nullptr; } return; }
    if (n.wnd) { NotchRender(); return; }
    static bool reg = false;
    if (!reg) { WNDCLASSW wc = {}; wc.lpfnWndProc = NotchProc; wc.hInstance = g_inst; wc.lpszClassName = L"VizuNotch"; RegisterClassW(&wc); reg = true; }
    n = Notch();
    n.edge = (int)min(3ul, (unsigned long)ReadDword(L"AiEdge", kEdgeRight));
    n.mon = {(LONG)ReadDword(L"AiMonX", 0), (LONG)ReadDword(L"AiMonY", 0)};
    if (!MonitorFromPoint(n.mon, MONITOR_DEFAULTTONULL)) n.mon = {0, 0};
    RECT wk = NotchWork();
    DWORD pos = ReadDword(L"AiPos", 0x80000000);
    n.pos = pos == 0x80000000 ? (NotchVertical() ? (wk.top + wk.bottom) / 2 : (wk.left + wk.right) / 2) : (LONG)pos;
    n.wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE, L"VizuNotch", L"Vizu", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, g_inst, nullptr);
    AiRefresh(n.wnd, false);
    NotchRender();
    ShowWindow(n.wnd, SW_SHOWNOACTIVATE);
    SetTimer(n.wnd, 1, 1000, nullptr);
}

// Muda a borda pela Configuração (o arrastar faz o mesmo).
static void NotchSetEdge(int e) {
    WriteDword(L"AiEdge", (DWORD)e); WriteDword(L"AiPos", 0x80000000);
    if (g_nt.wnd) { NotchEnable(false); NotchEnable(true); }
}
