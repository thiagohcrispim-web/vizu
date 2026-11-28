// Tutorial.inc.h - tutorial de boas-vindas (v19.8): 8 telas sobre a tela inteira, sempre no tema Vizu Classic.
// Fundo preto a 85%, setas dos lados, pontos embaixo e "Pular tutorial ✕". Pular ou concluir grava kTutorialRev:
// o tutorial só volta sozinho quando kTutorialRev subir (função nova). Configurações › "Clique para ver o tutorial" abre de novo.
// Incluído no Vizu.cpp depois do WallCal.inc.h (usa WallApply, OpenSettings, SaveConfig).

static const DWORD kTutorialRev = 1; // suba quando houver função nova para mostrar
struct TutUi { HWND wnd = nullptr; int step = 0, hot = 0; std::vector<std::pair<int, D2D1_RECT_F>> hits; };
static TutUi g_tut;
static const int kTutSteps = 8;

// Texto com destaque: *assim* sai em negrito laranja, #assim# em negrito branco. Espaço fixo ( ) evita palavra sozinha na linha.
struct TutSlide { const wchar_t *k, *t, *s[3], *tip; };
static const TutSlide* TutSlides() {
    static const TutSlide pt[kTutSteps] = {
        {L"VER", L"Espaço pra ver", {L"Selecione uma imagem, PSD ou PDF no *Explorer*.", L"Aperte a *barra de ESPAÇO*: a prévia abre na hora.", L"Use as *setas* para trocar de arquivo. *Espaço* de novo fecha."},
         L"Passe o mouse na imagem para ver o #HEX# de qualquer cor. Clique para copiar."},
        {L"CONVERTER", L"Alt pra resolver", {L"Selecione *um ou mais arquivos*.", L"Segure *ALT*: o menu circular aparece em volta do mouse.", L"Escolha o *formato*. O arquivo novo fica *ao lado do original*."},
         L"Gire a #roda do mouse# para trocar de lado: Ferramentas, PDF e Extras."},
        {L"TEMPO", L"Seu tempo à vista", {L"O *Mini-relógio* fica na área de trabalho ou na barra de tarefas.", L"O *triângulo* abre a Agenda e avisa 10 e 5 minutos antes.", L"Os botões ligam o *Foco*, o *Roubo de tempo*, a Limpeza e o OCR."},
         L"Alguém parou na sua mesa? Clique na #caveira# e veja no fim do dia quanto tempo perdeu."},
        {L"LER TEXTO", L"Copie qualquer texto", {L"Aperte *Ctrl + Alt + T* em qualquer lugar.", L"Arraste sobre a imagem, o PDF, o vídeo ou o site.", L"O texto vai *direto para a área de transferência*."},
         L"Traduza com um clique. O botão #A/a# troca tudo para CAIXA ALTA."},
        {L"GRAVAR TELA", L"Grave a sua tela", {L"Clique na *bolinha vermelha* do relógio ou aperte *Ctrl + Alt + R*.", L"A barra aparece no topo. Clique em *Gravar*, pause quando quiser.", L"Em *Parar*, o vídeo MP4 vai para a pasta *Vídeos*."},
         L"A barra nunca aparece no vídeo. Marque #Esconder# para tirar também o cursor e o menu Iniciar."},
        {L"LIMPEZA", L"Espaço de volta", {L"Clique com o botão direito no *fundo de uma pasta* ou use a bandeja.", L"O Vizu acha *temporários e caches* do Windows, navegadores e Adobe.", L"Marque o que quer e clique em *Limpar*."},
         L"Arquivos de trabalho #(PSD, AI, INDD, PDF)# e o que está em uso nunca saem."},
        {L"MONITOR", L"De olho no PC", {L"O widget de *desempenho* avisa quando o PC fica lento e mostra o que está pesando.", L"O widget de *uso de IA* mostra os limites do Claude e do ChatGPT na borda da tela.", L"Ligue os dois em *Configurações › Monitor*."},
         L"O anel fica amarelo em 75% e vermelho em 90%."},
        {L"EXPERIMENTE", L"WallEngine", {L"Um *relógio gigante* acima do seu papel de parede.", L"A *chuva digital* passando atrás de tudo.", L"Um *calendário de vidro* no canto, pronto para a Agenda."},
         L"Use os três juntos e ajuste cada um em #Configurações › WallEngine#."},
    };
    static const TutSlide en[kTutSteps] = {
        {L"VIEW", L"Space to see", {L"Select an image, PSD or PDF in *Explorer*.", L"Press the *SPACE BAR*: the preview opens instantly.", L"Use the *arrows* to switch files. *Space* again closes."},
         L"Hover the image to see the #HEX# of any color. Click to copy."},
        {L"CONVERT", L"Alt to get it done", {L"Select *one or more files*.", L"Hold *ALT*: the radial menu appears around the mouse.", L"Pick the *format*. The new file lands *next to the original*."},
         L"Turn the #mouse wheel# to switch sides: Tools, PDF and Extras."},
        {L"TIME", L"Your time in sight", {L"The *Mini clock* sits on the desktop or the taskbar.", L"The *triangle* opens the Agenda and warns 10 and 5 minutes before.", L"The buttons turn on *Focus*, *Time thief*, Cleanup and OCR."},
         L"Someone stopped by your desk? Click the #skull# and see at the end of the day how much time you lost."},
        {L"READ TEXT", L"Copy any text", {L"Press *Ctrl + Alt + T* anywhere.", L"Drag over the image, PDF, video or site.", L"The text goes *straight to the clipboard*."},
         L"Translate in one click. The #A/a# button turns it all UPPERCASE."},
        {L"RECORD SCREEN", L"Record your screen", {L"Click the clock's *red dot* or press *Ctrl + Alt + R*.", L"The bar appears at the top. Click *Record*, pause whenever you like.", L"On *Stop*, the MP4 goes to your *Videos* folder."},
         L"The bar never shows in the video. Tick #Hide# to also leave out the cursor and the Start menu."},
        {L"CLEANUP", L"Space back", {L"Right-click the *background of a folder* or use the tray.", L"Vizu finds *temp files and caches* from Windows, browsers and Adobe.", L"Tick what you want and click *Clean*."},
         L"Work files #(PSD, AI, INDD, PDF)# and anything in use never leave."},
        {L"MONITOR", L"An eye on your PC", {L"The *performance* widget warns when the PC gets slow and shows what is heavy.", L"The *AI usage* widget shows Claude and ChatGPT limits at the screen's edge.", L"Turn both on in *Settings › Monitor*."},
         L"The ring turns yellow at 75% and red at 90%."},
        {L"TRY IT", L"WallEngine", {L"A *giant clock* on your wallpaper.", L"The *digital rain* running behind everything.", L"A *glass calendar* in the corner, ready for the Agenda."},
         L"Use all three together and tune each one in #Settings › WallEngine#."},
    };
    return i18n::Current() == 1 ? en : pt; // espanhol usa o texto em português até ter tradução
}

