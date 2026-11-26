// Inspect.inc.h - Configurações › Sistema › Avançado › "Inspeção do Vizu".
// Testa de verdade o Espaço (prévia), o Alt com arquivo selecionado (menu de converter), o Alt sem seleção
// (menu de atalhos) e as miniaturas de uma pasta escolhida. Gera um relatório em Markdown com um bloco JSON
// (fácil de ler por pessoas e por assistentes como Claude ou Codex) e abre um e-mail pronto para o suporte.
// Incluído em Vizu.cpp depois do menu circular, da prévia e do registro do teclado.
#include <tlhelp32.h>
#include <mapi.h>

static const wchar_t* kSupportMail = L"thiago.art@live.com";
static const UINT_PTR kInspectTimer = 0x7A20;

struct InspCheck { std::string id; int result; std::wstring detail; }; // result: 1 ok, 0 falhou, 2 não testado
struct InspThumb { std::wstring file, ext, handler; bool handlerVizu = false, previewOnly = false; HRESULT vizu = E_FAIL; int vizuWhite = -1, shellWhite = -1; DWORD ms = 0; std::wstring psd; };
static struct {
    bool running = false;
    int step = 0;
    double at = 0;          // início do passo atual
    HWND explorer = nullptr;
    std::wstring dir, testFile;
    std::vector<InspCheck> checks;
    std::vector<std::pair<std::wstring, std::wstring>> env; // ambiente: nome -> valor
    std::wstring keylogDuring;
} g_insp;

static void InspAdd(const char* id, int r, const std::wstring& d) { g_insp.checks.push_back({id, r, d}); }
static double InspElapsed() { return ui::NowMs() - g_insp.at; }
static void InspNext(int step) { g_insp.step = step; g_insp.at = ui::NowMs(); }

static void InspKey(WORD vk, bool up) {
    INPUT in = {}; in.type = INPUT_KEYBOARD; in.ki.wVk = vk; in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    SendInput(1, &in, sizeof(in));
}

// Fração de pixels brancos de um HBITMAP de 32 bits (-1 = não deu para ler).
static int WhitePercent(HBITMAP b) {
    DIBSECTION ds = {};
    if (!b || GetObject(b, sizeof(ds), &ds) != sizeof(ds) || !ds.dsBm.bmBits || ds.dsBm.bmBitsPixel != 32) {
        // bitmap que não é DIB: copia para um
        BITMAP bm = {}; if (!b || !GetObject(b, sizeof(bm), &bm)) return -1;
        BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = bm.bmWidth; bi.bmiHeader.biHeight = -bm.bmHeight;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
        std::vector<BYTE> px((size_t)bm.bmWidth * bm.bmHeight * 4);
        HDC dc = GetDC(nullptr); int got = GetDIBits(dc, b, 0, bm.bmHeight, px.data(), &bi, DIB_RGB_COLORS); ReleaseDC(nullptr, dc);
        if (!got) return -1;
        size_t n = (size_t)bm.bmWidth * bm.bmHeight, w = 0;
        for (size_t i = 0; i < n; ++i) if (px[i * 4] > 250 && px[i * 4 + 1] > 250 && px[i * 4 + 2] > 250) ++w;
        return n ? (int)(w * 100 / n) : -1;
    }
    size_t n = (size_t)ds.dsBm.bmWidth * std::abs(ds.dsBm.bmHeight), w = 0;
    const BYTE* px = (const BYTE*)ds.dsBm.bmBits;
    for (size_t i = 0; i < n; ++i) if (px[i * 4] > 250 && px[i * 4 + 1] > 250 && px[i * 4 + 2] > 250) ++w;
    return n ? (int)(w * 100 / n) : -1;
}

static std::wstring EffectiveHandler(const std::wstring& ext, const wchar_t* iid, bool* isVizu, const wchar_t* vizuClsid) {
    wchar_t eff[128] = {}; DWORD en = 128;
    HRESULT ah = AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_SHELLEXTENSION, ext.c_str(), iid, eff, &en);
    *isVizu = SUCCEEDED(ah) && _wcsicmp(eff, vizuClsid) == 0;
    if (FAILED(ah) || !eff[0]) return L"(nenhum)";
    std::wstring who = eff;
    wchar_t nm[256] = {}; DWORD nsz = sizeof(nm);
    RegGetValueW(HKEY_CLASSES_ROOT, (std::wstring(L"CLSID\\") + eff).c_str(), nullptr, RRF_RT_REG_SZ, nullptr, nm, &nsz);
    if (nm[0]) who += std::wstring(L" (") + nm + L")";
    return who;
}

// PSD pequeno (64x64, laranja, imagem pronta real) para os testes do Espaço e do Alt.
static bool WriteTestPsd(const std::wstring& path) {
    std::vector<BYTE> b = {'8', 'B', 'P', 'S', 0, 1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 64, 0, 0, 0, 64, 0, 8, 0, 3};
    for (int i = 0; i < 3; ++i) { b.push_back(0); b.push_back(0); b.push_back(0); b.push_back(0); }
    b.push_back(0); b.push_back(0); // sem compressão
    BYTE rgb[3] = {0xFF, 0x66, 0x33};
    for (int c = 0; c < 3; ++c) for (int i = 0; i < 64 * 64; ++i) b.push_back(rgb[c]);
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0; bool ok = WriteFile(f, b.data(), (DWORD)b.size(), &w, nullptr) && w == b.size();
    CloseHandle(f);
    return ok;
}

