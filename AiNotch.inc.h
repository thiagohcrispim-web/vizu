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
    float ex = 1, ex0 = 1, exTo = 1;   // "ao passar o cursor": 0 recolhido na faixa, 1 aberto (animado)
    ULONGLONG exT0 = 0;
    bool fullHidden = false;           // escondido por um app em tela cheia
    AiWindow prev[2][2];               // leitura anterior (5 h, semanal) para o aviso de limite renovado
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


// ---------- logos ----------
// Logos de uma cor, em path SVG preenchido, sem mudar forma nem proporção.
// Claude: símbolo oficial da Anthropic (anthropic.com, Press/Brand), viewBox 256x257, cor #D97757.
static const char kLogoClaude[] = "m50.228 170.321 50.357 -28.257 .843 -2.463 -.843 -1.361h-2.462l-8.426 -.518 -28.775 -.778 -24.952 -1.037 -24.175 -1.296 -6.092 -1.297L0 125.796l.583 -3.759 5.12 -3.434 7.324 .648 16.202 1.101 24.304 1.685 17.629 1.037 26.118 2.722h4.148l.583 -1.685 -1.426 -1.037 -1.101 -1.037 -25.147 -17.045 -27.22 -18.017 -14.258 -10.37 -7.713 -5.25 -3.888 -4.925 -1.685 -10.758 7 -7.713 9.397 .649 2.398 .648 9.527 7.323 20.35 15.75L94.817 91.9l3.889 3.24 1.555 -1.102 .195 -.777 -1.75 -2.917 -14.453 -26.118 -15.425 -26.572 -6.87 -11.018 -1.814 -6.61c-.648 -2.723 -1.102 -4.991 -1.102 -7.778l7.972 -10.823L71.42 0 82.05 1.426l4.472 3.888 6.61 15.101 10.694 23.786 16.591 32.34 4.861 9.592 2.592 8.879 .973 2.722h1.685v-1.556l1.36 -18.211 2.528 -22.36 2.463 -28.776 .843 -8.1 4.018 -9.722 7.971 -5.25 6.222 2.981 5.12 7.324 -.713 4.73 -3.046 19.768 -5.962 30.98 -3.889 20.739h2.268l2.593 -2.593 10.499 -13.934 17.628 -22.036 7.778 -8.749 9.073 -9.657 5.833 -4.601h11.018l8.1 12.055 -3.628 12.443 -11.342 14.388 -9.398 12.184 -13.48 18.147 -8.426 14.518 .778 1.166 2.01 -.194 30.46 -6.481 16.462 -2.982 19.637 -3.37 8.88 4.148 .971 4.213 -3.5 8.62 -20.998 5.184 -24.628 4.926 -36.682 8.685 -.454 .324 .519 .648 16.526 1.555 7.065 .389h17.304l32.21 2.398 8.426 5.574 5.055 6.805 -.843 5.184 -12.962 6.611 -17.498 -4.148 -40.83 -9.721 -14 -3.5h-1.944v1.167l11.666 11.406 21.387 19.314 26.767 24.887 1.36 6.157 -3.434 4.86 -3.63 -.518 -23.526 -17.693 -9.073 -7.972 -20.545 -17.304h-1.36v1.814l4.73 6.935 25.017 37.59 1.296 11.536 -1.814 3.76 -6.481 2.268 -7.13 -1.297 -14.647 -20.544 -15.1 -23.138 -12.185 -20.739 -1.49 .843 -7.194 77.448 -3.37 3.953 -7.778 2.981 -6.48 -4.925 -3.436 -7.972 3.435 -15.749 4.148 -20.544 3.37 -16.333 3.046 -20.285 1.815 -6.74 -.13 -.454 -1.49 .194 -15.295 20.999 -23.267 31.433 -18.406 19.702 -4.407 1.75 -7.648 -3.954 .713 -7.064 4.277 -6.286 25.47 -32.405 15.36 -20.092 9.917 -11.6 -.065 -1.686h-.583L44.07 198.125l-12.055 1.555 -5.185 -4.86 .648 -7.972 2.463 -2.593 20.35 -13.999 -.064 .065Z";
// ChatGPT: marca oficial da OpenAI (openai.com/brand), viewBox 24x24, monocromática, regra par-ímpar.
static const char kLogoOpenAI[] = "M9.205 8.658v-2.26c0 -.19 .072 -.333 .238 -.428l4.543 -2.616c.619 -.357 1.356 -.523 2.117 -.523 2.854 0 4.662 2.212 4.662 4.566 0 .167 0 .357 -.024 .547l-4.71 -2.759a.797 .797 0 0 0 -.856 0l-5.97 3.473zm10.609 8.8V12.06c0 -.333 -.143 -.57 -.429 -.737l-5.97 -3.473 1.95 -1.118a.433 .433 0 0 1 .476 0l4.543 2.617c1.309 .76 2.189 2.378 2.189 3.948 0 1.808 -1.07 3.473 -2.76 4.163zM7.802 12.703l-1.95 -1.142c-.167 -.095 -.239 -.238 -.239 -.428V5.899c0 -2.545 1.95 -4.472 4.591 -4.472 1 0 1.927 .333 2.712 .928L8.23 5.067c-.285 .166 -.428 .404 -.428 .737v6.898zM12 15.128l-2.795 -1.57v-3.33L12 8.658l2.795 1.57v3.33L12 15.128zm1.796 7.23c-1 0 -1.927 -.332 -2.712 -.927l4.686 -2.712c.285 -.166 .428 -.404 .428 -.737v-6.898l1.974 1.142c.167 .095 .238 .238 .238 .428v5.233c0 2.545 -1.974 4.472 -4.614 4.472zm-5.637 -5.303l-4.544 -2.617c-1.308 -.761 -2.188 -2.378 -2.188 -3.948A4.482 4.482 0 0 1 4.21 6.327v5.423c0 .333 .143 .571 .428 .738l5.947 3.449 -1.95 1.118a.432 .432 0 0 1 -.476 0zm-.262 3.9c-2.688 0 -4.662 -2.021 -4.662 -4.519 0 -.19 .024 -.38 .047 -.57l4.686 2.71c.286 .167 .571 .167 .856 0l5.97 -3.448v2.26c0 .19 -.07 .333 -.237 .428l-4.543 2.616c-.619 .357 -1.356 .523 -2.117 .523zm5.899 2.83a5.947 5.947 0 0 0 5.827 -4.756C22.287 18.339 24 15.84 24 13.296c0 -1.665 -.713 -3.282 -1.998 -4.448 .119 -.5 .19 -.999 .19 -1.498 0 -3.401 -2.759 -5.947 -5.946 -5.947 -.642 0 -1.26 .095 -1.88 .31A5.962 5.962 0 0 0 10.205 0a5.947 5.947 0 0 0 -5.827 4.757C1.713 5.447 0 7.945 0 10.49c0 1.666 .713 3.283 1.998 4.448 -.119 .5 -.19 1 -.19 1.499 0 3.401 2.759 5.946 5.946 5.946 .642 0 1.26 -.095 1.88 -.309a5.96 5.96 0 0 0 4.162 1.713z";

