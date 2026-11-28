// RecordUi.inc.h - "Gravar tela": botão com bolinha vermelha na caixa do relógio (e atalho) abre uma barra no
// topo da tela com Gravar | Pausar | Parar, o tempo e as opções. O vídeo vai para a pasta Vídeos do usuário.
// A barra nunca aparece no vídeo (WDA_EXCLUDEFROMCAPTURE). Com "Esconder", a caixa do relógio, o cursor e a
// barra de tarefas também ficam de fora. Incluído no Vizu.cpp depois do OcrUi.inc.h (o motor fica em Record.h).

#include "Record.h"

static const UINT WM_REC_DONE = WM_APP + 90;
enum { kRecNone = 0, kRecGo, kRecPause, kRecStop, kRecSys, kRecMic, kRecHide, kRecClose, kRecCount };

struct RecUi {
    HWND wnd = nullptr;
    int state = 0;            // 0 parado, 1 gravando, 2 salvando
    bool sys = true, mic = false, hide = false;
    bool excluded = true;     // a barra ficou fora da captura (Windows 10 2004 ou mais novo)
    bool clockHidden = false; // marcamos a caixa do relógio para sair da captura
    int hot = 0;
    float s = 1;
    D2D1_RECT_F r[kRecCount]{};
    rec::Recorder* rc = nullptr;
};
static RecUi g_rec;