static std::wstring ProcessList(std::initializer_list<const wchar_t*> names) {
    std::wstring found;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return found;
    PROCESSENTRY32W pe = {sizeof(pe)};
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        for (auto n : names) if (_wcsicmp(pe.szExeFile, n) == 0 && found.find(n) == std::wstring::npos) found += (found.empty() ? L"" : L", ") + std::wstring(n);
    CloseHandle(snap);
    return found;
}

static std::wstring ReadTail(const std::wstring& path, int lines) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return L"";
    DWORD szHigh = 0, szLow = GetFileSize(f, &szHigh);
    ULONGLONG sz = ((ULONGLONG)szHigh << 32) | szLow;
    if (sz > 0xFFFFFFF0u) { CloseHandle(f); return L""; } // too large
    HANDLE map = CreateFileMappingW(f, nullptr, PAGE_READONLY, szHigh, szLow, nullptr);
    std::wstring w;
    if (map) {
        const BYTE* view = (const BYTE*)MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
        if (view) {
            int wn = MultiByteToWideChar(CP_UTF8, 0, (const char*)view, (int)sz, nullptr, 0);
            w.assign(wn, 0);
            MultiByteToWideChar(CP_UTF8, 0, (const char*)view, (int)sz, &w[0], wn);
            UnmapViewOfFile(view);
        }
        CloseHandle(map);
    }
    CloseHandle(f);
    size_t cut = w.size();
    for (int k = 0; k < lines && cut != std::wstring::npos && cut > 0; ++k) cut = w.rfind(L'\n', cut - 1);
    return w.substr(cut == std::wstring::npos ? 0 : cut + 1);
}

static std::string Utf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::string JsonStr(const std::wstring& w) {
    std::string s = Utf8(w), o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n"; else if (c == '\r') o += "\\r"; else if (c == '\t') o += "\\t";
        else if ((unsigned char)c < 0x20) { char b[8]; sprintf(b, "\\u%04x", c); o += b; }
        else o += c;
    }
    return o + "\"";
}