// Desenha a logo de `which` (0 Claude, 1 ChatGPT) centrada em `c`, com `size` px no lado maior.
static void NotchLogo(ID2D1RenderTarget* rt, int which, D2D1_POINT_2F c, float size, D2D1_COLOR_F col) {
    float vw = which == 0 ? 256.f : 24.f, vh = which == 0 ? 257.f : 24.f, sc = size / max(vw, vh);
    ID2D1Factory* f = nullptr; rt->GetFactory(&f);
    if (!f) return;
    ID2D1PathGeometry* g = ui::SvgPath(f, which == 0 ? kLogoClaude : kLogoOpenAI, sc, c.x - vw * sc / 2, c.y - vh * sc / 2, true);
    ID2D1SolidColorBrush* b = nullptr;
    if (g && SUCCEEDED(rt->CreateSolidColorBrush(col, &b))) { rt->FillGeometry(g, b); b->Release(); }
    if (g) g->Release();
    f->Release();
}

// ---------- opções (Configuração › Monitor) ----------

static int NotchWarn() { return (int)max(5ul, min(90ul, (unsigned long)ReadDword(L"AiWarn", 75))); }
static int NotchCrit() { int w = NotchWarn(); return (int)max((unsigned long)(w + 5), min(100ul, (unsigned long)ReadDword(L"AiCrit", 90))); }
static int NotchShowMode() { return (int)min(2ul, (unsigned long)ReadDword(L"AiShow", 0)); }  // 0 sempre, 1 ao passar o cursor, 2 oculto
static int NotchWeekMode() { return (int)min(2ul, (unsigned long)ReadDword(L"AiWeekRing", 0)); } // 0 não, 1 dentro, 2 fora
static float NotchSizeMul() { DWORD v = ReadDword(L"AiSize", 1); return v == 0 ? 0.85f : v == 2 ? 1.2f : 1.0f; }