// Desenha um parágrafo com os destaques; devolve a altura usada.
static float TutRich(ui::Painter& p, ID2D1RenderTarget* rt, const wchar_t* src, float x, float y, float w, float size, D2D1_COLOR_F base, D2D1_COLOR_F accent) {
    std::wstring txt; std::vector<std::pair<DWRITE_TEXT_RANGE, int>> ranges; // 1 laranja, 2 branco
    int mode = 0; UINT32 start = 0;
    for (const wchar_t* c = src; *c; ++c) {
        if (*c == L'*' || *c == L'#') {
            int m = *c == L'*' ? 1 : 2;
            if (mode == m) { ranges.push_back({{start, (UINT32)txt.size() - start}, m}); mode = 0; }
            else if (!mode) { mode = m; start = (UINT32)txt.size(); }
            continue;
        }
        txt += *c;
    }
    IDWriteTextFormat* f = p.Format(size, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, nullptr, true);
    if (!f) return 0;
    IDWriteTextLayout* lay = nullptr; float h = 0;
    if (SUCCEEDED(g_dw->CreateTextLayout(txt.c_str(), (UINT32)txt.size(), f, w, 1000, &lay))) {
        ID2D1SolidColorBrush *bb = nullptr, *ba = nullptr, *bw = nullptr;
        rt->CreateSolidColorBrush(base, &bb); rt->CreateSolidColorBrush(accent, &ba); rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &bw);
        for (auto& r : ranges) { lay->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, r.first); lay->SetDrawingEffect(r.second == 1 ? (IUnknown*)ba : (IUnknown*)bw, r.first); }
        rt->DrawTextLayout(D2D1::Point2F(x, y), lay, bb);
        DWRITE_TEXT_METRICS m{}; lay->GetMetrics(&m); h = m.height;
        if (bb) bb->Release(); if (ba) ba->Release(); if (bw) bw->Release();
        lay->Release();
    }
    f->Release();
    return h;
}