// Ambiente: versão, Windows, opções e o que pode atrapalhar o teclado.
static void InspEnvironment() {
    auto& e = g_insp.env;
    e.clear();
    e.push_back({L"vizu", upd::kVersion});
    e.push_back({L"edicao", vec::Edition() == vec::kLite ? L"Lite" : L"Vizu/Ultra"});
    {
        typedef LONG(WINAPI * RtlGetVersionT)(OSVERSIONINFOW*);
        OSVERSIONINFOW v = {sizeof(v)};
        if (auto f = (RtlGetVersionT)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")) f(&v);
        wchar_t b[64]; swprintf(b, 64, L"%lu.%lu build %lu", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
        e.push_back({L"windows", b});
    }
    e.push_back({L"idioma", i18n::Current() == i18n::kPt ? L"pt" : i18n::Current() == i18n::kEn ? L"en" : L"es"});
    e.push_back({L"tema", std::to_wstring(g_cfg.themeMode)});
    e.push_back({L"opcao_espaco", g_cfg.space ? L"ligado" : L"desligado"});
    e.push_back({L"opcao_alt", g_cfg.alt ? L"ligado" : L"desligado"});
    e.push_back({L"atraso_alt_ms", std::to_wstring(g_cfg.altDelay)});
    e.push_back({L"preset", g_preset == kPresetMinimal ? L"Minimal" : g_preset == kPresetDesign ? L"Design" : L"Custom"});
    e.push_back({L"iniciar_com_windows", AutostartEnabled() ? L"sim" : L"não"});
    e.push_back({L"eventos_de_foco", g_focusHook && g_fgHook ? L"ok" : L"falharam"});
    HANDLE tok = nullptr; TOKEN_ELEVATION el = {}; DWORD n = 0;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { GetTokenInformation(tok, TokenElevation, &el, sizeof(el), &n); CloseHandle(tok); }
    e.push_back({L"vizu_como_admin", el.TokenIsElevated ? L"sim" : L"não"});
    // Espaço livre? Se o Vizu não o tem agora, tenta reservar e soltar na hora.
    std::wstring space = g_spaceKey ? L"reservado pelo Vizu agora" : L"";
    if (!g_spaceKey) {
        if (RegisterHotKey(g_wnd, 0x7A21, MOD_NOREPEAT, VK_SPACE)) { UnregisterHotKey(g_wnd, 0x7A21); space = L"livre"; }
        else { wchar_t b[64]; swprintf(b, 64, L"OCUPADO por outro programa (erro %lu)", GetLastError()); space = b; }
    }
    e.push_back({L"tecla_espaco", space});
    std::wstring rivals = ProcessList({L"QuickLook.exe", L"Seer.exe", L"PowerToys.Peek.UI.exe", L"WinQuickLook.exe", L"Icaros_Host.exe", L"Explorer++.exe", L"Files.exe",
                                       L"dopus.exe", L"XYplorer.exe", L"TotalCMD64.exe", L"AutoHotkey.exe", L"AutoHotkey64.exe"});
    e.push_back({L"programas_que_podem_interferir", rivals.empty() ? L"nenhum encontrado" : rivals});
    int ringSet = 0; for (int i = 0; i < kRingApps; ++i) if (!ReadStr(RingAppName(i).c_str(), L"").empty()) ++ringSet;
    e.push_back({L"atalhos_menu_radial_definidos", std::to_wstring(ringSet) + L" de " + std::to_wstring(kRingApps)});
}

// Solta o Alt sem ativar o menu do Explorer: uma tecla neutra (F24) no meio cancela a ativação.
static void InspAltUp() { InspKey(VK_F24, false); InspKey(VK_F24, true); InspKey(VK_MENU, true); }
static void InspFinish();
static void InspThumbsAndReport();

// Passos do teste ao vivo (timer de 100 ms na janela principal).
static void InspectTick() {
    if (!g_insp.running) { KillTimer(g_wnd, kInspectTimer); return; }
    switch (g_insp.step) {
    case 1: { // esperando a janela do Explorer com o arquivo de teste em foco
        HWND fg = GetForegroundWindow();
        HWND list = FocusedFileList();
        if (list && fg && ClassOf(fg) != L"VizuSettings") {
            g_insp.explorer = fg;
            InspAdd("explorer_lista_em_foco", 1, L"janela " + ClassOf(fg) + L", lista de arquivos reconhecida");
            { // o Vizu consegue ler a seleção? (Espaço e Alt dependem disso)
                std::vector<std::wstring> sel = SelectedFiles(list);
                bool hasTest = std::find(sel.begin(), sel.end(), g_insp.testFile) != sel.end();
                InspAdd("explorer_selecao", hasTest ? 1 : 0, std::to_wstring(sel.size()) + L" arquivo(s) selecionado(s) lido(s)" + (hasTest ? L", inclui o de teste" : L", o de teste NÃO") + L" · " + g_viewProbe);
            }
            UpdateHotkeys();
            InspNext(2);
        } else if (InspElapsed() > 9000) {
            HWND f2 = GetForegroundWindow();
            InspAdd("explorer_lista_em_foco", 0, L"a lista de arquivos do Explorer não ficou em foco em 9 s (janela em foco: " + (f2 ? ClassOf(f2) : std::wstring(L"nenhuma")) + L")");
            InspAdd("espaco_previa", 2, L"não testado: o Explorer não ficou em foco");
            InspAdd("alt_menu_converter", 2, L"não testado: o Explorer não ficou em foco");
            InspAdd("alt_menu_atalhos", 2, L"não testado: o Explorer não ficou em foco");
            InspFinish();
        }
        return;
    }
    case 2: // Espaço
        if (InspElapsed() < 600) return; // deixa a seleção assentar
        if (!g_cfg.space) { InspAdd("espaco_previa", 2, L"não testado: \"Ver com a barra de espaço\" está desligado nas Configurações"); InspNext(4); return; }
        InspKey(VK_SPACE, false); InspKey(VK_SPACE, true);
        InspNext(3);
        return;
    case 3:
        if (g_open) {
            InspAdd("espaco_previa", 1, L"a prévia abriu " + std::to_wstring((int)InspElapsed()) + L" ms depois do Espaço");
            ClosePreview();
            InspNext(4);
        } else if (InspElapsed() > 3000) {
            InspAdd("espaco_previa", 0, std::wstring(L"a prévia não abriu em 3 s; Espaço reservado pelo Vizu no momento: ") + (g_spaceKey ? L"sim" : L"NÃO") +
                                         L"; lista em foco: " + (FocusedFileList() ? L"sim" : L"NÃO"));
            InspNext(4);
        }
        return;
    case 4: // Alt com o arquivo selecionado: menu de converter
        if (InspElapsed() < 500) return;
        if (!g_cfg.alt) { InspAdd("alt_menu_converter", 2, L"não testado: o menu do Alt está desligado nas Configurações (ou edição Lite)"); InspAdd("alt_menu_atalhos", 2, L"não testado: menu do Alt desligado"); InspFinish(); return; }
        InspKey(VK_MENU, false);
        InspNext(5);
        return;
    case 5:
        if (IsWindowVisible(g_altWnd) && !g_ringClosing) {
            InspAdd("alt_menu_converter", g_ringLaunch ? 0 : 1, g_ringLaunch ? L"abriu o menu de ATALHOS em vez do de converter: o Vizu não viu o arquivo selecionado"
                                                                        : L"o menu de converter abriu " + std::to_wstring((int)InspElapsed()) + L" ms depois de segurar Alt");
            InspAltUp();
            HideAltPopup();
            InspNext(6);
        } else if (InspElapsed() > g_cfg.altDelay + 2500) {
            InspAltUp();
            InspAdd("alt_menu_converter", 0, L"o menu não abriu em " + std::to_wstring(g_cfg.altDelay + 2500) + L" ms segurando Alt; lista em foco: " + (FocusedFileList() ? L"sim" : L"NÃO"));
            InspNext(6);
        }
        return;
    case 6: { // Alt sem nada selecionado: menu de atalhos
        if (InspElapsed() < 700) return;
        if (HWND list = FocusedFileList()) if (IShellView* sv = ShellViewFor(list)) { sv->SelectItem(nullptr, SVSI_DESELECTOTHERS); sv->Release(); }
        InspNext(7);
        return;
    }
    case 7:
        if (InspElapsed() < 500) return;
        InspKey(VK_MENU, false);
        InspNext(8);
        return;
    case 8:
        if (IsWindowVisible(g_altWnd) && !g_ringClosing) {
            InspAdd("alt_menu_atalhos", g_ringLaunch ? 1 : 0, g_ringLaunch ? L"o menu de atalhos abriu " + std::to_wstring((int)InspElapsed()) + L" ms depois de segurar Alt"
                                                                       : L"abriu o menu de CONVERTER, mas nada estava selecionado");
            InspAltUp();
            HideAltPopup();
            InspFinish();
        } else if (InspElapsed() > g_cfg.altDelay + 2500) {
            InspAltUp();
            InspAdd("alt_menu_atalhos", 0, L"o menu de atalhos não abriu segurando Alt sem seleção; lista em foco: " + std::wstring(FocusedFileList() ? L"sim" : L"NÃO"));
            InspFinish();
        }
        return;
    }
}

// Fim do teste ao vivo: fecha o Explorer de teste e passa às miniaturas.
static void InspFinish() {
    KillTimer(g_wnd, kInspectTimer);
    g_insp.keylogDuring = ReadTail(KeyLogPath(), 25);
    if (g_insp.explorer && IsWindow(g_insp.explorer) && ClassOf(g_insp.explorer) == L"CabinetWClass") PostMessageW(g_insp.explorer, WM_CLOSE, 0, 0);
    g_insp.step = 0;
    InspThumbsAndReport();
}

// Começa a inspeção (botão nas Configurações).
static void StartInspection() {
    if (g_insp.running) return;
    if (MessageBoxW(g_settingsWnd,
                    T(L"A inspeção vai:\n\n1. Abrir uma pasta de teste no Explorer e simular o Espaço e o Alt (uns 10 segundos). Não mexa no mouse nem no teclado nesse tempo.\n2. Pedir uma pasta para conferir as miniaturas (escolha a pasta onde elas aparecem em branco).\n3. Gerar um relatório e abrir um e-mail pronto para o suporte. Você confere e decide se envia.\n\nO relatório leva os nomes dos arquivos da pasta escolhida.",
                      L"The inspection will:\n\n1. Open a test folder in Explorer and simulate Space and Alt (about 10 seconds). Don't touch the mouse or keyboard meanwhile.\n2. Ask for a folder to check thumbnails (pick the one where they show up blank).\n3. Create a report and open an e-mail ready for support. You review it and decide whether to send.\n\nThe report includes the file names of the chosen folder.",
                      L"La inspección va a:\n\n1. Abrir una carpeta de prueba en el Explorador y simular Espacio y Alt (unos 10 segundos). No toques el mouse ni el teclado mientras tanto.\n2. Pedir una carpeta para revisar las miniaturas (elige la carpeta donde salen en blanco).\n3. Crear un informe y abrir un correo listo para soporte. Tú lo revisas y decides si lo envías.\n\nEl informe incluye los nombres de los archivos de la carpeta elegida."),
                    T(L"Inspeção do Vizu", L"Vizu inspection", L"Inspección de Vizu"), MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) return;
    g_insp.running = true;
    g_insp.checks.clear();
    g_insp.explorer = nullptr;
    InspEnvironment();
    if (g_settingsWnd) DestroyWindow(g_settingsWnd); // não pode ficar na frente do Explorer
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    g_insp.dir = std::wstring(tmp) + L"VizuInspecao";
    CreateDirectoryW(g_insp.dir.c_str(), nullptr);
    g_insp.testFile = g_insp.dir + L"\\teste-vizu.psd";
    if (!WriteTestPsd(g_insp.testFile)) {
        InspAdd("arquivo_de_teste", 0, L"não deu para criar " + g_insp.testFile);
        InspFinish(); return;
    }
    // miniatura do arquivo de teste pelo código do Vizu (sem depender do Explorer)
    {
        IStream* s = nullptr; HBITMAP b = nullptr; bool a = false;
        HRESULT hr = SUCCEEDED(SHCreateStreamOnFileEx(g_insp.testFile.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, 0, FALSE, nullptr, &s)) ? fmt::Render(s, L".psd", 256, 0, &b, &a) : E_FAIL;
        if (s) s->Release();
        int wp = WhitePercent(b); if (b) DeleteObject(b);
        InspAdd("leitor_psd_do_vizu", SUCCEEDED(hr) && wp < 50 ? 1 : 0, L"PSD de teste: resultado 0x" + [&] { wchar_t x[16]; swprintf(x, 16, L"%08X", (unsigned)hr); return std::wstring(x); }() + L", pixels brancos " + std::to_wstring(wp) + L"%");
    }
    ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"/select,\"" + g_insp.testFile + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
    InspNext(1);
    SetTimer(g_wnd, kInspectTimer, 100, nullptr);
}

// Pasta escolhida: compara a miniatura do Vizu com a que o Explorer entrega (inclui o cache do Windows).
static std::vector<InspThumb> InspScanFolder(const std::wstring& dir) {
    std::vector<InspThumb> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    const wchar_t* vizuThumb = L"{9E85547E-13C6-4C5E-95CC-990C74D9BB3F}";
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring path = dir + L"\\" + fd.cFileName, ext = conv::ExtOf(path);
        if (!fmt::Handles(ext)) continue;
        InspThumb t; t.file = fd.cFileName; t.ext = ext;
        t.handler = EffectiveHandler(ext, L"{e357fccd-a995-4576-b01f-234630154e96}", &t.handlerVizu, vizuThumb);
        for (auto po : fmt::kPreviewOnly) if (_wcsicmp(po, ext.c_str()) == 0) t.previewOnly = true;
        if (t.previewOnly) t.psd = L"o Vizu só faz o painel e a prévia deste tipo; a miniatura é do Windows ou de outro programa";
        else if (!t.handlerVizu && t.handler.find(L"CDR Thumbnail") != std::wstring::npos) t.psd = L"miniatura do próprio CorelDRAW instalado";
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS)) t.psd = L"arquivo só na nuvem (não baixado)";
        DWORD t0 = GetTickCount();
        IStream* s = nullptr; HBITMAP b = nullptr; bool a = false;
        if (SUCCEEDED(SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, 0, FALSE, nullptr, &s))) {
            t.vizu = fmt::Render(s, ext, 256, 0, &b, &a);
            t.vizuWhite = WhitePercent(b); if (b) DeleteObject(b);
            fmt::PsdHeader ph;
            if (fmt::ReadPsdHeader(s, &ph)) {
                wchar_t x[160];
                swprintf(x, 160, L"%ls %ux%u, %u bits, modo %u, %u canais, imagem pronta real: %ls", ph.psb ? L"PSB" : L"PSD", ph.w, ph.h, ph.depth, ph.mode, ph.channels, fmt::PsdHasRealComposite(s) ? L"sim" : L"NÃO");
                t.psd = x;
            }
            s->Release();
        }
        t.ms = GetTickCount() - t0;
        IShellItemImageFactory* f = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&f)))) {
            HBITMAP sb = nullptr;
            if (SUCCEEDED(f->GetImage({256, 256}, SIIGBF_THUMBNAILONLY, &sb)) && sb) { t.shellWhite = WhitePercent(sb); DeleteObject(sb); }
            f->Release();
        }
        out.push_back(t);
    } while (out.size() < 300 && FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

static std::wstring PickFolder() {
    std::wstring r;
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return r;
    DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    d->SetTitle(T(L"Pasta para conferir as miniaturas (ou Cancelar para pular)", L"Folder to check thumbnails (or Cancel to skip)", L"Carpeta para revisar miniaturas (o Cancelar para omitir)"));
    SetForegroundWindow(g_wnd);
    if (SUCCEEDED(d->Show(nullptr))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(d->GetResult(&it))) { r = ItemPath(it); it->Release(); }
    }
    d->Release();
    return r;
}