// ---------- desenho ----------

static D2D1_COLOR_F NotchLevel(double pct) { return pct >= NotchCrit() ? ui::Rgba(0xFF4D3D) : pct >= NotchWarn() ? ui::Rgba(0xFFB020) : ui::Rgba(0x2BD97C); }

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
    const float sr = s * NotchSizeMul(); // o trilho muda de tamanho; o cartão fica no normal para o texto ser legível
    const int wkMode = NotchWeekMode();
    const bool showLeft = ReadDword(L"AiShowLeft", 0) != 0;
    const float ch = (wkMode == 2 ? 86 : 74) * sr; // altura do conteúdo de um item (anel + número)
    const float item = ch, thickFull = vert ? (wkMode == 2 ? 66 : 62) * sr : ch - 4 * sr, pad = 10 * sr, cardW = 262 * s, gap = 10 * s, M = 14 * s;
    const float strip = 6 * s;
    const bool collapsed = n.ex < 0.999f;
    if (collapsed) n.open = -1;
    const float thick = strip + (thickFull - strip) * n.ex;
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
    ui::Painter p(rt, g_dw, th, s), pr(rt, g_dw, th, sr); // pr: trilho (com o tamanho escolhido); p: cartão
    D2D1_COLOR_F bg = ui::Rgba(0x0E0E10, 0.97f), line = ui::Rgba(0xFFFFFF, 0.10f), text = ui::Rgba(0xFFFFFF, 0.96f), text2 = ui::Rgba(0xFFFFFF, 0.58f), track = ui::Rgba(0xFFFFFF, 0.12f);
    // trilho: grudado na borda (os cantos do lado da borda ficam fora da janela)
    D2D1_RECT_F rail;
    float R = min(22 * sr, thick + 22 * sr * n.ex);
    if (n.edge == kEdgeRight) rail = D2D1::RectF(W - railW, (H - railH) / 2, W + R, (H + railH) / 2);
    else if (n.edge == kEdgeLeft) rail = D2D1::RectF(-R, (H - railH) / 2, railW, (H + railH) / 2);
    else if (n.edge == kEdgeTop) rail = D2D1::RectF((W - railW) / 2, -R, (W + railW) / 2, railH);
    else rail = D2D1::RectF((W - railW) / 2, H - railH, (W + railW) / 2, H + R);
    p.Fill(rail, R, bg); // alfa alto também na faixa recolhida: a janela em camadas só recebe o mouse onde há pixel
    p.Stroke(rail, R, line, 1 * s);
    n.rItem[0] = n.rItem[1] = D2D1::RectF(0, 0, 0, 0);
    if (collapsed) { if (n.ex <= 0.001f) n.rItem[0] = rail; } // recolhido: a faixa inteira conta como item para abrir
    else for (int i = 0; i < 2; ++i) {
        D2D1_RECT_F it;
        if (vert) { float y0 = rail.top + pad + i * item; it = D2D1::RectF(rail.left + (n.edge == kEdgeLeft ? R : 0), y0, rail.right - (n.edge == kEdgeRight ? R : 0), y0 + item); }
        else { float x0 = rail.left + pad + i * item; it = D2D1::RectF(x0, rail.top + (n.edge == kEdgeTop ? R : 0), x0 + item, rail.bottom - (n.edge == kEdgeBottom ? R : 0)); }
        n.rItem[i] = it;
        const AiProvider& a = ai[i];
        bool ok = a.fetchedMs != 0 && a.w5h.pct >= 0;
        bool okW = a.fetchedMs != 0 && a.week.pct >= 0;
        if (n.hot == i || n.open == i) p.Fill(D2D1::RectF(it.left + 5 * s, it.top + 3 * s, it.right - 5 * s, it.bottom - 3 * s), 12 * s, ui::Rgba(0xFFFFFF, 0.06f));
        float top = vert ? it.top : (it.top + it.bottom) / 2 - ch / 2;
        D2D1_POINT_2F c = D2D1::Point2F((it.left + it.right) / 2, top + (wkMode == 2 ? 32 : 27) * sr);
        float inner = wkMode == 1 ? 12 : 17; // anel semanal por dentro: o fundo e a logo diminuem
        p.Fill(D2D1::RectF(c.x - inner * sr, c.y - inner * sr, c.x + inner * sr, c.y + inner * sr), inner * sr, ui::Rgba(0xFFFFFF, 0.07f));
        NotchRing(rt, c, 20 * sr, 3 * sr, ok ? a.w5h.pct : 0, track, ok ? NotchLevel(a.w5h.pct) : track);
        if (wkMode) {
            D2D1_COLOR_F wc = okW ? NotchLevel(a.week.pct) : track; wc.a *= 0.7f;
            NotchRing(rt, c, (wkMode == 1 ? 15 : 25) * sr, 2 * sr, okW ? a.week.pct : 0, ui::Rgba(0xFFFFFF, 0.08f), wc);
        }
        // Claude na cor da marca; ChatGPT monocromático (branco). Sem leitura: cinza.
        NotchLogo(rt, i, c, (wkMode == 1 ? 13 : 18) * sr, !ok ? text2 : i == 0 ? ui::Rgba(0xD97757) : text);
        wchar_t b[32];
        if (ok) swprintf(b, 32, L"%.0f%%", showLeft ? max(0.0, 100 - a.w5h.pct) : a.w5h.pct); else wcscpy_s(b, a.state == 0 ? L"…" : L"—");
        float ty = c.y + (wkMode == 2 ? 29 : 24) * sr;
        ClText(pr, b, D2D1::RectF(it.left, ty, it.right, ty + 18 * sr), 13, ok ? text : text2, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
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
        NotchLogo(rt, n.open, D2D1::Point2F(x + 8 * s, y + 11 * s), 16 * s, n.open == 0 ? ui::Rgba(0xD97757) : text);
        ClText(p, n.open == 0 ? T(L"Uso do Claude", L"Claude usage", L"Uso de Claude") : T(L"Uso do ChatGPT", L"ChatGPT usage", L"Uso de ChatGPT"),
               D2D1::RectF(x + 24 * s, y, xr, y + 22 * s), 15, text, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, L"Space Grotesk");
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

// ---------- aviso de limite renovado ----------

struct NotchToast { HWND wnd = nullptr; int which = 0, kind = 0; };
static NotchToast g_ntt;

static void NotchToastRender() {
    NotchToast& t = g_ntt;
    if (!t.wnd) return;
    float s = GetDpiForWindow(t.wnd) / 96.0f;
    UINT Wt = (UINT)(300 * s), Ht = (UINT)(66 * s);
    fmt::Canvas cv(Wt, Ht);
    if (!cv.rt) return;
    ID2D1RenderTarget* rt = cv.rt;
    rt->Clear(D2D1::ColorF(0, 0));
    rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ui::Theme th = g_theme; th.dark = true;
    ui::Painter p(rt, g_dw, th, s);
    D2D1_RECT_F card = D2D1::RectF(0.5f, 0.5f, Wt - 0.5f, Ht - 0.5f);
    p.Fill(card, 14 * s, ui::Rgba(0x0E0E10, 0.97f));
    p.Stroke(card, 14 * s, ui::Rgba(0xFFFFFF, 0.10f), 1 * s);
    NotchLogo(rt, t.which, D2D1::Point2F(30 * s, Ht / 2.0f), 24 * s, t.which == 0 ? ui::Rgba(0xD97757) : ui::Rgba(0xFFFFFF));
    const wchar_t* title = t.which == 0
        ? (t.kind == 0 ? T(L"Limite de 5 h do Claude renovado", L"Claude 5-hour limit renewed", L"Límite de 5 h de Claude renovado")
                       : T(L"Limite semanal do Claude renovado", L"Claude weekly limit renewed", L"Límite semanal de Claude renovado"))
        : (t.kind == 0 ? T(L"Limite de 5 h do ChatGPT renovado", L"ChatGPT 5-hour limit renewed", L"Límite de 5 h de ChatGPT renovado")
                       : T(L"Limite semanal do ChatGPT renovado", L"ChatGPT weekly limit renewed", L"Límite semanal de ChatGPT renovado"));
    ClText(p, title, D2D1::RectF(56 * s, 13 * s, Wt - 14 * s, 33 * s), 13, ui::Rgba(0xFFFFFF, 0.96f), DWRITE_FONT_WEIGHT_BOLD);
    ClText(p, T(L"Você está com 100% de novo", L"You're back to 100%", L"Vuelves a tener el 100%"), D2D1::RectF(56 * s, 34 * s, Wt - 14 * s, 52 * s), 11.5f, ui::Rgba(0xFFFFFF, 0.58f));
    HBITMAP bmp = nullptr;
    if (FAILED(cv.Finish(&bmp)) || !bmp) return;
    // perto do trilho, para dentro da tela
    RECT wk = NotchWork();
    LONG off = (LONG)(84 * s), px, py;
    bool vert = NotchVertical();
    LONG c = g_nt.pos;
    if (vert) { py = max(wk.top, min(wk.bottom - (LONG)Ht, c - (LONG)Ht / 2)); px = g_nt.edge == kEdgeRight ? wk.right - off - (LONG)Wt : wk.left + off; }
    else { px = max(wk.left, min(wk.right - (LONG)Wt, c - (LONG)Wt / 2)); py = g_nt.edge == kEdgeTop ? wk.top + off : wk.bottom - off - (LONG)Ht; }
    Widget tmp; tmp.wnd = t.wnd;
    WidgetPresent(tmp, bmp, px, py, Wt, Ht);
}

static LRESULT CALLBACK NotchToastProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TIMER: case WM_LBUTTONUP: DestroyWindow(h); return 0; // fecha sozinho em 8 s ou com um clique
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE;
    case WM_DESTROY: if (g_ntt.wnd == h) g_ntt.wnd = nullptr; return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static bool NotchFullscreen();
// which: 0 Claude, 1 ChatGPT; kind: 0 limite de 5 h, 1 semanal. `preview`: botão "Ver prévia" (sempre mostra).
static void NotchToastShow(int which, int kind, bool preview) {
    if (!preview && NotchFullscreen()) return;
    static bool reg = false;
    if (!reg) { WNDCLASSW wc = {}; wc.lpfnWndProc = NotchToastProc; wc.hInstance = g_inst; wc.lpszClassName = L"VizuNotchToast"; RegisterClassW(&wc); reg = true; }
    if (g_ntt.wnd) DestroyWindow(g_ntt.wnd);
    g_ntt.which = which; g_ntt.kind = kind;
    g_ntt.wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE, L"VizuNotchToast", L"Vizu", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, g_inst, nullptr);
    if (!g_ntt.wnd) return;
    if (!g_nt.wnd) { // sem o trilho: usa a posição salva
        g_nt.edge = (int)min(3ul, (unsigned long)ReadDword(L"AiEdge", kEdgeRight));
        RECT wk = NotchWork(); DWORD pos = ReadDword(L"AiPos", 0x80000000);
        g_nt.pos = pos == 0x80000000 ? (NotchVertical() ? (wk.top + wk.bottom) / 2 : (wk.left + wk.right) / 2) : (LONG)pos;
    }
    NotchToastRender();
    ShowWindow(g_ntt.wnd, SW_SHOWNOACTIVATE);
    SetTimer(g_ntt.wnd, 1, 8000, nullptr);
    if (ReadDword(L"AiResetSound", 1)) PlaySoundW(L"SystemNotification", nullptr, SND_ALIAS | SND_ASYNC);
}

// Leitura nova: avisa quando uma janela que tinha passado de 10% renovou (data de renovação maior e uso abaixo de 5%).
static void NotchCheckReset() {
    AiProvider ai[2];
    { std::lock_guard<std::mutex> lk(g_aiMx); ai[0] = g_ai[0]; ai[1] = g_ai[1]; }
    bool on = ReadDword(L"AiResetNotify", 1) != 0;
    for (int i = 0; i < 2; ++i) {
        if (!ai[i].fetchedMs) continue;
        const AiWindow* cur[2] = {&ai[i].w5h, &ai[i].week};
        for (int k = 0; k < 2; ++k) {
            AiWindow& pv = g_nt.prev[i][k];
            const AiWindow& cw = *cur[k];
            if (cw.pct < 0) continue;
            if (on && pv.pct >= 10 && cw.pct < 5 && cw.resetMs > pv.resetMs) NotchToastShow(i, k, false);
            pv = cw;
        }
    }
}

// ---------- janela ----------

// O app em primeiro plano cobre o monitor inteiro do trilho (vídeo, jogo, apresentação)? A área de trabalho e o shell não contam.
static bool NotchFullscreen() {
    HWND fg = GetForegroundWindow();
    if (!fg || !IsWindowVisible(fg) || IsIconic(fg)) return false;
    DWORD pid = 0; GetWindowThreadProcessId(fg, &pid);
    if (pid == GetCurrentProcessId()) return false;
    wchar_t cls[64] = {}; GetClassNameW(fg, cls, 64);
    for (const wchar_t* c : {L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"Windows.UI.Core.CoreWindow", L"XamlExplorerHostIslandWindow"})
        if (!wcscmp(cls, c)) return false;
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromPoint(g_nt.mon, MONITOR_DEFAULTTOPRIMARY), &mi);
    if (MonitorFromWindow(fg, MONITOR_DEFAULTTONULL) != MonitorFromPoint(g_nt.mon, MONITOR_DEFAULTTOPRIMARY)) return false;
    RECT r; if (!GetWindowRect(fg, &r)) return false;
    return r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top && r.right >= mi.rcMonitor.right && r.bottom >= mi.rcMonitor.bottom;
}