static void RecRender() {
    HWND w = g_rec.wnd;
    if (!w) return;
    ui::Theme th = g_theme;
    float s = GetDpiForWindow(w) / 96.0f;
    g_rec.s = s;
    bool going = g_rec.state == 1, paused = going && g_rec.rc && g_rec.rc->Paused(), saving = g_rec.state == 2;
    float H = 52 * s, M = 14 * s, gap = 8 * s, pad = 10 * s;

    // larguras pelo texto (os três idiomas cabem)
    auto measure = [&](const std::wstring& t, float size) {
        float wd = 0;
        IDWriteTextFormat* f = nullptr;
        if (SUCCEEDED(g_dw->CreateTextFormat(th.font, nullptr, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size * s, L"pt-br", &f))) {
            IDWriteTextLayout* l = nullptr; DWRITE_TEXT_METRICS m = {};
            if (SUCCEEDED(g_dw->CreateTextLayout(t.c_str(), (UINT32)t.size(), f, 4000, 100, &l))) { l->GetMetrics(&m); l->Release(); wd = m.widthIncludingTrailingWhitespace; }
            f->Release();
        }
        return wd;
    };
    std::wstring lGo = paused ? T(L"Continuar", L"Resume", L"Continuar") : T(L"Gravar", L"Record", L"Grabar");
    std::wstring lPause = T(L"Pausar", L"Pause", L"Pausar"), lStop = saving ? T(L"Salvando…", L"Saving…", L"Guardando…") : T(L"Parar", L"Stop", L"Detener");
    std::wstring lSys = T(L"Som do computador", L"Computer sound", L"Sonido del equipo"), lMic = T(L"Microfone", L"Microphone", L"Micrófono");
    std::wstring lHide = T(L"Esconder botões, cursor e menu Iniciar", L"Hide buttons, cursor and Start menu", L"Ocultar botones, cursor y menú Inicio");
    float fs = 13;
    float wGo = 38 * s + measure(lGo, fs), wPause = 36 * s + measure(lPause, fs), wStop = 36 * s + measure(lStop, fs);
    float wTime = measure(L"00:00:00", 15) + 26 * s;
    float wSys = 30 * s + measure(lSys, fs), wMic = 30 * s + measure(lMic, fs), wHide = 30 * s + measure(lHide, fs), wX = 32 * s;
    float Wc = pad + wGo + gap + wPause + gap + wStop + gap + wTime + gap + 1 * s + gap + wSys + gap + wMic + gap + wHide + (going || saving ? 0 : gap + wX) + pad;
    UINT W = (UINT)(Wc + 2 * M), Ht = (UINT)(H + 2 * M);
    fmt::Canvas cv(W, Ht);
    if (!cv.rt) return;
    ID2D1RenderTarget* rt = cv.rt;
    rt->Clear(D2D1::ColorF(0, 0));
    rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ui::Tokens tk = ui::MakeTokens(th);
    ui::Painter p(rt, g_dw, th, tk, s);
    D2D1_RECT_F card = D2D1::RectF(M, M, M + Wc, M + H);
    float rad = H / 2;
    if (!th.neon) p.Shadow(card, rad);
    if (th.neon) p.Card(card, rad);
    else p.Fill(card, rad, th.classic ? ui::Rgba(0x4A4F57) : th.dark ? ui::Rgba(0x161618, 0.96f) : ui::Rgba(0xFAFAFC, 0.97f));
    if (going && !paused) p.Stroke(card, rad, ui::Rgba(0xFF3B30, 0.85f), 1.2f * s);
    else if (!th.neon) p.Stroke(card, rad, th.stroke, 1 * s);

    D2D1_COLOR_F hotBg = th.dark || th.classic ? ui::Rgba(0xFFFFFF, 0.12f) : ui::Rgba(0x000000, 0.08f);
    D2D1_COLOR_F chipBg = th.dark || th.classic ? ui::Rgba(0xFFFFFF, 0.05f) : ui::Rgba(0x000000, 0.04f);
    D2D1_COLOR_F off = th.text2; off.a *= 0.45f;
    float x = card.left + pad, cy = (card.top + card.bottom) / 2, bh = 36 * s;
    for (auto& r : g_rec.r) r = D2D1::RectF(0, 0, 0, 0);
    auto place = [&](int id, float wd) { D2D1_RECT_F r = D2D1::RectF(x, cy - bh / 2, x + wd, cy + bh / 2); g_rec.r[id] = r; x += wd + gap; return r; };
    auto button = [&](int id, float wd, bool enabled, const std::wstring& label, int icon) { // icon: 0 bolinha, 1 pausa, 2 quadrado
        D2D1_RECT_F r = place(id, wd);
        if (!enabled) g_rec.r[id] = D2D1::RectF(0, 0, 0, 0);
        bool hv = enabled && g_rec.hot == id;
        p.Fill(r, bh / 2, hv ? hotBg : chipBg);
        D2D1_COLOR_F c = enabled ? th.text : off;
        D2D1_RECT_F ir = D2D1::RectF(r.left + 10 * s, cy - 9 * s, r.left + 28 * s, cy + 9 * s);
        if (icon == 0) { float k = 6 * s; p.Fill(D2D1::RectF((ir.left + ir.right) / 2 - k, cy - k, (ir.left + ir.right) / 2 + k, cy + k), k, enabled ? ui::Rgba(0xFF3B30) : off); }
        else p.FeatherIcon(icon == 1 ? "pause" : "square", ir, 15 * s, c);
        p.Text(label, D2D1::RectF(ir.right + 4 * s, r.top, r.right - 6 * s, r.bottom), fs, c, DWRITE_FONT_WEIGHT_MEDIUM);
    };
    button(kRecGo, wGo, !saving && (!going || paused), lGo, 0);
    button(kRecPause, wPause, going && !paused, lPause, 1);
    button(kRecStop, wStop, going, lStop, 2);
    { // tempo
        D2D1_RECT_F r = place(kRecNone, wTime);
        g_rec.r[kRecNone] = D2D1::RectF(0, 0, 0, 0);
        LONGLONG e = g_rec.rc ? g_rec.rc->Elapsed() / 10000000 : 0;
        wchar_t tb[16]; swprintf(tb, 16, L"%02d:%02d:%02d", (int)(e / 3600), (int)(e / 60 % 60), (int)(e % 60));
        bool blink = going && !paused && (GetTickCount64() / 500) % 2 == 0;
        float k = 4 * s;
        p.Fill(D2D1::RectF(r.left + 4 * s, cy - k, r.left + 4 * s + 2 * k, cy + k), k, going ? ui::Rgba(0xFF3B30, paused ? 0.4f : blink ? 1.0f : 0.35f) : off);
        p.Text(tb, D2D1::RectF(r.left + 18 * s, r.top, r.right, r.bottom), 15, going ? th.text : th.text2, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    }
    p.Fill(D2D1::RectF(x, cy - 14 * s, x + 1 * s, cy + 14 * s), 0, th.stroke);
    x += 1 * s + gap;
    auto check = [&](int id, float wd, bool on, const std::wstring& label) {
        bool enabled = g_rec.state == 0;
        D2D1_RECT_F r = place(id, wd);
        if (!enabled) g_rec.r[id] = D2D1::RectF(0, 0, 0, 0);
        if (enabled && g_rec.hot == id) p.Fill(r, 10 * s, hotBg);
        D2D1_RECT_F b = D2D1::RectF(r.left + 6 * s, cy - 8 * s, r.left + 22 * s, cy + 8 * s);
        D2D1_COLOR_F c = enabled ? th.text : th.text2;
        if (on) { p.Fill(b, 4 * s, enabled ? th.accent : th.text2); p.FeatherIcon("check", b, 12 * s, th.onAccent); }
        else p.Stroke(b, 4 * s, c, 1.4f * s);
        p.Text(label, D2D1::RectF(b.right + 6 * s, r.top, r.right, r.bottom), fs, c);
    };
    check(kRecSys, wSys, g_rec.sys, lSys);
    check(kRecMic, wMic, g_rec.mic, lMic);
    check(kRecHide, wHide, g_rec.hide, lHide);
    if (!going && !saving) {
        D2D1_RECT_F r = place(kRecClose, wX);
        r = D2D1::RectF(r.left, cy - wX / 2, r.right, cy + wX / 2);
        g_rec.r[kRecClose] = r;
        if (g_rec.hot == kRecClose) p.Fill(r, wX / 2, hotBg);
        p.FeatherIcon("x", r, 15 * s, th.text2);
    }

    HBITMAP bmp = nullptr;
    if (FAILED(cv.Finish(&bmp)) || !bmp) return;
    RECT wr; GetWindowRect(w, &wr);
    HDC screen = GetDC(nullptr), mem = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(mem, bmp);
    POINT pos = {wr.left + ((wr.right - wr.left) - (LONG)W) / 2, wr.top}, zero = {0, 0}; // cresce/encolhe pelo centro
    if (wr.right - wr.left <= 10) pos.x = wr.left - (LONG)W / 2; // primeira vez: a janela nasce com 1 px no centro
    SIZE size = {(LONG)W, (LONG)Ht};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(w, screen, &pos, &size, mem, &zero, 0, &bf, ULW_ALPHA);
    SelectObject(mem, old); DeleteDC(mem); ReleaseDC(nullptr, screen); DeleteObject(bmp);
}

static int RecHit(POINT pt) {
    for (int i = 1; i < kRecCount; ++i) {
        const D2D1_RECT_F& r = g_rec.r[i];
        if (pt.x >= r.left && pt.x < r.right && pt.y >= r.top && pt.y < r.bottom) return i;
    }
    return 0;
}

// A caixa do relógio sai (ou volta) da captura junto com a opção "Esconder".
static void RecHideClock(bool on) {
    if (!g_clock.wnd) { g_rec.clockHidden = false; return; }
    if (on == g_rec.clockHidden) return;
    SetWindowDisplayAffinity(g_clock.wnd, on ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
    g_rec.clockHidden = on;
}

static void RecStart() {
    if (g_rec.state == 1 && g_rec.rc && g_rec.rc->Paused()) { g_rec.rc->Pause(false); RecRender(); return; }
    if (g_rec.state) return;
    if (!g_rec.excluded && MessageBoxW(g_rec.wnd, T(L"Este Windows não deixa esconder a barra da gravação: ela vai aparecer no vídeo. Gravar assim mesmo?",
                                                     L"This Windows can't hide the recording bar: it will appear in the video. Record anyway?",
                                                     L"Este Windows no permite ocultar la barra de grabación: aparecerá en el video. ¿Grabar de todos modos?"),
                                       L"Vizu", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    if (g_rec.hide) RecHideClock(true);
    rec::Options o;
    o.mon = MonitorFromWindow(g_rec.wnd, MONITOR_DEFAULTTONEAREST);
    o.sysAudio = g_rec.sys; o.mic = g_rec.mic; o.hide = g_rec.hide;
    o.notify = g_rec.wnd; o.msg = WM_REC_DONE;
    if (!g_rec.rc) g_rec.rc = new rec::Recorder();
    std::wstring err;
    if (!g_rec.rc->Start(o, &err)) {
        RecHideClock(false);
        ShowToast(T(L"Não foi possível gravar", L"Couldn't record", L"No se pudo grabar"), err, 7000, 2);
        return;
    }
    g_rec.state = 1;
    SetTimer(g_rec.wnd, 1, 250, nullptr);
    RecRender();
}
static void RecStop() {
    if (g_rec.state != 1 || !g_rec.rc) return;
    g_rec.state = 2;
    g_rec.rc->Stop(); // o motor fecha o arquivo e avisa com WM_REC_DONE
    RecRender();
}
static void RecClose() {
    if (g_rec.state) return;
    if (g_rec.wnd) { KillTimer(g_rec.wnd, 1); DestroyWindow(g_rec.wnd); }
}

static LRESULT CALLBACK RecProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_TIMER: RecRender(); return 0;
    case WM_MOUSEMOVE: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int hot = RecHit(pt);
        if (hot != g_rec.hot) { g_rec.hot = hot; RecRender(); }
        TRACKMOUSEEVENT te = {sizeof(te), TME_LEAVE, h, 0}; TrackMouseEvent(&te);
        return 0;
    }
    case WM_MOUSELEAVE: g_rec.hot = 0; RecRender(); return 0;
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, g_rec.hot ? IDC_HAND : IDC_SIZEALL)); return TRUE;
    case WM_LBUTTONDOWN: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int hit = RecHit(pt);
        switch (hit) {
        case kRecNone: ReleaseCapture(); SendMessage(h, WM_NCLBUTTONDOWN, HTCAPTION, 0); return 0; // arrastar (até para outro monitor)
        case kRecGo: RecStart(); return 0;
        case kRecPause: if (g_rec.rc) g_rec.rc->Pause(true); break;
        case kRecStop: RecStop(); return 0;
        case kRecSys: g_rec.sys = !g_rec.sys; WriteDword(L"RecSystemAudio", g_rec.sys); break;
        case kRecMic: g_rec.mic = !g_rec.mic; WriteDword(L"RecMic", g_rec.mic); break;
        case kRecHide: g_rec.hide = !g_rec.hide; WriteDword(L"RecHide", g_rec.hide); break;
        case kRecClose: RecClose(); return 0;
        }
        RecRender();
        return 0;
    }
    case WM_REC_DONE: {
        KillTimer(h, 1);
        RecHideClock(false);
        g_rec.state = 0;
        rec::Recorder* r = g_rec.rc;
        r->Join();
        if (wp) {
            g_toastOutputs = {r->Path()};
            g_toastAction = 0;
            std::wstring text = PathFindFileNameW(r->Path().c_str());
            if (!r->Error().empty()) text = r->Error() + L" · " + text;
            else if (!r->Warning().empty()) text = r->Warning() + L"· " + text;
            ShowToast(T(L"Gravação salva · clique para ver na pasta", L"Recording saved · click to show in folder", L"Grabación guardada · clic para ver en la carpeta"), text, 8000, 1);
        } else ShowToast(T(L"A gravação não foi salva", L"The recording wasn't saved", L"La grabación no se guardó"), r->Error(), 8000, 2);
        delete r; g_rec.rc = nullptr;
        DestroyWindow(h);
        return 0;
    }
    case WM_DPICHANGED: case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: RecRender(); break;
    case WM_DESTROY: if (g_rec.wnd == h) g_rec.wnd = nullptr; return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// Abre a barra no topo, no meio do monitor do relógio (ou do mouse).
