// WallCal.inc.h - WallEngine: calendário do mês no fundo (v19.8).
// Caixa de vidro: o papel de parede atrás dela aparece desfocado, com a cor e a opacidade escolhidas por cima.
// Fica sempre no fundo da pilha de janelas (acima da área de trabalho, abaixo de tudo o mais); arrasta para mover,
// os cantos ampliam ou diminuem na mesma proporção, as setas trocam de mês e o clique num dia abre a Agenda nele.
// Incluído no Vizu.cpp depois do Agenda.inc.h (usa LoadAgenda, AddDays, DayOfWeek, MonthName e OpenAgendaWindow).

struct WallCalUi {
    HWND wnd = nullptr;
    int monthOff = 0;              // 0 = mês atual
    int hot = 0;                   // 1 anterior, 2 próximo, 3 hoje, 10..13 cantos, 100 + dia
    int resize = 0;                // canto sendo puxado (10..13); 0 = nada
    POINT anchor{};                // canto oposto, fixo durante o redimensionamento (tela)
    float startDiag = 1;           // diagonal do cartão no início do redimensionamento
    DWORD startScale = 100;
    float s = 1, M = 0, W = 0, H = 0;
    D2D1_RECT_F rPrev{}, rNext{}, rToday{};
    std::vector<std::pair<int, D2D1_RECT_F>> days;
    std::vector<BYTE> blur;        // papel de parede do monitor em 1/12, desfocado (PBGRA)
    UINT blurW = 0, blurH = 0;
    RECT blurMon{};
    std::wstring blurPath;
};
static WallCalUi g_wcal;
static const float kWallCalBlurK = 12; // fator de redução do papel de parede usado no vidro