// Mostra ou esconde a janela conforme "Exibição" e a tela cheia (a leitura continua rodando nos dois casos).
static void NotchUpdateVisible() {
    Notch& n = g_nt;
    if (!n.wnd) return;
    n.fullHidden = ReadDword(L"AiHideFull", 1) && NotchFullscreen();
    bool show = NotchShowMode() != 2 && !n.fullHidden;
    if (show != (IsWindowVisible(n.wnd) != 0)) {
        if (show) { NotchRender(); ShowWindow(n.wnd, SW_SHOWNOACTIVATE); SetWindowPos(n.wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); }
        else { n.open = n.hot = -1; ShowWindow(n.wnd, SW_HIDE); }
    }
}

// Abre (1) ou recolhe (0) o trilho no modo "ao passar o cursor", com uma animação curta.
static void NotchExpand(float to) {
    Notch& n = g_nt;
    if (n.exTo == to && (n.ex == to || n.exT0)) return;
    n.ex0 = n.ex; n.exTo = to; n.exT0 = GetTickCount64();
    if (!n.wnd || !SetTimer(n.wnd, 2, 15, nullptr)) { n.ex = to; n.exT0 = 0; NotchRender(); } // sem timer: abre direto
}

// Aplica as opções da Configuração na hora.
static void NotchApply() {
    Notch& n = g_nt;
    if (!n.wnd) return;
    KillTimer(n.wnd, 3);
    if (NotchShowMode() == 1) { if (n.exTo != 0) { n.ex = n.ex0 = n.exTo = 0; n.exT0 = 0; } }
    else { n.ex = n.ex0 = n.exTo = 1; n.exT0 = 0; }
    NotchRender();
    NotchUpdateVisible();
}