static void OpenRecordBar() {
    if (g_rec.wnd) { RecRender(); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc = {}; wc.lpfnWndProc = RecProc; wc.hInstance = g_inst; wc.lpszClassName = L"DesignThumbsRecord";
        RegisterClassW(&wc); reg = true;
    }
    g_rec.sys = ReadDword(L"RecSystemAudio", 1) != 0;
    g_rec.mic = ReadDword(L"RecMic", 0) != 0;
    g_rec.hide = ReadDword(L"RecHide", 0) != 0;
    g_rec.hot = 0;
    HMONITOR mon;
    if (g_clock.wnd && IsWindowVisible(g_clock.wnd)) mon = MonitorFromWindow(g_clock.wnd, MONITOR_DEFAULTTOPRIMARY);
    else { POINT pt; GetCursorPos(&pt); mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY); }
    MONITORINFO mi = {sizeof(mi), {}, {}, 0};
    GetMonitorInfoW(mon, &mi);
    int cx = (mi.rcWork.left + mi.rcWork.right) / 2;
    g_rec.wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE, L"DesignThumbsRecord", L"Vizu", WS_POPUP,
                                cx, mi.rcWork.top + 4, 1, 1, nullptr, nullptr, g_inst, nullptr);
    if (!g_rec.wnd) return;
    g_rec.excluded = SetWindowDisplayAffinity(g_rec.wnd, WDA_EXCLUDEFROMCAPTURE) != FALSE; // a barra nunca entra no vídeo
    RecRender();
    ShowWindow(g_rec.wnd, SW_SHOWNOACTIVATE);
}

// Botão da caixa e atalho: abre a barra; com ela aberta, grava; gravando, para e salva.
static void RecordShortcut() {
    if (!g_rec.wnd) OpenRecordBar();
    else if (g_rec.state == 0) RecStart();
    else if (g_rec.state == 1) RecStop();
}
// Ao sair do Vizu no meio da gravação: fecha o arquivo direito (MP4 sem Finalize não abre).
static void RecShutdown() {
    if (!g_rec.rc) return;
    g_rec.rc->Notify(nullptr);
    g_rec.rc->Stop(); g_rec.rc->Join();
    RecHideClock(false);
}