// Abre o e-mail: MAPI (Outlook clássico, Thunderbird...) com o relatório anexado; senão mailto com o resumo.
static void InspMail(const std::wstring& report, const std::wstring& summary) {
    std::wstring subject = std::wstring(L"Vizu ") + upd::kVersion + L" - " + T(L"relatório de inspeção", L"inspection report", L"informe de inspección");
    std::wstring body = std::wstring(T(L"Relatório da inspeção do Vizu em anexo.\r\n\r\n", L"Vizu inspection report attached.\r\n\r\n", L"Informe de inspección de Vizu adjunto.\r\n\r\n")) + summary;
    // estruturas Unicode do MAPI (o mapi.h do MinGW só traz as ANSI); mesmo layout do SDK do Windows
    struct VMapiRecipW { ULONG ulReserved, ulRecipClass; PWSTR lpszName, lpszAddress; ULONG ulEIDSize; LPVOID lpEntryID; };
    struct VMapiFileW { ULONG ulReserved, flFlags, nPosition; PWSTR lpszPathName, lpszFileName; LPVOID lpFileType; };
    struct VMapiMessageW { ULONG ulReserved; PWSTR lpszSubject, lpszNoteText, lpszMessageType, lpszDateReceived, lpszConversationID; FLAGS flFlags;
                           VMapiRecipW* lpOriginator; ULONG nRecipCount; VMapiRecipW* lpRecips; ULONG nFileCount; VMapiFileW* lpFiles; };
    typedef ULONG(WINAPI * SendW)(LHANDLE, ULONG_PTR, VMapiMessageW*, FLAGS, ULONG);
    HMODULE m = LoadLibraryW(L"mapi32.dll");
    SendW send = m ? (SendW)(void*)GetProcAddress(m, "MAPISendMailW") : nullptr;
    if (send) {
        std::wstring to = std::wstring(L"SMTP:") + kSupportMail;
        VMapiRecipW rcpt = {}; rcpt.ulRecipClass = MAPI_TO; rcpt.lpszName = (PWSTR)kSupportMail; rcpt.lpszAddress = (PWSTR)to.c_str();
        size_t slash = report.find_last_of(L'\\');
        std::wstring fname = report.substr(slash + 1);
        VMapiFileW file = {}; file.nPosition = (ULONG)-1; file.lpszPathName = (PWSTR)report.c_str(); file.lpszFileName = (PWSTR)fname.c_str();
        VMapiMessageW msg = {}; msg.lpszSubject = (PWSTR)subject.c_str(); msg.lpszNoteText = (PWSTR)body.c_str();
        msg.nRecipCount = 1; msg.lpRecips = &rcpt; msg.nFileCount = 1; msg.lpFiles = &file;
        ULONG r = send(0, (ULONG_PTR)g_wnd, &msg, MAPI_DIALOG | MAPI_LOGON_UI, 0);
        if (r == SUCCESS_SUCCESS || r == MAPI_USER_ABORT) return;
    }
    // sem cliente MAPI: e-mail com o resumo no corpo e a pasta aberta com o relatório selecionado para anexar
    auto enc = [](const std::wstring& w) {
        std::string u = Utf8(w), o;
        for (unsigned char c : u) { if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c; else { char b[4]; sprintf(b, "%%%02X", c); o += b; } }
        return std::wstring(o.begin(), o.end());
    };
    std::wstring shortBody = body.size() > 1500 ? body.substr(0, 1500) + L"…" : body;
    shortBody += std::wstring(T(L"\r\n\r\nAnexe o arquivo: ", L"\r\n\r\nAttach the file: ", L"\r\n\r\nAdjunta el archivo: ")) + report;
    std::wstring url = std::wstring(L"mailto:") + kSupportMail + L"?subject=" + enc(subject) + L"&body=" + enc(shortBody);
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"/select,\"" + report + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
}