static void NotchSave() {
    WriteDword(L"AiEdge", (DWORD)g_nt.edge); WriteDword(L"AiPos", (DWORD)g_nt.pos);
    WriteDword(L"AiMonX", (DWORD)g_nt.mon.x); WriteDword(L"AiMonY", (DWORD)g_nt.mon.y);
}

static void NotchEnable(bool on);
// Volta o trilho para o meio da borda atual; se o monitor salvo não existe mais, vai para a borda direita do principal.
static void NotchRecenter() {
    POINT m = {(LONG)ReadDword(L"AiMonX", 0), (LONG)ReadDword(L"AiMonY", 0)};
    if (!MonitorFromPoint(m, MONITOR_DEFAULTTONULL)) { WriteDword(L"AiEdge", kEdgeRight); WriteDword(L"AiMonX", 0); WriteDword(L"AiMonY", 0); }
    WriteDword(L"AiPos", 0x80000000);
    if (g_nt.wnd) { NotchEnable(false); NotchEnable(true); }
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
            NotchUpdateVisible();
            if (n.open >= 0 && n.leaveAt && GetTickCount64() > n.leaveAt) { n.open = n.hot = -1; n.leaveAt = 0; n.btnHot = false; NotchRender(); }
            static int tick = 0;
            if (++tick % 30 == 0) NotchRender(); // "volta em…" muda com o tempo
        } else if (wp == 2) { // animação de abrir/recolher (~150 ms)
            float k = min(1.0f, (GetTickCount64() - n.exT0) / 150.0f);
            k = 1 - (1 - k) * (1 - k);
            n.ex = n.ex0 + (n.exTo - n.ex0) * k;
            if (k >= 1) { n.ex = n.exTo; n.exT0 = 0; KillTimer(h, 2); }
            NotchRender();
        } else if (wp == 3) { // 800 ms fora do trilho e do cartão: recolhe
            KillTimer(h, 3);
            POINT c; GetCursorPos(&c);
            if (WindowFromPoint(c) != h && !n.pressed && NotchShowMode() == 1) { n.open = n.hot = -1; n.btnHot = false; NotchExpand(0); }
        }
        return 0;
    case WM_APP + 71: NotchCheckReset(); NotchRender(); return 0;
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        TRACKMOUSEEVENT te = {sizeof(te), TME_LEAVE, h, 0}; TrackMouseEvent(&te);
        KillTimer(h, 3);
        if (NotchShowMode() == 1 && n.exTo < 1 && !n.pressed) { NotchExpand(1); return 0; } // chegou na faixa: abre
        if (n.ex < 0.999f && !n.dragging) return 0;
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
        return 0;
    }
    case WM_MOUSELEAVE:
        if (n.open >= 0) n.leaveAt = GetTickCount64() + 400;
        n.hot = -1; n.btnHot = false;
        if (NotchShowMode() == 1) SetTimer(h, 3, 800, nullptr);
        return 0;
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
        enum { kRefresh = 1, kRecenter, kHide };
        AppendMenuW(m, MF_STRING, kRefresh, T(L"Atualizar agora", L"Refresh now", L"Actualizar ahora"));
        AppendMenuW(m, MF_STRING, kRecenter, T(L"Recentralizar", L"Recenter", L"Recentrar"));
        AppendMenuW(m, MF_STRING, kHide, T(L"Ocultar widget", L"Hide widget", L"Ocultar widget"));
        SetForegroundWindow(h);
        int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
        DestroyMenu(m);
        if (cmd == kRefresh) AiRefresh(h, true);
        else if (cmd == kRecenter) PostMessageW(h, WM_APP + 72, 0, 0);
        else if (cmd == kHide) { WriteDword(L"AiWidget", 0); PostMessageW(h, WM_APP + 70, kWAi, 0); }
        return 0;
    }
    case WM_APP + 70: WidgetEnable(kWAi, false); return 0;
    case WM_APP + 72: NotchRecenter(); return 0; // fora do WndProc da própria janela, que é recriada
    case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: case WM_DPICHANGED: NotchRender(); break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void NotchEnable(bool on) {
    Notch& n = g_nt;
    if (!on) { if (n.wnd) { KillTimer(n.wnd, 1); KillTimer(n.wnd, 2); KillTimer(n.wnd, 3); DestroyWindow(n.wnd); n.wnd = nullptr; } return; }
    if (n.wnd) { NotchApply(); return; }
    static bool reg = false;
    if (!reg) { WNDCLASSW wc = {}; wc.lpfnWndProc = NotchProc; wc.hInstance = g_inst; wc.lpszClassName = L"VizuNotch"; RegisterClassW(&wc); reg = true; }
    AiWindow prev[2][2]; memcpy(prev, n.prev, sizeof(prev)); // a leitura anterior sobrevive a recriar a janela
    n = Notch();
    memcpy(n.prev, prev, sizeof(prev));
    n.edge = (int)min(3ul, (unsigned long)ReadDword(L"AiEdge", kEdgeRight));
    n.mon = {(LONG)ReadDword(L"AiMonX", 0), (LONG)ReadDword(L"AiMonY", 0)};
    if (!MonitorFromPoint(n.mon, MONITOR_DEFAULTTONULL)) n.mon = {0, 0};
    RECT wk = NotchWork();
    DWORD pos = ReadDword(L"AiPos", 0x80000000);
    n.pos = pos == 0x80000000 ? (NotchVertical() ? (wk.top + wk.bottom) / 2 : (wk.left + wk.right) / 2) : (LONG)pos;
    if (NotchShowMode() == 1) n.ex = n.ex0 = n.exTo = 0;
    n.wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE, L"VizuNotch", L"Vizu", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, g_inst, nullptr);
    AiRefresh(n.wnd, false);
    NotchRender();
    n.fullHidden = ReadDword(L"AiHideFull", 1) && NotchFullscreen();
    if (NotchShowMode() != 2 && !n.fullHidden) ShowWindow(n.wnd, SW_SHOWNOACTIVATE);
    SetTimer(n.wnd, 1, 1000, nullptr);
}

// Muda a borda pela Configuração (o arrastar faz o mesmo).
static void NotchSetEdge(int e) {
    WriteDword(L"AiEdge", (DWORD)e); WriteDword(L"AiPos", 0x80000000);
    if (g_nt.wnd) { NotchEnable(false); NotchEnable(true); }
}