static void TutRender() {
    HWND w = g_tut.wnd;
    if (!w) return;
    RECT wr; GetWindowRect(w, &wr);
    UINT dx = 96, dy = 96; GetDpiForMonitor(MonitorFromWindow(w, MONITOR_DEFAULTTOPRIMARY), MDT_EFFECTIVE_DPI, &dx, &dy);
    float s = dx / 96.0f;
    UINT CW = (UINT)(wr.right - wr.left), CH = (UINT)(wr.bottom - wr.top);
    fmt::Canvas cv(CW, CH);
    if (!cv.rt) return;
    ID2D1RenderTarget* rt = cv.rt;
    rt->Clear(D2D1::ColorF(0, 0.85f));
    rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ui::Theme th = g_theme; th.classic = true; th.neon = false; // sempre Vizu Classic
    th.accent = ui::Rgba(0xFF6633); th.text = ui::Rgba(0xFFFFFF); th.text2 = ui::Rgba(0xCDD2DA);
    ui::Tokens tk = ui::MakeTokens(th);
    ui::Painter p(rt, g_dw, th, tk, s);
    const D2D1_COLOR_F acc = ui::Rgba(0xFF6633), soft = ui::Rgba(0xCDD2DA), panel = ui::Rgba(0x4A4F57), panel2 = ui::Rgba(0x5C6472);
    g_tut.hits.clear();
    auto hit = [&](int id, D2D1_RECT_F r) { g_tut.hits.push_back({id, r}); return g_tut.hot == id; };

    const TutSlide& S = TutSlides()[g_tut.step];
    float W = 760 * s, H = 460 * s, L = (CW - W) / 2, Tp = (CH - H) / 2 - 20 * s;
    D2D1_RECT_F card = D2D1::RectF(L, Tp, L + W, Tp + H);
    p.Shadow(card, 16 * s);
    p.Fill(card, 16 * s, panel);
    float x = L + 48 * s, y = Tp + 44 * s, tw = W - 96 * s;
    wchar_t kick[64]; swprintf(kick, 64, L"%d / %d  ·  %ls", g_tut.step + 1, kTutSteps, S.k);
    p.Text(kick, D2D1::RectF(x, y, x + tw, y + 18 * s), 12, acc, DWRITE_FONT_WEIGHT_BOLD);
    y += 26 * s;
    p.Text(S.t, D2D1::RectF(x, y, x + tw, y + 44 * s), 32, ui::Rgba(0xFFFFFF), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    y += 66 * s;
    for (int i = 0; i < 3; ++i) {
        D2D1_RECT_F n = D2D1::RectF(x, y, x + 28 * s, y + 28 * s);
        p.Fill(n, 14 * s, acc);
        p.Text(std::to_wstring(i + 1), n, 13, ui::Rgba(0x000000), DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        float h = TutRich(p, rt, S.s[i], x + 44 * s, y + 3 * s, tw - 44 * s, 16, soft, acc);
        y += max(28 * s, h + 6 * s) + 16 * s;
    }
    D2D1_RECT_F tip = D2D1::RectF(x, card.bottom - 40 * s - 56 * s, x + tw, card.bottom - 40 * s);
    p.Fill(tip, 10 * s, panel2);
    TutRich(p, rt, S.tip, tip.left + 20 * s, tip.top + 17 * s, tw - 40 * s, 14, soft, acc);

    // setas dos lados (a da direita some na última tela, onde ficam os botões)
    float ab = 52 * s, ay = (card.top + card.bottom) / 2 - ab / 2;
    if (g_tut.step > 0) {
        D2D1_RECT_F a = D2D1::RectF(card.left - 28 * s - ab, ay, card.left - 28 * s, ay + ab);
        p.Fill(a, ab / 2, ui::Rgba(0xFFFFFF, hit(1, a) ? 0.22f : 0.10f));
        p.Feather("m15 18-6-6 6-6", a, 22 * s, ui::Rgba(0xFFFFFF), 2);
    }
    if (g_tut.step < kTutSteps - 1) {
        D2D1_RECT_F a = D2D1::RectF(card.right + 28 * s, ay, card.right + 28 * s + ab, ay + ab);
        p.Fill(a, ab / 2, hit(2, a) ? ui::Rgba(0xFF7A4D) : acc);
        p.Feather("m9 18 6-6-6-6", a, 22 * s, ui::Rgba(0x000000), 2);
    }
    // pontos
    float dd = 8 * s, gap = 10 * s, dw = kTutSteps * dd + (kTutSteps - 1) * gap + 16 * s, dx0 = (CW - dw) / 2, dyy = card.bottom + 28 * s;
    for (int i = 0, xx = 0; i < kTutSteps; ++i) {
        float wdt = i == g_tut.step ? 24 * s : dd;
        D2D1_RECT_F d = D2D1::RectF(dx0 + xx, dyy, dx0 + xx + wdt, dyy + dd);
        hit(20 + i, D2D1::RectF(d.left - 4 * s, d.top - 6 * s, d.right + 4 * s, d.bottom + 6 * s));
        p.Fill(d, dd / 2, i == g_tut.step ? acc : ui::Rgba(0xFFFFFF, g_tut.hot == 20 + i ? 0.6f : 0.3f));
        xx += (int)(wdt + gap);
    }
    // botões da última tela
    if (g_tut.step == kTutSteps - 1) {
        float bh = 44 * s, by = card.bottom + 56 * s;
        const wchar_t* tryT = T(L"Experimentar o WallEngine", L"Try WallEngine", L"Probar WallEngine");
        const wchar_t* doneT = T(L"Concluir", L"Done", L"Terminar");
        float w1 = p.Measure(tryT, 15, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 48 * s, w2 = p.Measure(doneT, 15, DWRITE_FONT_WEIGHT_SEMI_BOLD) + 48 * s, bx = (CW - w1 - w2 - 14 * s) / 2;
        D2D1_RECT_F b1 = D2D1::RectF(bx, by, bx + w1, by + bh), b2 = D2D1::RectF(b1.right + 14 * s, by, b1.right + 14 * s + w2, by + bh);
        p.Fill(b1, bh / 2, hit(3, b1) ? ui::Rgba(0xFF7A4D) : acc);
        p.Text(tryT, b1, 15, ui::Rgba(0x000000), DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        p.Fill(b2, bh / 2, ui::Rgba(0xFFFFFF, hit(4, b2) ? 0.2f : 0.08f));
        p.Stroke(b2, bh / 2, ui::Rgba(0xFFFFFF, 0.4f), 1 * s);
        p.Text(doneT, b2, 15, ui::Rgba(0xFFFFFF), DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    // pular (canto superior direito)
    {
        const wchar_t* sk = T(L"Pular tutorial  ✕", L"Skip tutorial  ✕", L"Saltar tutorial  ✕");
        float sw = p.Measure(sk, 14, DWRITE_FONT_WEIGHT_MEDIUM) + 32 * s;
        D2D1_RECT_F b = D2D1::RectF(CW - 32 * s - sw, 28 * s, CW - 32 * s, 28 * s + 38 * s);
        bool hh = hit(5, b);
        p.Fill(b, 19 * s, ui::Rgba(0xFFFFFF, hh ? 0.16f : 0.06f));
        p.Text(sk, b, 14, ui::Rgba(0xFFFFFF, hh ? 1.0f : 0.75f), DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    HBITMAP bmp = nullptr;
    if (FAILED(cv.Finish(&bmp)) || !bmp) return;
    HDC screen = GetDC(nullptr), mem = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(mem, bmp);
    POINT pos = {wr.left, wr.top}, zero = {0, 0};
    SIZE size = {(LONG)CW, (LONG)CH};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(w, screen, &pos, &size, mem, &zero, 0, &bf, ULW_ALPHA);
    SelectObject(mem, old); DeleteDC(mem); ReleaseDC(nullptr, screen); DeleteObject(bmp);
}

// Fecha e não mostra mais sozinho até a próxima função nova.
static void TutClose() {
    WriteDword(L"TutorialRev", kTutorialRev);
    if (g_tut.wnd) DestroyWindow(g_tut.wnd);
    g_tut.wnd = nullptr;
}
static void TutGo(int step) { g_tut.step = max(0, min(kTutSteps - 1, step)); TutRender(); }

static LRESULT CALLBACK TutProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; int hot = 0;
        for (auto& r : g_tut.hits) if (pt.x >= r.second.left && pt.x < r.second.right && pt.y >= r.second.top && pt.y < r.second.bottom) hot = r.first;
        if (hot != g_tut.hot) { g_tut.hot = hot; TutRender(); }
        return 0;
    }
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, g_tut.hot ? IDC_HAND : IDC_ARROW)); return TRUE;
    case WM_LBUTTONUP: {
        int id = g_tut.hot;
        if (id == 1) TutGo(g_tut.step - 1);
        else if (id == 2) TutGo(g_tut.step + 1);
        else if (id >= 20 && id < 20 + kTutSteps) TutGo(id - 20);
        else if (id == 3) { TutClose(); g_cfg.rainEnabled = true; g_cfg.wcOn = true; g_cfg.calOn = true; SaveConfig(); WallApply(); OpenSettings(); }
        else if (id == 4 || id == 5) TutClose();
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_LEFT) TutGo(g_tut.step - 1);
        else if (wp == VK_RIGHT || wp == VK_RETURN) { if (g_tut.step == kTutSteps - 1 && wp == VK_RETURN) TutClose(); else TutGo(g_tut.step + 1); }
        else if (wp == VK_ESCAPE) TutClose();
        return 0;
    case WM_DESTROY: if (g_tut.wnd == h) g_tut.wnd = nullptr; return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void OpenTutorial() {
    if (g_tut.wnd) { SetForegroundWindow(g_tut.wnd); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc = {}; wc.lpfnWndProc = TutProc; wc.hInstance = g_inst; wc.lpszClassName = L"VizuTutorial"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc); reg = true;
    }
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
    RECT r = mi.rcMonitor;
    g_tut.step = 0; g_tut.hot = 0;
    g_tut.wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW, L"VizuTutorial", L"Vizu", WS_POPUP, r.left, r.top, r.right - r.left, r.bottom - r.top, nullptr, nullptr, g_inst, nullptr);
    if (!g_tut.wnd) { diag::Note("tutorial", "CreateWindow", HRESULT_FROM_WIN32(GetLastError())); return; }
    TutRender();
    ShowWindow(g_tut.wnd, SW_SHOW);
    SetForegroundWindow(g_tut.wnd);
}
// Na abertura: mostra se ainda não viu esta revisão do tutorial.
static void TutorialAtStartup() {
    if (g_lite) return;
    if (ReadDword(L"TutorialRev", 0) >= kTutorialRev) return;
    OpenTutorial();
}