// Miniaturas da pasta escolhida + relatório (Markdown com bloco JSON) + e-mail.
static void InspFunctional();
static void InspThumbsAndReport() {
    InspFunctional();
    std::wstring folder = PickFolder();
    std::vector<InspThumb> thumbs;
    if (!folder.empty()) {
        ShowToast(T(L"Inspeção do Vizu", L"Vizu inspection", L"Inspección de Vizu"), T(L"Conferindo as miniaturas…", L"Checking thumbnails…", L"Revisando miniaturas…"), 0, 0);
        thumbs = InspScanFolder(folder);
        HideToast(g_toastWnd);
    }
    int blankVizu = 0, blankShell = 0, failVizu = 0, notOurs = 0;
    for (auto& t : thumbs) {
        if (FAILED(t.vizu)) ++failVizu; else if (t.vizuWhite >= 97) ++blankVizu;
        if (!t.previewOnly && (t.shellWhite >= 97 || t.shellWhite < 0)) ++blankShell;
        if (!t.handlerVizu && !t.previewOnly) ++notOurs;
    }
    if (folder.empty()) InspAdd("miniaturas", 2, L"não testado: nenhuma pasta escolhida");
    else InspAdd("miniaturas", failVizu || blankVizu || blankShell || notOurs ? 0 : 1,
                 std::to_wstring(thumbs.size()) + L" arquivos; Vizu falhou em " + std::to_wstring(failVizu) + L", Vizu em branco em " + std::to_wstring(blankVizu) +
                 L", Explorer em branco ou sem miniatura em " + std::to_wstring(blankShell) + L", gerador de miniaturas não é o do Vizu em " + std::to_wstring(notOurs));

    // tabela de geradores de miniatura por extensão (só as que não são do Vizu)
    std::wstring foreign;
    const wchar_t* vizuThumb = L"{9E85547E-13C6-4C5E-95CC-990C74D9BB3F}";
    for (auto ext : fmt::kExtensions) {
        bool ours = false; std::wstring h = EffectiveHandler(ext, L"{e357fccd-a995-4576-b01f-234630154e96}", &ours, vizuThumb);
        if (!ours) foreign += std::wstring(foreign.empty() ? L"" : L", ") + ext + L" → " + h;
    }

    // ---- relatório ----
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t stamp[64]; swprintf(stamp, 64, L"%04d-%02d-%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    wchar_t la[MAX_PATH]; GetEnvironmentVariableW(L"LOCALAPPDATA", la, MAX_PATH);
    std::wstring dir = std::wstring(la) + L"\\Vizu"; CreateDirectoryW(dir.c_str(), nullptr);
    dir += L"\\Inspecao"; CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring path = dir + L"\\vizu-inspecao-" + stamp + L".md";

    std::wstring md, summary;
    auto mark = [](int r) { return r == 1 ? L"OK" : r == 0 ? L"FALHOU" : L"NÃO TESTADO"; };
    md += L"# Relatório de inspeção do Vizu\n\n";
    md += L"Gerado em " + std::wstring(stamp) + L" pelo botão Configurações › Sistema › Avançado › Inspeção do Vizu.\n\n";
    md += L"Para quem for ler (pessoa, Claude ou Codex): o Vizu é um programa Win32/Direct2D (repositório thiagohcrispim-web/vizu). "
          L"O Espaço abre a prévia quando a lista de arquivos do Explorer está em foco (RegisterHotKey só nesse momento); "
          L"o Alt segurado abre o menu radial: com arquivo selecionado, converter; sem seleção, atalhos de programas. "
          L"As miniaturas vêm do DesignThumbs.dll (CLSID {9E85547E-13C6-4C5E-95CC-990C74D9BB3F}) registrado em HKCU\\Software\\Classes\\<ext>\\ShellEx. "
          L"Os dados completos estão no bloco JSON no fim.\n\n";
    md += L"## Resultado\n\n| Teste | Resultado | Detalhe |\n|---|---|---|\n";
    for (auto& c : g_insp.checks) {
        md += L"| " + std::wstring(c.id.begin(), c.id.end()) + L" | " + mark(c.result) + L" | " + c.detail + L" |\n";
        summary += std::wstring(c.id.begin(), c.id.end()) + L": " + mark(c.result) + L" - " + c.detail + L"\r\n";
    }
    md += L"\n## Ambiente\n\n";
    for (auto& e : g_insp.env) md += L"- **" + e.first + L"**: " + e.second + L"\n";
    md += L"\n## Geradores de miniatura que não são do Vizu\n\n" + (foreign.empty() ? std::wstring(L"Nenhum: todas as extensões usam o Vizu.") : foreign) + L"\n";
    if (!thumbs.empty()) {
        md += L"\n## Miniaturas da pasta escolhida\n\nPasta: `" + folder + L"`\n\n| Arquivo | Gerador é do Vizu | Vizu (resultado, % branco, ms) | Explorer (% branco) | Observação |\n|---|---|---|---|---|\n";
        for (auto& t : thumbs) {
            wchar_t hr[16]; swprintf(hr, 16, L"0x%08X", (unsigned)t.vizu);
            md += L"| " + t.file + L" | " + (t.handlerVizu ? L"sim" : L"NÃO: " + t.handler) + L" | " + hr + L", " + std::to_wstring(t.vizuWhite) + L"%, " + std::to_wstring(t.ms) +
                  L" | " + (t.shellWhite < 0 ? std::wstring(L"sem miniatura") : std::to_wstring(t.shellWhite) + L"%") + L" | " + t.psd + L" |\n";
        }
        md += L"\nLeitura: Vizu bom e Explorer em branco = miniatura antiga no cache do Windows (rodar a Limpeza com \"Miniaturas do Windows\") ou outro gerador de miniaturas; "
              L"Vizu em branco também = o próprio arquivo não traz imagem utilizável ou falta suporte no leitor.\n";
    }
    md += L"\n## Registro do teclado (últimas linhas de %TEMP%\\Vizu-teclado.log)\n\n```\n" + g_insp.keylogDuring + L"\n```\n";
    std::wstring errLog = ReadTail(diag::Dir() + L"\\erros.log", 30);
    md += L"\n## Registro de erros (últimas linhas de %LOCALAPPDATA%\\DesignThumbs\\erros.log)\n\n```\n" + errLog + L"\n```\n";
    // JSON
    std::string js = "{\n  \"report\": \"vizu-inspection\",\n  \"version\": 1,\n  \"generated\": " + JsonStr(stamp) + ",\n  \"environment\": {";
    for (size_t i = 0; i < g_insp.env.size(); ++i) js += std::string(i ? ", " : "") + JsonStr(g_insp.env[i].first) + ": " + JsonStr(g_insp.env[i].second);
    js += "},\n  \"checks\": [";
    for (size_t i = 0; i < g_insp.checks.size(); ++i) {
        auto& c = g_insp.checks[i];
        js += std::string(i ? ",\n    " : "\n    ") + "{\"id\": \"" + c.id + "\", \"result\": \"" + (c.result == 1 ? "ok" : c.result == 0 ? "fail" : "skipped") + "\", \"detail\": " + JsonStr(c.detail) + "}";
    }
    js += "\n  ],\n  \"foreign_thumbnail_handlers\": " + JsonStr(foreign) + ",\n  \"thumbnail_folder\": " + JsonStr(folder) + ",\n  \"thumbnails\": [";
    for (size_t i = 0; i < thumbs.size(); ++i) {
        auto& t = thumbs[i];
        char hr[16]; sprintf(hr, "0x%08X", (unsigned)t.vizu);
        js += std::string(i ? ",\n    " : "\n    ") + "{\"file\": " + JsonStr(t.file) + ", \"handler_is_vizu\": " + (t.handlerVizu ? "true" : "false") + ", \"handler\": " + JsonStr(t.handler) +
              ", \"vizu_hresult\": \"" + hr + "\", \"vizu_white_percent\": " + std::to_string(t.vizuWhite) + ", \"vizu_ms\": " + std::to_string(t.ms) +
              ", \"explorer_white_percent\": " + std::to_string(t.shellWhite) + ", \"note\": " + JsonStr(t.psd) + "}";
    }
    js += "\n  ],\n  \"key_log\": " + JsonStr(g_insp.keylogDuring) + ",\n  \"error_log\": " + JsonStr(ReadTail(diag::Dir() + L"\\erros.log", 30)) + "\n}\n";
    std::string out = "\xEF\xBB\xBF" + Utf8(md) + "\n## Dados (JSON)\n\n```json\n" + js + "```\n";
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(f, out.data(), (DWORD)out.size(), &w, nullptr); CloseHandle(f); }

    // limpa a pasta de teste
    DeleteFileW(g_insp.testFile.c_str()); RemoveDirectoryW(g_insp.dir.c_str());
    g_insp.running = false;

    int fails = 0; for (auto& c : g_insp.checks) if (c.result == 0) ++fails;
    std::wstring msg = std::wstring(fails ? T(L"A inspeção encontrou problemas:\n\n", L"The inspection found problems:\n\n", L"La inspección encontró problemas:\n\n")
                                          : T(L"Tudo certo na inspeção:\n\n", L"Everything passed:\n\n", L"Todo bien en la inspección:\n\n")) + summary +
                       T(L"\nRelatório salvo em:\n", L"\nReport saved at:\n", L"\nInforme guardado en:\n") + path +
                       T(L"\n\nAbrir um e-mail para o suporte com o relatório?", L"\n\nOpen an e-mail to support with the report?", L"\n\n¿Abrir un correo a soporte con el informe?");
    if (MessageBoxW(nullptr, msg.c_str(), T(L"Inspeção do Vizu", L"Vizu inspection", L"Inspección de Vizu"), MB_YESNO | (fails ? MB_ICONWARNING : MB_ICONINFORMATION) | MB_SETFOREGROUND) == IDYES)
        InspMail(path, summary);
}