// Papel de parede do monitor, "preenchendo" como o Windows, reduzido e desfocado (3 passadas de caixa).
static void WallCalLoadBlur(const RECT& mon) {
    wchar_t path[MAX_PATH] = {};
    SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0);
    if (g_wcal.blurPath == path && EqualRect(&g_wcal.blurMon, &mon) && !g_wcal.blur.empty()) return;
    g_wcal.blurPath = path; g_wcal.blurMon = mon; g_wcal.blur.clear(); g_wcal.blurW = g_wcal.blurH = 0;
    if (!path[0]) return;
    UINT mw = (UINT)max(1L, mon.right - mon.left), mh = (UINT)max(1L, mon.bottom - mon.top);
    UINT bw = max(1u, (UINT)(mw / kWallCalBlurK)), bh = max(1u, (UINT)(mh / kWallCalBlurK));
    IWICImagingFactory* wic = nullptr; IWICBitmapDecoder* dec = nullptr; IWICBitmapFrameDecode* fr = nullptr;
    IWICBitmapClipper* clip = nullptr; IWICBitmapScaler* sc = nullptr; IWICFormatConverter* cv = nullptr;
    bool ok = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) &&
        SUCCEEDED(wic->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec)) && SUCCEEDED(dec->GetFrame(0, &fr))) {
        UINT iw = 0, ih = 0; fr->GetSize(&iw, &ih);
        if (iw && ih) {
            double k = max((double)mw / iw, (double)mh / ih); // "preencher": corta o que sobra, centralizado
            UINT cw = min(iw, (UINT)(mw / k + 0.5)), ch = min(ih, (UINT)(mh / k + 0.5));
            WICRect r = {(INT)((iw - cw) / 2), (INT)((ih - ch) / 2), (INT)max(1u, cw), (INT)max(1u, ch)};
            if (SUCCEEDED(wic->CreateBitmapClipper(&clip)) && SUCCEEDED(clip->Initialize(fr, &r)) &&
                SUCCEEDED(wic->CreateBitmapScaler(&sc)) && SUCCEEDED(sc->Initialize(clip, bw, bh, WICBitmapInterpolationModeFant)) &&
                SUCCEEDED(wic->CreateFormatConverter(&cv)) && SUCCEEDED(cv->Initialize(sc, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) {
                g_wcal.blur.resize((size_t)bw * bh * 4);
                ok = SUCCEEDED(cv->CopyPixels(nullptr, bw * 4, (UINT)g_wcal.blur.size(), g_wcal.blur.data()));
            }
        }
    }
    if (cv) cv->Release(); if (sc) sc->Release(); if (clip) clip->Release(); if (fr) fr->Release(); if (dec) dec->Release(); if (wic) wic->Release();
    if (!ok) { g_wcal.blur.clear(); return; }
    g_wcal.blurW = bw; g_wcal.blurH = bh;
    std::vector<BYTE> tmp(g_wcal.blur.size());
    const int rad = 2;
    for (int pass = 0; pass < 3; ++pass) { // caixa horizontal + vertical, três vezes: quase gaussiano
        for (UINT y = 0; y < bh; ++y)
            for (UINT x = 0; x < bw; ++x)
                for (int c = 0; c < 4; ++c) {
                    int sum = 0, n = 0;
                    for (int d = -rad; d <= rad; ++d) { int xx = (int)x + d; if (xx < 0 || xx >= (int)bw) continue; sum += g_wcal.blur[((size_t)y * bw + xx) * 4 + c]; ++n; }
                    tmp[((size_t)y * bw + x) * 4 + c] = (BYTE)(sum / n);
                }
        for (UINT y = 0; y < bh; ++y)
            for (UINT x = 0; x < bw; ++x)
                for (int c = 0; c < 4; ++c) {
                    int sum = 0, n = 0;
                    for (int d = -rad; d <= rad; ++d) { int yy = (int)y + d; if (yy < 0 || yy >= (int)bh) continue; sum += tmp[((size_t)yy * bw + x) * 4 + c]; ++n; }
                    g_wcal.blur[((size_t)y * bw + x) * 4 + c] = (BYTE)(sum / n);
                }
    }
}

// Iniciais dos dias, começando no domingo.
static const wchar_t* WallCalWeekLetter(int d) {
    static const wchar_t* pt[] = {L"D", L"S", L"T", L"Q", L"Q", L"S", L"S"};
    static const wchar_t* en[] = {L"S", L"M", L"T", L"W", L"T", L"F", L"S"};
    static const wchar_t* es[] = {L"D", L"L", L"M", L"X", L"J", L"V", L"S"};
    int l = i18n::Current();
    return (l == 1 ? en : l == 2 ? es : pt)[d];
}

// Primeiro dia do mês exibido (mês atual + deslocamento das setas).
static SYSTEMTIME WallCalMonth() {
    SYSTEMTIME t; GetLocalTime(&t);
    int m = (int)t.wYear * 12 + (t.wMonth - 1) + g_wcal.monthOff;
    t.wYear = (WORD)(m / 12); t.wMonth = (WORD)(m % 12 + 1); t.wDay = 1; t.wHour = 12; t.wMinute = t.wSecond = t.wMilliseconds = 0;
    return t;
}
static int WallCalDaysIn(SYSTEMTIME first) {
    static const int dm[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int y = first.wYear, m = first.wMonth;
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return m == 2 && leap ? 29 : dm[m - 1];
}

static void WallCalRender() {
    HWND w = g_wcal.wnd;
    if (!w) return;
    RECT wr; GetWindowRect(w, &wr);
    HMONITOR mon = MonitorFromWindow(w, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(mon, &mi);
    UINT dpix = 96, dpiy = 96; GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpix, &dpiy);
    float s = dpix / 96.0f * g_cfg.calScale / 100.0f, M = 24 * dpix / 96.0f;
    float W = 320 * s, H = 296 * s;
    g_wcal.s = s; g_wcal.M = M; g_wcal.W = W; g_wcal.H = H;
    UINT CW = (UINT)(W + 2 * M), CH = (UINT)(H + 2 * M);
    fmt::Canvas cv(CW, CH);
    if (!cv.rt) return;
    ID2D1RenderTarget* rt = cv.rt;
    rt->Clear(D2D1::ColorF(0, 0));
    rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ui::Theme th = g_theme;
    ui::Tokens tk = ui::MakeTokens(th);
    ui::Painter p(rt, g_dw, th, tk, s);
    D2D1_RECT_F card = D2D1::RectF(M, M, M + W, M + H);
    float rad = 16 * s;
    { // sombra só por fora do cartão: por dentro ela escurecia o vidro e a opacidade baixa não ficava transparente
        ID2D1Factory* f0 = nullptr; rt->GetFactory(&f0);
        ID2D1RectangleGeometry* all = nullptr; ID2D1RoundedRectangleGeometry* in = nullptr; ID2D1PathGeometry* out = nullptr; ID2D1GeometrySink* sk = nullptr; ID2D1Layer* ly = nullptr;
        if (f0 && SUCCEEDED(f0->CreateRectangleGeometry(D2D1::RectF(0, 0, (float)CW, (float)CH), &all)) && SUCCEEDED(f0->CreateRoundedRectangleGeometry(D2D1::RoundedRect(card, rad, rad), &in)) &&
            SUCCEEDED(f0->CreatePathGeometry(&out)) && SUCCEEDED(out->Open(&sk)) && SUCCEEDED(all->CombineWithGeometry(in, D2D1_COMBINE_MODE_EXCLUDE, nullptr, sk)) && SUCCEEDED(sk->Close()) &&
            SUCCEEDED(rt->CreateLayer(&ly))) {
            rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), out), ly);
            p.Shadow(card, rad);
            rt->PopLayer();
        }
        if (ly) ly->Release(); if (sk) sk->Release(); if (out) out->Release(); if (in) in->Release(); if (all) all->Release(); if (f0) f0->Release();
    }

    // vidro: o pedaço do papel de parede atrás da caixa, desfocado
    ID2D1Factory* fac = nullptr; rt->GetFactory(&fac);
    ID2D1RoundedRectangleGeometry* geo = nullptr; ID2D1Layer* layer = nullptr;
    bool clipped = fac && SUCCEEDED(fac->CreateRoundedRectangleGeometry(D2D1::RoundedRect(card, rad, rad), &geo)) && SUCCEEDED(rt->CreateLayer(&layer));
    if (clipped) rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), geo), layer);
    // Com a chuva ligada, o que fica atrás do calendário é o fundo da chuva, não o arquivo do papel de parede:
    // desfocar o arquivo mostrava uma imagem que não está na tela e a opacidade parecia não mudar nada.
    // Nesse caso o vidro é um véu leve da cor da chuva e o que está atrás (chuva, WallClock) aparece.
    bool overRain = g_cfg.rainEnabled && rain::g_running;
    if (!overRain) WallCalLoadBlur(mi.rcMonitor);
    if (!overRain && !g_wcal.blur.empty()) {
        ID2D1Bitmap* b = nullptr;
        if (SUCCEEDED(rt->CreateBitmap(D2D1::SizeU(g_wcal.blurW, g_wcal.blurH), g_wcal.blur.data(), g_wcal.blurW * 4,
                                       D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &b))) {
            float cx = (float)(wr.left - mi.rcMonitor.left) + M, cy = (float)(wr.top - mi.rcMonitor.top) + M; // cartão no monitor (px)
            D2D1_RECT_F src = D2D1::RectF(cx / kWallCalBlurK, cy / kWallCalBlurK, (cx + W) / kWallCalBlurK, (cy + H) / kWallCalBlurK);
            rt->DrawBitmap(b, card, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, src);
            b->Release();
        }
    }
    p.Fill(card, 0, ui::Rgba(g_cfg.calBox, g_cfg.calOpacity / 100.0f));
    { // brilho do vidro: diagonal clara no alto, filetes de luz no topo e na lateral direita (sombra interna suave)
        D2D1_GRADIENT_STOP sheen[3] = {{0, ui::Rgba(0xFFFFFF, 0.10f)}, {0.45f, ui::Rgba(0xFFFFFF, 0.02f)}, {1, ui::Rgba(0xFFFFFF, 0)}}; // sem escurecer: a cor do box fica fiel
        if (auto* gb = p.Angle(card, 160, sheen, 3)) { rt->FillRectangle(card, gb); gb->Release(); }
        D2D1_GRADIENT_STOP topGlow[2] = {{0, ui::Rgba(0xFFFFFF, 0.16f)}, {1, ui::Rgba(0xFFFFFF, 0)}};
        D2D1_RECT_F tg = D2D1::RectF(card.left, card.top, card.right, card.top + 26 * s);
        if (auto* gb = p.Angle(tg, 180, topGlow, 2)) { rt->FillRectangle(tg, gb); gb->Release(); }
        D2D1_RECT_F rg = D2D1::RectF(card.right - 26 * s, card.top, card.right, card.bottom);
        if (auto* gb = p.Angle(rg, 270, topGlow, 2)) { rt->FillRectangle(rg, gb); gb->Release(); }
    }
    if (clipped) rt->PopLayer();
    if (layer) layer->Release(); if (geo) geo->Release(); if (fac) fac->Release();
    p.Fill(D2D1::RectF(card.left + rad, card.top, card.right - rad, card.top + 1 * s), 0, ui::Rgba(0xFFFFFF, 0.35f));
    p.Fill(D2D1::RectF(card.right - 1 * s, card.top + rad, card.right, card.bottom - rad), 0, ui::Rgba(0xFFFFFF, 0.22f));
    p.Stroke(card, rad, ui::Rgba(0xFFFFFF, 0.12f), 1 * s);

    // cabeçalho: "Novembro 2026", setas e "Hoje" (só fora do mês atual)
    SYSTEMTIME first = WallCalMonth(), today; GetLocalTime(&today);
    D2D1_COLOR_F num = ui::Rgba(g_cfg.calNum);
    float pad = 20 * s, x0 = card.left + pad, y0 = card.top + pad;
    std::wstring title = MonthName(first.wMonth);
    if (!title.empty()) CharUpperBuffW(&title[0], 1);
    title += L" " + std::to_wstring(first.wYear);
    p.Text(title, D2D1::RectF(x0, y0, x0 + 170 * s, y0 + 24 * s), 18, num, DWRITE_FONT_WEIGHT_MEDIUM);
    float bs = 28 * s, by = y0 - 2 * s, bx = card.right - pad - bs;
    g_wcal.rNext = D2D1::RectF(bx, by, bx + bs, by + bs); bx -= bs + 4 * s;
    g_wcal.rPrev = D2D1::RectF(bx, by, bx + bs, by + bs); bx -= 8 * s;
    g_wcal.rToday = D2D1::RectF(0, 0, 0, 0);
    for (int i = 0; i < 2; ++i) {
        D2D1_RECT_F r = i ? g_wcal.rNext : g_wcal.rPrev;
        p.Fill(r, bs / 2, ui::Rgba(0xFFFFFF, g_wcal.hot == 1 + i ? 0.16f : 0.06f));
        p.Feather(i ? "m9 18 6-6-6-6" : "m15 18-6-6 6-6", r, 14 * s, ui::Rgba(g_cfg.calNum, 0.8f), 2);
    }
    if (g_wcal.monthOff) {
        g_wcal.rToday = D2D1::RectF(bx - 52 * s, by, bx, by + bs);
        p.Fill(g_wcal.rToday, bs / 2, ui::Rgba(0xFFFFFF, g_wcal.hot == 3 ? 0.16f : 0.08f));
        p.Stroke(g_wcal.rToday, bs / 2, ui::Rgba(0xFFFFFF, 0.18f), 1 * s);
        p.Text(T(L"Hoje", L"Today", L"Hoy"), g_wcal.rToday, 12, ui::Rgba(g_cfg.calNum, 0.85f), DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    // grade: iniciais (domingo primeiro) e os dias; fim de semana mais apagado, hoje na cor de destaque
    float cw = 40 * s, ch = 32 * s, gy = y0 + 40 * s;
    for (int d = 0; d < 7; ++d)
        p.Text(WallCalWeekLetter(d), D2D1::RectF(x0 + d * cw, gy, x0 + (d + 1) * cw, gy + 18 * s), 11, ui::Rgba(g_cfg.calNum, d == 0 || d == 6 ? 0.32f : 0.45f),
               DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_TEXT_ALIGNMENT_CENTER);
    std::vector<int> marks(32, 0);
    for (auto& a : LoadAgenda()) if (a.when.wYear == first.wYear && a.when.wMonth == first.wMonth && a.when.wDay < 32) marks[a.when.wDay] = 1;
    int off = DayOfWeek(first), n = WallCalDaysIn(first);
    D2D1_COLOR_F acc = AccentOf(th), onAcc = OnAccentOf(th);
    g_wcal.days.clear();
    for (int d = 1; d <= n; ++d) {
        int idx = d - 1 + off, col = idx % 7, row = idx / 7;
        float cx = x0 + col * cw, cy = gy + 24 * s + row * ch, dd = 28 * s;
        D2D1_RECT_F c = D2D1::RectF(cx + (cw - dd) / 2, cy + (ch - dd) / 2, cx + (cw + dd) / 2, cy + (ch + dd) / 2);
        g_wcal.days.push_back({d, c});
        bool isToday = !g_wcal.monthOff && d == today.wDay, hv = g_wcal.hot == 100 + d;
        if (isToday) p.Fill(c, dd / 2, acc);
        else if (hv) p.Fill(c, dd / 2, ui::Rgba(0xFFFFFF, 0.12f));
        p.Text(std::to_wstring(d), c, 13, isToday ? onAcc : ui::Rgba(g_cfg.calNum, col == 0 || col == 6 ? 0.5f : 0.82f),
               isToday ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER);
        if (marks[d] && !isToday) p.Fill(D2D1::RectF((c.left + c.right) / 2 - 2 * s, c.bottom - 1 * s, (c.left + c.right) / 2 + 2 * s, c.bottom + 3 * s), 2 * s, ui::Rgba(g_cfg.calNum, 0.45f));
    }
    if (g_wcal.hot >= 10 && g_wcal.hot <= 13) { // canto sob o mouse: marca de redimensionar
        int k = g_wcal.hot - 10; // 10 sup-esq, 11 sup-dir, 12 inf-esq, 13 inf-dir
        float cx = k % 2 ? card.right : card.left, cy = k / 2 ? card.bottom : card.top, l = 14 * s, sx = k % 2 ? -1.0f : 1.0f, sy = k / 2 ? -1.0f : 1.0f;
        p.Fill(D2D1::RectF(min(cx, cx + sx * l), cy - 1 * s, max(cx, cx + sx * l), cy + 1 * s), 1 * s, ui::Rgba(0xFFFFFF, 0.9f));
        p.Fill(D2D1::RectF(cx - 1 * s, min(cy, cy + sy * l), cx + 1 * s, max(cy, cy + sy * l)), 1 * s, ui::Rgba(0xFFFFFF, 0.9f));
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

// Onde fica o cartão (canto superior esquerdo, tela). Sem posição salva: canto superior direito da área de trabalho.
static POINT WallCalCardPos() {
    MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
    UINT dx = 96, dy = 96; GetDpiForMonitor(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), MDT_EFFECTIVE_DPI, &dx, &dy);
    float s = dx / 96.0f;
    if (g_cfg.calX == LONG_MIN || g_cfg.calY == LONG_MIN || !MonitorFromPoint({g_cfg.calX + 20, g_cfg.calY + 20}, MONITOR_DEFAULTTONULL))
        return {mi.rcWork.right - (LONG)(48 * s) - (LONG)(320 * s * g_cfg.calScale / 100.0f), mi.rcWork.top + (LONG)(48 * s)};
    return {g_cfg.calX, g_cfg.calY};
}
static void WallCalPlace() {
    if (!g_wcal.wnd) return;
    POINT c = WallCalCardPos();
    UINT dx = 96, dy = 96; GetDpiForMonitor(MonitorFromPoint(c, MONITOR_DEFAULTTOPRIMARY), MDT_EFFECTIVE_DPI, &dx, &dy);
    float s = dx / 96.0f, k = s * g_cfg.calScale / 100.0f, M = 24 * s;
    SetWindowPos(g_wcal.wnd, HWND_BOTTOM, c.x - (int)M, c.y - (int)M, (int)(320 * k + 2 * M), (int)(296 * k + 2 * M), SWP_NOACTIVATE);
    WallCalRender();
}
static void WallCalSavePos() {
    if (!g_wcal.wnd) return;
    RECT wr; GetWindowRect(g_wcal.wnd, &wr);
    g_cfg.calX = wr.left + (LONG)g_wcal.M; g_cfg.calY = wr.top + (LONG)g_wcal.M;
    WriteDword(L"WallCalX", (DWORD)g_cfg.calX); WriteDword(L"WallCalY", (DWORD)g_cfg.calY); WriteDword(L"WallCalScale", g_cfg.calScale);
}

static int WallCalHit(POINT pt) {
    D2D1_RECT_F card = D2D1::RectF(g_wcal.M, g_wcal.M, g_wcal.M + g_wcal.W, g_wcal.M + g_wcal.H);
    float g = 14 * g_wcal.s / (g_cfg.calScale / 100.0f);
    for (int k = 0; k < 4; ++k) { // cantos (mesmo um pouco para fora do cartão)
        float cx = k % 2 ? card.right : card.left, cy = k / 2 ? card.bottom : card.top;
        if (fabsf(pt.x - cx) <= g && fabsf(pt.y - cy) <= g) return 10 + k;
    }
    auto in = [&](const D2D1_RECT_F& r) { return pt.x >= r.left && pt.x < r.right && pt.y >= r.top && pt.y < r.bottom; };
    if (pt.x < card.left || pt.x >= card.right || pt.y < card.top || pt.y >= card.bottom) return -1; // fora: a sombra não responde
    if (in(g_wcal.rPrev)) return 1;
    if (in(g_wcal.rNext)) return 2;
    if (in(g_wcal.rToday)) return 3;
    for (auto& d : g_wcal.days) if (in(d.second)) return 100 + d.first;
    return 0;
}

// Janela logo acima da qual o calendário deve ficar: a que está imediatamente acima da área de trabalho (a janela dos ícones,
// Progman ou o WorkerW com SHELLDLL_DefView). HWND_BOTTOM punha o calendário ABAIXO da área de trabalho e ele sumia
// assim que se clicava no fundo. Devolve h quando o calendário já está no lugar.
static HWND WallCalAfter(HWND h) {
    HWND host = nullptr;
    EnumWindows([](HWND w, LPARAM lp) -> BOOL {
        if (FindWindowExW(w, nullptr, L"SHELLDLL_DefView", nullptr)) { *(HWND*)lp = w; return FALSE; }
        return TRUE;
    }, (LPARAM)&host);
    if (!host) host = FindWindowW(L"Progman", nullptr);
    if (!host) return HWND_BOTTOM;
    HWND prev = GetWindow(host, GW_HWNDPREV);
    if (prev == h) return h;
    return prev ? prev : HWND_TOP; // nada acima da área de trabalho: fica no topo dessa pilha vazia
}

static LRESULT CALLBACK WallCalProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCHITTEST: { // a sombra em volta deixa o clique passar para a área de trabalho
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; ScreenToClient(h, &pt);
        return WallCalHit(pt) < 0 ? HTTRANSPARENT : HTCLIENT;
    }
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_WINDOWPOSCHANGING: { // sempre no fundo: logo acima da área de trabalho, abaixo das outras janelas
        auto* wp2 = (WINDOWPOS*)lp;
        HWND after = WallCalAfter(h);
        if (after == h) wp2->flags |= SWP_NOZORDER; // já está no lugar
        else { wp2->hwndInsertAfter = after; wp2->flags &= ~SWP_NOZORDER; }
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (g_wcal.resize) { // proporcional: a escala segue a diagonal entre o mouse e o canto oposto
            POINT sp = pt; ClientToScreen(h, &sp);
            float dx = (float)(sp.x - g_wcal.anchor.x), dy = (float)(sp.y - g_wcal.anchor.y), d = sqrtf(dx * dx + dy * dy);
            DWORD sc = (DWORD)max(60.0f, min(250.0f, g_wcal.startScale * d / max(1.0f, g_wcal.startDiag)));
            if (sc != g_cfg.calScale) {
                g_cfg.calScale = sc;
                UINT dpx = 96, dpy = 96; GetDpiForMonitor(MonitorFromWindow(h, MONITOR_DEFAULTTOPRIMARY), MDT_EFFECTIVE_DPI, &dpx, &dpy);
                float s0 = dpx / 96.0f, k = s0 * sc / 100.0f, M = 24 * s0, W = 320 * k, H = 296 * k;
                int c = g_wcal.resize - 10;
                float left = c % 2 ? g_wcal.anchor.x : g_wcal.anchor.x - W, top = c / 2 ? g_wcal.anchor.y : g_wcal.anchor.y - H;
                SetWindowPos(h, HWND_BOTTOM, (int)(left - M), (int)(top - M), (int)(W + 2 * M), (int)(H + 2 * M), SWP_NOACTIVATE);
                WallCalRender();
            }
            return 0;
        }
        int hot = WallCalHit(pt);
        if (hot != g_wcal.hot) { g_wcal.hot = hot; WallCalRender(); }
        TRACKMOUSEEVENT te = {sizeof(te), TME_LEAVE, h, 0}; TrackMouseEvent(&te);
        return 0;
    }
    case WM_MOUSELEAVE: if (!g_wcal.resize && g_wcal.hot) { g_wcal.hot = 0; WallCalRender(); } return 0;
    case WM_SETCURSOR: {
        int hc = g_wcal.resize ? g_wcal.resize : g_wcal.hot;
        LPCWSTR c = hc == 10 || hc == 13 ? IDC_SIZENWSE : hc == 11 || hc == 12 ? IDC_SIZENESW : hc > 0 ? IDC_HAND : IDC_SIZEALL;
        SetCursor(LoadCursor(nullptr, c));
        return TRUE;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int hit = WallCalHit(pt);
        if (hit >= 10 && hit <= 13) { // começa a redimensionar pelo canto; o oposto fica parado
            int c = hit - 10;
            RECT wr; GetWindowRect(h, &wr);
            float M = g_wcal.M;
            g_wcal.anchor = {c % 2 ? wr.left + (LONG)M : wr.right - (LONG)M, c / 2 ? wr.top + (LONG)M : wr.bottom - (LONG)M};
            g_wcal.startDiag = sqrtf(g_wcal.W * g_wcal.W + g_wcal.H * g_wcal.H);
            g_wcal.startScale = g_cfg.calScale;
            g_wcal.resize = hit;
            SetCapture(h);
            return 0;
        }
        if (hit == 1 || hit == 2) { g_wcal.monthOff += hit == 1 ? -1 : 1; WallCalRender(); return 0; }
        if (hit == 3) { g_wcal.monthOff = 0; WallCalRender(); return 0; }
        if (hit > 100) { // dia: abre a Agenda nele (ver e adicionar eventos)
            SYSTEMTIME d = WallCalMonth(); d.wDay = (WORD)(hit - 100);
            OpenAgendaWindow();
            g_ag.day = d; AgResetForm();
            if (g_ag.wnd) InvalidateRect(g_ag.wnd, nullptr, FALSE);
            return 0;
        }
        ReleaseCapture(); SendMessage(h, WM_NCLBUTTONDOWN, HTCAPTION, 0); // arrastar
        WallCalSavePos();
        return 0;
    }
    case WM_LBUTTONUP:
        if (g_wcal.resize) { g_wcal.resize = 0; ReleaseCapture(); WallCalSavePos(); WallCalRender(); }
        return 0;
    case WM_CAPTURECHANGED: if (g_wcal.resize) { g_wcal.resize = 0; WallCalSavePos(); } return 0;
    case WM_TIMER: if (wp == 2) { // "Mostrar área de trabalho" e cliques no fundo mexem na pilha: volta ao lugar
            if (!IsWindowVisible(h)) ShowWindow(h, SW_SHOWNOACTIVATE);
            HWND after = WallCalAfter(h);
            if (after != h) SetWindowPos(h, after, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            return 0;
        }
        { // troca de dia (meia-noite) e eventos novos da Agenda
        static WORD lastDay = 0;
        SYSTEMTIME t; GetLocalTime(&t);
        if (t.wDay != lastDay) { lastDay = t.wDay; g_wcal.monthOff = 0; }
        WallCalRender();
        return 0;
    }
    case WM_SETTINGCHANGE: case WM_DISPLAYCHANGE: case WM_DPICHANGED: g_wcal.blurPath.clear(); WallCalPlace(); return 0;
    case WM_DESTROY: if (g_wcal.wnd == h) g_wcal.wnd = nullptr; return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void WallCalEnable(bool on) {
    if (!on) { if (g_wcal.wnd) DestroyWindow(g_wcal.wnd); g_wcal.wnd = nullptr; return; }
    if (g_wcal.wnd) return;
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc = {}; wc.lpfnWndProc = WallCalProc; wc.hInstance = g_inst; wc.lpszClassName = L"VizuWallCalendar";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc); reg = true;
    }
    g_wcal.monthOff = 0; g_wcal.hot = 0; g_wcal.resize = 0;
    // sem dono e sem botão na barra de tarefas; WS_EX_NOACTIVATE: clicar não rouba o foco da janela em uso
    g_wcal.wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE, L"VizuWallCalendar", L"Vizu", WS_POPUP,
                                 0, 0, 1, 1, nullptr, nullptr, g_inst, nullptr);
    if (!g_wcal.wnd) { diag::Note("calendário no fundo", "CreateWindow", HRESULT_FROM_WIN32(GetLastError())); return; }
    WallCalPlace();
    ShowWindow(g_wcal.wnd, SW_SHOWNOACTIVATE);
    SetWindowPos(g_wcal.wnd, WallCalAfter(g_wcal.wnd), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetTimer(g_wcal.wnd, 1, 60000, nullptr);
    SetTimer(g_wcal.wnd, 2, 1000, nullptr);
}
