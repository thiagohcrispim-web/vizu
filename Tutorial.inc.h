// Tutorial.inc.h - tutorial de boas-vindas (v19.8): 8 telas sobre a tela inteira, sempre no tema Vizu Classic.
// Fundo preto a 85%, setas dos lados, pontos embaixo e "Pular tutorial ✕". Pular ou concluir grava kTutorialRev:
// o tutorial só volta sozinho quando kTutorialRev subir (função nova). Configurações › "Clique para ver o tutorial" abre de novo.
// Incluído no Vizu.cpp depois do WallCal.inc.h (usa WallApply, OpenSettings, SaveConfig).

static const DWORD kTutorialRev = 2; // suba quando houver função nova para mostrar
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
         L"O anel fica\u00A0^amarelo^ em 75% e\u00A0~vermelho~ em\u00A090%."},
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
         L"The ring turns\u00A0^yellow^ at 75% and\u00A0~red~ at\u00A090%."},
        {L"TRY IT", L"WallEngine", {L"A *giant clock* on your wallpaper.", L"The *digital rain* running behind everything.", L"A *glass calendar* in the corner, ready for the Agenda."},
         L"Use all three together and tune each one in #Settings › WallEngine#."},
    };
    return i18n::Current() == 1 ? en : pt; // espanhol usa o texto em português até ter tradução
}

// Desenha um parágrafo com os destaques; devolve a altura usada.
// *x* laranja negrito, #x# branco negrito, ^x^ amarelo negrito, ~x~ vermelho negrito.
static float TutRich(ui::Painter& p, ID2D1RenderTarget* rt, const wchar_t* src, float x, float y, float w, float size, D2D1_COLOR_F base, float lineH) {
    std::wstring txt; std::vector<std::pair<DWRITE_TEXT_RANGE, int>> ranges;
    int mode = 0; UINT32 start = 0;
    for (const wchar_t* c = src; *c; ++c) {
        int m = *c == L'*' ? 1 : *c == L'#' ? 2 : *c == L'^' ? 3 : *c == L'~' ? 4 : 0;
        if (m) {
            if (mode == m) { ranges.push_back({{start, (UINT32)txt.size() - start}, m}); mode = 0; }
            else if (!mode) { mode = m; start = (UINT32)txt.size(); }
            continue;
        }
        txt += *c;
    }
    IDWriteTextFormat* f = p.Format(size, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_LEADING, nullptr, true);
    if (!f) return 0;
    f->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineH, lineH * 0.8f);
    IDWriteTextLayout* lay = nullptr; float h = 0;
    if (SUCCEEDED(g_dw->CreateTextLayout(txt.c_str(), (UINT32)txt.size(), f, w, 10000, &lay))) {
        static const UINT cols[5] = {0, 0xFF6633, 0xFFFFFF, 0xFFB020, 0xFF4D3D};
        ID2D1SolidColorBrush* bb = nullptr; ID2D1SolidColorBrush* hb[5] = {};
        rt->CreateSolidColorBrush(base, &bb);
        for (int i = 1; i < 5; ++i) rt->CreateSolidColorBrush(ui::Rgba(cols[i]), &hb[i]);
        for (auto& r : ranges) { lay->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, r.first); lay->SetDrawingEffect(hb[r.second], r.first); }
        rt->DrawTextLayout(D2D1::Point2F(x, y), lay, bb);
        DWRITE_TEXT_METRICS m{}; lay->GetMetrics(&m); h = m.height;
        if (bb) bb->Release();
        for (auto* b : hb) if (b) b->Release();
        lay->Release();
    }
    f->Release();
    return h;
}