// Testes de função que não precisam de clique (rodam no fim da inspeção). Nada é apagado fora da pasta de teste.
static void InspFunctional() {
    // 1) autoteste de desenho, registro, versão e fonte (o mesmo da abertura)
    {
        std::string st = RunSelfTest();
        std::wstring w(st.begin(), st.end());
        InspAdd("autoteste", st.find("FALHOU") == std::string::npos ? 1 : 0, w);
    }
    // 2) conversões a partir do PSD de teste (PNG, JPG, PDF, WEBP); as saídas são apagadas em seguida
    if (vec::Edition() == vec::kLite) InspAdd("conversoes", 2, L"não testado: edição Lite não converte");
    else if (GetFileAttributesW(g_insp.testFile.c_str()) == INVALID_FILE_ATTRIBUTES) InspAdd("conversoes", 2, L"não testado: arquivo de teste ausente");
    else {
        struct { ops::Format f; const wchar_t* n; } fmts[] = {{ops::kPng, L"PNG"}, {ops::kJpg, L"JPG"}, {ops::kPdf, L"PDF"}, {ops::kWebp, L"WEBP"}};
        std::wstring ok, bad;
        for (auto& f : fmts) {
            jobs::Job j; j.format = f.f; j.suffix = L"(inspecao)";
            jobs::Result r;
            try { r = jobs::Run({g_insp.testFile}, j, [](size_t, size_t) {}); } catch (...) { r.failed = 1; }
            bool good = r.failed == 0 && !r.outputs.empty();
            for (auto& o : r.outputs) {
                WIN32_FILE_ATTRIBUTE_DATA fa;
                if (!GetFileAttributesExW(o.c_str(), GetFileExInfoStandard, &fa) || (fa.nFileSizeLow == 0 && fa.nFileSizeHigh == 0)) good = false;
                if (o.rfind(g_insp.dir, 0) == 0) DeleteFileW(o.c_str()); // só dentro da pasta de teste
            }
            (good ? ok : bad) += std::wstring(f.n) + (good ? L" " : (r.errors.empty() ? L" " : L" (" + r.errors[0] + L") "));
        }
        InspAdd("conversoes", bad.empty() ? 1 : 0, bad.empty() ? L"funcionaram: " + ok : L"falharam: " + bad + L"· funcionaram: " + (ok.empty() ? L"nenhuma" : ok));
    }
    // 3) motor vetorial (CDR → SVG/EPS na edição Ultra)
    InspAdd("motor_vetorial", 2, vec::Edition() == vec::kUltra ? L"edição Ultra: CDR → SVG/EPS pelo motor instalado" : L"edição sem SVG/EPS vetorial (CDR → PDF/PNG pelo leitor próprio)");
    // 4) OCR: idiomas de reconhecimento instalados no Windows
    if (!g_clock.ocr) InspAdd("ocr", 2, L"não testado: OCR desligado nas Configurações");
    else {
        std::vector<std::wstring> langs;
        try { langs = ocr::Languages(); } catch (...) {}
        std::wstring l; for (auto& x : langs) l += (l.empty() ? L"" : L", ") + x;
        InspAdd("ocr", langs.empty() ? 0 : 1, langs.empty() ? L"o Windows não tem nenhum idioma de OCR instalado (Configurações do Windows › Idioma › recursos de idioma)" : L"idiomas: " + l);
    }
    // 5) caixa do relógio e botões
    {
        std::wstring b = std::wstring(L"hora ") + (g_clock.time ? L"ligada" : L"desligada") + L"; botões: " +
                         (g_clock.theftOn ? L"roubo " : L"") + (g_clock.pomoOn ? L"pomodoro " : L"") + (g_clock.chat ? L"app " : L"") +
                         (g_clock.clean ? L"limpeza " : L"") + (g_clock.ocr && g_clock.ocrBtn ? L"ocr " : L"");
        bool want = g_clock.time || g_clock.theftOn || g_clock.pomoOn || g_clock.chat || g_clock.clean || (g_clock.ocr && g_clock.ocrBtn);
        bool has = g_clock.wnd && IsWindowVisible(g_clock.wnd);
        if (g_lite) InspAdd("caixa_relogio", 2, L"edição Lite: sem relógio");
        else InspAdd("caixa_relogio", want == has ? 1 : 0, b + (want == has ? L"" : has ? L"; a caixa aparece mas devia estar fechada" : L"; a caixa devia aparecer e NÃO está na tela") +
                     L"; tamanho " + std::to_wstring(g_clock.scalePct) + L"%");
    }
    // 6) chuva no fundo
    {
        bool want = rain::ShouldRun((int)g_cfg.themeMode);
        if (!g_cfg.rainEnabled) InspAdd("chuva", 2, L"desligada nas Configurações");
        else if (!want) InspAdd("chuva", 2, L"ligada, mas o tema atual não tem chuva (só Classic e Crispy)");
        else InspAdd("chuva", rain::g_running && rain::g_rainWnd && IsWindow(rain::g_rainWnd) ? 1 : 0,
                     rain::g_running ? std::wstring(L"rodando na camada ") + (rain::g_insertAfter ? L"Progman (Windows 11 24H2+)" : L"WorkerW") + L"; " + std::to_wstring(rain::g_frames) + L" quadros desenhados" + (rain::g_pauseReason[0] ? L"; PAUSADA agora: " + std::wstring(rain::g_pauseReason, rain::g_pauseReason + strlen(rain::g_pauseReason)) : std::wstring(L"; andando")) : L"devia estar rodando e NÃO está (veja o erros.log)");
    }
    // 7) relatórios anônimos
    InspAdd("relatorios", 2, diag::Consent() ? L"envio ligado" : L"envio desligado pelo usuário");
}