// Igual ao protótipo (Design, página 2 · Introdução): tela de 1280×720 escalada para o monitor, fundo preto a 85%,
// texto à esquerda (x 160) e a ilustração da função à direita (x 700).
static void TutRender() {
    HWND w = g_tut.wnd;
    if (!w) return;
    RECT wr; GetWindowRect(w, &wr);
    UINT CW = (UINT)(wr.right - wr.left), CH = (UINT)(wr.bottom - wr.top);
    fmt::Canvas cv(CW, CH);
    if (!cv.rt) return;
    ID2D1RenderTarget* rt = cv.rt;
    rt->Clear(D2D1::ColorF(0, 0.85f));
    rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    float k = min(CW / 1280.0f, CH / 720.0f), OX = (CW - 1280 * k) / 2, OY = (CH - 720 * k) / 2;
    ui::Theme th = g_theme; th.classic = true; th.neon = false; th.dark = true; // sempre Vizu Classic
    th.accent = ui::Rgba(0xFF6633); th.text = ui::Rgba(0xFFFFFF); th.text2 = ui::Rgba(0xBCC2CC);
    ui::Tokens tk = ui::MakeTokens(th);
    ui::Painter p(rt, g_dw, th, tk, k);
    g_tut.hits.clear();
    auto R = [&](float x, float y, float ww, float hh) { return D2D1::RectF(OX + x * k, OY + y * k, OX + (x + ww) * k, OY + (y + hh) * k); };
    auto fill = [&](float x, float y, float ww, float hh, float rad, UINT c, float a = 1) { p.Fill(R(x, y, ww, hh), rad * k, ui::Rgba(c, a)); };
    auto line = [&](float x, float y, float ww, float hh, float rad, UINT c, float a, float sw) { p.Stroke(R(x, y, ww, hh), rad * k, ui::Rgba(c, a), sw * k); };
    auto grad = [&](float x, float y, float ww, float hh, float rad, float deg, UINT c1, UINT c2) {
        D2D1_RECT_F r = R(x, y, ww, hh); D2D1_GRADIENT_STOP st[2] = {{0, ui::Rgba(c1)}, {1, ui::Rgba(c2)}};
        if (auto* b = p.Angle(r, deg, st, 2)) { p.FillBrush(r, rad * k, b); b->Release(); }
    };
    auto text = [&](const std::wstring& t, float x, float y, float ww, float hh, float size, UINT c, DWRITE_FONT_WEIGHT wt = DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_TEXT_ALIGNMENT al = DWRITE_TEXT_ALIGNMENT_LEADING, const wchar_t* fam = nullptr, float a = 1) {
        IDWriteTextFormat* f = p.Format(size, wt, al, fam);
        ID2D1SolidColorBrush* b = nullptr;
        if (f && SUCCEEDED(rt->CreateSolidColorBrush(ui::Rgba(c, a), &b))) { rt->DrawText(t.c_str(), (UINT32)t.size(), f, R(x, y, ww, hh), b); b->Release(); }
        if (f) f->Release();
    };
    auto hit = [&](int id, D2D1_RECT_F r) { g_tut.hits.push_back({id, r}); return g_tut.hot == id; };
    auto key = [&](float x, float y, float ww, float hh, const wchar_t* t, float fs) { // tecla: borda laranja, brilho e a "tampa" de dentro
        line(x - 2, y - 2, ww + 4, hh + 4, 12, 0xFF6633, 0.18f, 4);
        line(x, y, ww, hh, 10, 0xFF6633, 1, 1.5f);
        line(x + 5, y + 4, ww - 10, hh - 15, 7, 0xFF6633, 1, 1.5f);
        text(t, x + 5, y + 4, ww - 10, hh - 15, fs, 0xFF6633, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, L"Space Grotesk");
    };
    auto icon = [&](const char* name, float x, float y, float box, UINT c, float a) { p.FilledIcon(name, R(x, y, box, box), box * k, ui::Rgba(c, a)); };
    auto clockPill = [&](float x, float y, float fs) {
        fill(x, y, 500, 76, 26, 0x4A4F57); line(x, y, 500, 76, 26, 0x5C6472, 1, 1);
    };
    const wchar_t* clockFont = L"Bahnschrift";

    const TutSlide& S = TutSlides()[g_tut.step];
    const int i = g_tut.step + 1;
    wchar_t kick[64]; swprintf(kick, 64, L"%d %ls %d  ·  %ls", i, T(L"DE", L"OF", L"DE"), kTutSteps, S.k);
    text(kick, 160, 150, 470, 20, 12, 0xFF6633, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    {
        std::wstring t = S.t;
        float tw = p.Measure(t, 46, DWRITE_FONT_WEIGHT_SEMI_BOLD) / k;
        text(t, 160, 180, 560, 56, 46, 0xFFFFFF, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        text(L".", 160 + tw, 180, 40, 56, 46, 0xFF6633, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    }
    for (int n = 0; n < 3; ++n) {
        float y = 290 + n * 66.0f;
        fill(160, y - 2, 28, 28, 14, 0x4A4F57);
        text(std::to_wstring(n + 1), 160, y - 2, 28, 28, 12, 0xFFFFFF, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        TutRich(p, rt, S.s[n], OX + 204 * k, OY + y * k, 426 * k, 17, ui::Rgba(0xE3E6EB), 24 * k);
    }
    fill(160, 500, 470, 1, 0, 0x4A4F57);
    TutRich(p, rt, S.tip, OX + 160 * k, OY + 516 * k, 470 * k, 14, ui::Rgba(0xBCC2CC), 21 * k);

    // ilustração de cada tela
    switch (i) {
    case 1:
        fill(700, 150, 150, 118, 8, 0x2E333B); line(700, 150, 150, 118, 8, 0xFF6633, 1, 2);
        grad(710, 160, 130, 74, 4, 135, 0xFFB27A, 0xC2452A);
        text(L"campanha_verao.psd", 710, 240, 134, 18, 11, 0xFFFFFF);
        key(700, 330, 220, 60, T(L"ESPAÇO", L"SPACE", L"ESPACIO"), 18);
        p.Feather("M4 20h78M70 8l12 12-12 12", R(870, 200, 90, 40), 24 * k, ui::Rgba(0xBCC2CC), 2); // seta (caixa 24 → a seta ocupa o retângulo)
        p.Shadow(R(970, 120, 250, 330), 12 * k);
        fill(970, 120, 250, 330, 12, 0x2E333B);
        text(L"campanha_verao.psd", 970, 120, 250, 28, 10, 0xFFFFFF, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        grad(980, 148, 230, 230, 6, 135, 0xFFB27A, 0xC2452A);
        {
            float cx = 980; const wchar_t* chips[3] = {L"4000 × 2500 px", L"300 dpi", L"CMYK"};
            for (auto* c : chips) { float cw = p.Measure(c, 9) / k + 12; fill(cx, 390, cw, 16, 4, 0x23272E); text(c, cx, 390, cw, 16, 9, 0xFFFFFF, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER); cx += cw + 4; }
        }
        break;
    case 2: { // o anel de verdade (RingBitmap), com o tema Classic e um PSD selecionado, como no protótipo
        static HBITMAP ring = nullptr;
        if (!ring) {
            auto sides = std::move(g_sides); int side = g_side, hot = g_ringHot; float scale = g_ringScale;
            double shown = g_ringShownAt, hotAt = g_ringHotAt; bool closing = g_ringClosing, launch = g_ringLaunch;
            ui::Theme saved = g_theme; auto files = g_convFiles;
            g_theme = ui::MakeTheme(ui::kThemeClassic, false);
            g_convFiles = {L"C:\\campanha_verao.psd"}; g_ringLaunch = false;
            BuildSides();
            g_side = 0; g_ringHot = 0; g_ringScale = max(1.0f, k); g_ringShownAt = g_ringHotAt = 0; g_ringClosing = false;
            ring = RingBitmap();
            g_sides = std::move(sides); g_side = side; g_ringHot = hot; g_ringScale = scale;
            g_ringShownAt = shown; g_ringHotAt = hotAt; g_ringClosing = closing; g_ringLaunch = launch; g_theme = saved; g_convFiles = files;
        }
        IWICImagingFactory* wic = nullptr; IWICBitmap* wb = nullptr; ID2D1Bitmap* bm = nullptr;
        if (ring && SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) &&
            SUCCEEDED(wic->CreateBitmapFromHBITMAP(ring, nullptr, WICBitmapUsePremultipliedAlpha, &wb)) && SUCCEEDED(rt->CreateBitmapFromWicBitmap(wb, &bm)))
            rt->DrawBitmap(bm, R(740, 70, 400, 400), 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        if (bm) bm->Release(); if (wb) wb->Release(); if (wic) wic->Release();
        float sw = p.Measure(T(L"Segure", L"Hold", L"Mantén"), 14) / k, gx = 940 - (sw + 14 + 96) / 2;
        text(T(L"Segure", L"Hold", L"Mantén"), gx, 478, sw + 4, 60, 14, 0xBCC2CC);
        key(gx + sw + 14, 478, 96, 60, L"ALT", 18);
        break;
    }
    case 3: {
        clockPill(700, 170, 42);
        p.Feather("M10.29 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0zM12 9v4M12 17h.01", R(714, 197, 22, 22), 22 * k, ui::Rgba(0xBCC2CC), 2);
        text(L"14:32", 752, 170, 160, 76, 42, 0xFFFFFF, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, clockFont);
        const char* chips[5] = {"skull", "timer", "chat-ai", "broom", "scan-text"};
        for (int c = 0; c < 5; ++c) { float x = 920 + c * 48.0f; fill(x, 188, 40, 40, 20, 0x23272E, 0.55f); icon(chips[c], x + 9, 197, 22, 0xFFFFFF, 0.55f); }
        const wchar_t* lt[6][2] = {{L"Agenda", L"triângulo"}, {L"Roubo de tempo", L"caveira"}, {L"Foco", L"Pomodoro"}, {L"ChatGPT", L"ou outro site"}, {L"Limpeza", L"vassoura"}, {L"Ler texto", L"OCR"}};
        for (int n = 0; n < 6; ++n) {
            float bx = 700 + (n % 3) * 172.0f, by = 300 + (n / 3) * 70.0f;
            fill(bx, by, 2, 52, 0, 0xFF6633);
            text(lt[n][0], bx + 12, by + 6, 150, 20, 13, 0xFFFFFF, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            text(lt[n][1], bx + 12, by + 27, 150, 18, 12, 0xBCC2CC);
        }
        break;
    }
    case 4: {
        key(720, 120, 84, 56, L"CTRL", 15); text(L"+", 804, 120, 32, 56, 22, 0xBCC2CC, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        key(836, 120, 84, 56, L"ALT", 15); text(L"+", 920, 120, 32, 56, 22, 0xBCC2CC, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        key(952, 120, 64, 56, L"T", 15);
        grad(720, 220, 480, 120, 8, 135, 0xC9CED6, 0x8A919E);
        text(L"Summer collection", 744, 244, 400, 36, 28, 0x141413, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        text(L"New arrivals every week", 744, 286, 400, 22, 15, 0x23272E);
        line(736, 240, 290, 84, 0, 0xFF6633, 1, 2);
        fill(736, 222, 66, 18, 0, 0xFF6633); text(L"290 × 84", 736, 222, 66, 18, 11, 0x000000, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        fill(880, 366, 320, 150, 12, 0x4A4F57); line(880, 366, 320, 150, 12, 0x5C6472, 1, 1);
        text(T(L"✓ Copiado · inglês detectado", L"✓ Copied · English detected", L"✓ Copiado · inglés detectado"), 894, 380, 290, 16, 11, 0x30D158, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        fill(894, 404, 292, 40, 10, 0x23272E); text(L"Summer collection — new arrivals every week.", 904, 404, 280, 40, 13, 0xFFFFFF);
        text(T(L"Traduzir:", L"Translate:", L"Traducir:"), 894, 454, 70, 28, 12, 0xBCC2CC);
        fill(960, 454, 40, 28, 6, 0xFF6633); text(L"PT", 960, 454, 40, 28, 11, 0x000000, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        line(1006, 454, 40, 28, 6, 0xFFFFFF, 0.16f, 1); text(L"ES", 1006, 454, 40, 28, 11, 0xFFFFFF, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        line(1146, 454, 40, 28, 6, 0xFFFFFF, 0.16f, 1); text(L"A/a", 1146, 454, 40, 28, 11, 0xFFFFFF, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        break;
    }
    case 5: {
        clockPill(700, 120, 34);
        text(L"14:32", 716, 120, 160, 76, 34, 0xFFFFFF, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, clockFont);
        fill(1050, 138, 40, 40, 20, 0x23272E, 0.55f); fill(1098, 138, 40, 40, 20, 0x23272E, 0.55f);
        fill(1146, 138, 40, 40, 20, 0x23272E, 0.55f); line(1145, 137, 42, 42, 21, 0xFF6633, 1, 2); fill(1159, 151, 14, 14, 7, 0xFF3B30);
        p.Shadow(R(700, 230, 520, 52), 26 * k);
        fill(700, 230, 520, 52, 26, 0x4A4F57); line(700, 230, 520, 52, 26, 0xFF3B30, 0.85f, 1.2f);
        fill(710, 238, 84, 36, 18, 0xFFFFFF, 0.05f); fill(720, 250, 12, 12, 6, 0xBCC2CC, 0.45f);
        text(T(L"Gravar", L"Record", L"Grabar"), 738, 238, 54, 36, 13, 0xBCC2CC, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_TEXT_ALIGNMENT_LEADING, nullptr, 0.45f);
        fill(802, 238, 68, 36, 18, 0xFFFFFF, 0.05f); text(T(L"Pausar", L"Pause", L"Pausar"), 802, 238, 68, 36, 13, 0xFFFFFF, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_TEXT_ALIGNMENT_CENTER);
        fill(878, 238, 60, 36, 18, 0xFFFFFF, 0.05f); text(T(L"Parar", L"Stop", L"Detener"), 878, 238, 60, 36, 13, 0xFFFFFF, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_TEXT_ALIGNMENT_CENTER);
        fill(948, 252, 8, 8, 4, 0xFF3B30); text(L"00:03:42", 964, 238, 100, 36, 15, 0xFFFFFF, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        grad(700, 310, 520, 200, 10, 135, 0xC9CED6, 0x4A4F57);
        fill(720, 330, 72, 72, 12, 0x000000, 0.35f);
        icon("play", 741, 351, 30, 0xFFFFFF, 1);
        if (!feather::FindFilled("play")) { // triângulo de "play" desenhado à mão se o ícone não existir
            ID2D1Factory* f = nullptr; rt->GetFactory(&f);
            if (ID2D1PathGeometry* g = f ? ui::SvgPath(f, "M8 5v14l11-7z", 30 * k / 24, OX + 741 * k, OY + 351 * k, true) : nullptr) {
                ID2D1SolidColorBrush* b = nullptr; if (SUCCEEDED(rt->CreateSolidColorBrush(ui::Rgba(0xFFFFFF), &b))) { rt->FillGeometry(g, b); b->Release(); }
                g->Release();
            }
            if (f) f->Release();
        }
        text(L"Vizu Gravação 2026-11-28 14-32-05.mp4", 810, 340, 400, 20, 15, 0x141413, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        text(T(L"Vídeos", L"Videos", L"Vídeos"), 810, 364, 200, 18, 13, 0x23272E);
        break;
    }
    case 6: {
        fill(720, 130, 480, 400, 12, 0x2E333B);
        text(T(L"Limpeza", L"Cleanup", L"Limpieza"), 740, 150, 300, 24, 18, 0xFFFFFF, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, L"Space Grotesk");
        fill(740, 188, 440, 72, 12, 0x3A3F47);
        text(T(L"Pronto para liberar", L"Ready to free", L"Listo para liberar"), 756, 200, 200, 16, 12, 0xBCC2CC);
        text(L"11,7 GB", 756, 216, 200, 34, 26, 0xFFFFFF, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, L"Space Grotesk");
        fill(1050, 207, 114, 34, 10, 0xFF6633); text(T(L"Limpar 11,7 GB", L"Clean 11.7 GB", L"Limpiar 11,7 GB"), 1050, 207, 114, 34, 13, 0x000000, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        const wchar_t* rows[5][2] = {{T(L"Sistema", L"System", L"Sistema"), L"3,8 GB"}, {T(L"Navegadores", L"Browsers", L"Navegadores"), L"2,1 GB"}, {L"Adobe", L"4,6 GB"}, {T(L"Mensageiros", L"Messengers", L"Mensajería"), L"1,2 GB"}, {T(L"Outros", L"Other", L"Otros"), L"700 MB"}};
        for (int n = 0; n < 5; ++n) {
            float y = 268 + n * 43.0f;
            if (n < 4) { fill(748, y + 12, 18, 18, 9, 0xFF6633); text(L"✓", 748, y + 12, 18, 18, 11, 0x000000, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER); }
            else line(748, y + 12, 18, 18, 9, 0xB5BAC2, 1, 1.5f);
            text(rows[n][0], 778, y + 8, 300, 26, 13, 0xFFFFFF);
            text(rows[n][1], 1080, y + 8, 96, 26, 12, 0xBCC2CC, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            fill(744, y + 41, 432, 1, 0, 0x4A4F57);
        }
        text(T(L"PSD, AI, INDD e PDF nunca saem.", L"PSD, AI, INDD and PDF never leave.", L"PSD, AI, INDD y PDF nunca salen."), 740, 492, 400, 18, 12, 0xBCC2CC);
        break;
    }
    case 7: {
        fill(720, 150, 330, 270, 14, 0x353A42); line(720, 150, 330, 270, 14, 0xFF6633, 1, 1.5f);
        text(T(L"O PC está lento", L"The PC is slow", L"El PC está lento"), 738, 168, 290, 24, 18, 0xFFFFFF, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING, L"Space Grotesk");
        text(T(L"A memória está quase cheia.", L"Memory is almost full.", L"La memoria está casi llena."), 738, 194, 290, 16, 12, 0xBCC2CC);
        const wchar_t* pn[3] = {L"CPU", T(L"Memória", L"Memory", L"Memoria"), T(L"Disco", L"Disk", L"Disco")}; int pv[3] = {34, 91, 8};
        for (int n = 0; n < 3; ++n) {
            float y = 222 + n * 38.0f;
            text(pn[n], 738, y, 200, 18, 13, 0xFFFFFF);
            text(std::to_wstring(pv[n]) + L"%", 940, y, 92, 18, 13, 0xFFFFFF, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            fill(738, y + 22, 294, 6, 3, 0x2A2F36); fill(738, y + 22, 294 * pv[n] / 100.0f, 6, 3, n == 1 ? 0xFF6633 : 0x8A919C);
        }
        text(T(L"O QUE ESTÁ PESANDO", L"WHAT IS HEAVY", L"LO QUE PESA"), 738, 340, 290, 16, 12, 0xBCC2CC, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        text(L"Adobe Photoshop", 738, 362, 200, 18, 13, 0xFFFFFF);
        text(L"6,2 GB", 940, 362, 92, 18, 12, 0xBCC2CC, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
        text(T(L"Desempenho", L"Performance", L"Rendimiento"), 720, 430, 330, 18, 12, 0xBCC2CC, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        fill(1110, 170, 90, 178, 14, 0x0E0E10, 0.97f); line(1110, 170, 90, 178, 14, 0xFFFFFF, 0.10f, 1);
        int av[2] = {42, 78}; UINT ac[2] = {0x2BD97C, 0xFFB020};
        for (int n = 0; n < 2; ++n) {
            float cx = 1148, cy = 206 + n * 84.0f;
            D2D1_POINT_2F c = D2D1::Point2F(OX + cx * k, OY + cy * k);
            ID2D1SolidColorBrush* b = nullptr;
            if (SUCCEEDED(rt->CreateSolidColorBrush(ui::Rgba(0xFFFFFF, 0.07f), &b))) { rt->FillEllipse(D2D1::Ellipse(c, 17 * k, 17 * k), b); b->Release(); }
            if (SUCCEEDED(rt->CreateSolidColorBrush(ui::Rgba(0xFFFFFF, 0.12f), &b))) { rt->DrawEllipse(D2D1::Ellipse(c, 20 * k, 20 * k), b, 3 * k); b->Release(); }
            ID2D1Factory* f = nullptr; rt->GetFactory(&f); ID2D1PathGeometry* g = nullptr; ID2D1GeometrySink* sk = nullptr;
            if (f && SUCCEEDED(f->CreatePathGeometry(&g)) && SUCCEEDED(g->Open(&sk))) {
                float a = av[n] / 100.0f * 6.2831853f;
                sk->BeginFigure(D2D1::Point2F(c.x, c.y - 20 * k), D2D1_FIGURE_BEGIN_HOLLOW);
                sk->AddArc(D2D1::ArcSegment(D2D1::Point2F(c.x + 20 * k * sinf(a), c.y - 20 * k * cosf(a)), D2D1::SizeF(20 * k, 20 * k), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE, a > 3.14159f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
                sk->EndFigure(D2D1_FIGURE_END_OPEN); sk->Close();
                ID2D1StrokeStyle* ss = nullptr;
                f->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND), nullptr, 0, &ss);
                if (SUCCEEDED(rt->CreateSolidColorBrush(ui::Rgba(ac[n]), &b))) { rt->DrawGeometry(g, b, 3 * k, ss); b->Release(); }
                if (ss) ss->Release();
            }
            if (sk) sk->Release(); if (g) g->Release(); if (f) f->Release();
            text(std::to_wstring(av[n]) + L"%", 1110, cy + 26, 76, 16, 12, 0xFFFFFF, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        text(L"Claude · ChatGPT", 1090, 358, 130, 16, 12, 0xBCC2CC, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        break;
    }
    case 8: {
        p.Shadow(R(700, 110, 520, 320), 12 * k);
        { // papel de parede, relógio, chuva e calendário de vidro, na ordem das camadas
            D2D1_RECT_F r = R(700, 110, 520, 320);
            D2D1_GRADIENT_STOP st[3] = {{0, ui::Rgba(0x5B6F8C)}, {0.45f, ui::Rgba(0x2B3A52)}, {1, ui::Rgba(0x121822)}};
            if (auto* b = p.Angle(r, 135, st, 3)) { p.FillBrush(r, 12 * k, b); b->Release(); }
        }
        text(L"14:32", 700, 110, 520, 320, 120, 0xFF6633, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER, nullptr, 0.55f);
        for (int r = 0; r < 14; ++r) {
            float x = 720 + r * 23.0f, y = 110 + (r * 53) % 220, hh = 40 + (r * 17) % 50;
            D2D1_RECT_F rr = R(x, y, 2, hh); D2D1_GRADIENT_STOP st[2] = {{0, ui::Rgba(0xFF6633, 0)}, {1, ui::Rgba(0xFF6633, 0.55f)}};
            if (auto* b = p.Angle(rr, 180, st, 2)) { rt->FillRectangle(rr, b); b->Release(); }
        }
        fill(1038, 128, 164, 150, 12, 0x0A0C10, 0.38f);
        { D2D1_RECT_F rr = R(1038, 128, 164, 150); D2D1_GRADIENT_STOP st[2] = {{0, ui::Rgba(0xFFFFFF, 0.10f)}, {1, ui::Rgba(0xFFFFFF, 0.02f)}};
          if (auto* b = p.Angle(rr, 160, st, 2)) { p.FillBrush(rr, 12 * k, b); b->Release(); } }
        line(1038, 128, 164, 150, 12, 0xFFFFFF, 0.12f, 1); fill(1050, 128, 140, 1, 0, 0xFFFFFF, 0.35f);
        {
            std::wstring mn = MonthName(11); if (!mn.empty()) CharUpperBuffW(&mn[0], 1);
            text(mn + L" 2026", 1048, 136, 150, 16, 11, 0xFFFFFF, DWRITE_FONT_WEIGHT_MEDIUM);
            float cw = 144 / 7.0f;
            for (int d = 0; d < 7; ++d) text(WallCalWeekLetter(d), 1048 + d * cw, 158, cw, 12, 8, 0xFFFFFF, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, nullptr, 0.45f);
            for (int n = 1; n <= 28; ++n) {
                int idx = n - 1, c = idx % 7, rw = idx / 7; float x = 1048 + c * cw, y = 174 + rw * 15.0f;
                if (n == 27) fill(x + 2, y - 1, cw - 4, 14, 6, 0xFF6633);
                text(std::to_wstring(n), x, y, cw, 12, 8, n == 27 ? 0x000000 : 0xFFFFFF, n == 27 ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER, nullptr, n == 27 ? 1 : 0.82f);
            }
        }
        const wchar_t* lb[3] = {T(L"WallClock · camada de baixo", L"WallClock · bottom layer", L"WallClock · capa de abajo"), T(L"Chuva · meio", L"Rain · middle", L"Lluvia · medio"), T(L"Calendário · cima", L"Calendar · top", L"Calendario · arriba")};
        float tot = 0, gw[3]; for (int n = 0; n < 3; ++n) { gw[n] = p.Measure(lb[n], 12) / k; tot += gw[n]; }
        float lx = 960 - (tot + 48) / 2;
        for (int n = 0; n < 3; ++n) { text(lb[n], lx, 446, gw[n] + 4, 18, 12, 0xBCC2CC); lx += gw[n] + 24; }
        break;
    }
    }

    // setas (56 px, contorno branco 28%)
    {
        D2D1_RECT_F a = R(32, 332, 56, 56);
        bool on = g_tut.step > 0, hh = on && hit(1, a);
        p.Fill(a, 28 * k, ui::Rgba(0xFFFFFF, (hh ? 0.16f : 0.06f) * (on ? 1 : 0.3f)));
        p.Stroke(a, 28 * k, ui::Rgba(0xFFFFFF, 0.28f * (on ? 1 : 0.3f)), 1 * k);
        p.Feather("m15 18-6-6 6-6", a, 22 * k, ui::Rgba(0xFFFFFF, on ? 1 : 0.3f), 2);
    }
    if (g_tut.step < kTutSteps - 1) {
        D2D1_RECT_F a = R(1192, 332, 56, 56);
        bool hh = hit(2, a);
        p.Fill(a, 28 * k, ui::Rgba(0xFFFFFF, hh ? 0.16f : 0.06f));
        p.Stroke(a, 28 * k, ui::Rgba(0xFFFFFF, 0.28f), 1 * k);
        p.Feather("m9 18 6-6-6-6", a, 22 * k, ui::Rgba(0xFFFFFF), 2);
    }
    // pontos (centralizados em 640, y 646)
    {
        float tot = (kTutSteps - 1) * 8 + 24 + (kTutSteps - 1) * 8.0f, x = 640 - tot / 2;
        for (int n = 0; n < kTutSteps; ++n) {
            float ww = n == g_tut.step ? 24.0f : 8.0f;
            hit(20 + n, R(x - 3, 640, ww + 6, 20));
            fill(x, 648, ww, 8, 4, n == g_tut.step ? 0xFF6633 : 0xFFFFFF, n == g_tut.step ? 1 : (g_tut.hot == 20 + n ? 0.6f : 0.35f));
            x += ww + 8;
        }
    }
    if (g_tut.step == kTutSteps - 1) { // última tela: os dois botões no lugar do "Pular"
        D2D1_RECT_F b1 = R(160, 586, 250, 48), b2 = R(422, 586, 120, 48);
        p.Fill(b1, 8 * k, hit(3, b1) ? ui::Rgba(0xFF7A4D) : ui::Rgba(0xFF6633));
        text(T(L"Experimentar o WallEngine", L"Try WallEngine", L"Probar WallEngine"), 160, 586, 250, 48, 15, 0x000000, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
        if (hit(4, b2)) p.Fill(b2, 8 * k, ui::Rgba(0xFFFFFF, 0.08f));
        p.Stroke(b2, 8 * k, ui::Rgba(0x9AA1AD), 1 * k);
        text(T(L"Concluir", L"Done", L"Terminar"), 422, 586, 120, 48, 15, 0xFFFFFF, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
    } else {
        const wchar_t* sk = T(L"Pular tutorial", L"Skip tutorial", L"Saltar tutorial");
        float sw = p.Measure(sk, 13, DWRITE_FONT_WEIGHT_MEDIUM) / k;
        D2D1_RECT_F b = R(1240 - sw - 24, 636, sw + 24, 32);
        bool hh = hit(5, b);
        UINT c = hh ? 0xFFFFFF : 0xBCC2CC;
        text(sk, 1240 - sw - 24, 636, sw + 2, 32, 13, c, DWRITE_FONT_WEIGHT_MEDIUM);
        p.Feather("M18 6 6 18M6 6l12 12", R(1224, 644, 16, 16), 16 * k, ui::Rgba(c), 2);
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
