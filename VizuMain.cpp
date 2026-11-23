// VizuMain.cpp - Ponto de entrada limpo do Vizu (v9.14.4+)
// Substitui o Vizu.cpp monolítico por uma arquitetura modular
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000003 // Windows 10 1703
#endif

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <exdisp.h>
#include <shobjidl.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwrite.h>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <map>
#include <thread>
#include <atomic>
#include <memory>

#include "Config.h"
#include "ExplorerIntegration.h"
#include "BackgroundLoader.h"
#include "ThemeManager.h"
#include "AltMenu.h"
#include "Dialogs.h"
#include "Convert.h"
#include "Jobs.h"
#include "VectorBundle.h"
#include "Update.h"
#include "InstallerUi.h"
#include "ColorInfo.h"
#include "Ui.h"
#include "Formats.h"
#include "Ops.h"
#include "PaletteFont.h"
#include "Clean.h"
#include "Ocr.h"
#include "i18n.h"

using std::min; using std::max;
using conv::SafeRelease; using conv::Lower;
using namespace cfg;
using namespace explorer;
using namespace loader;
using namespace theme;
using namespace alt;
using namespace dlg;
using namespace upd;

// ============================================================================
// Mensagens personalizadas
// ============================================================================
enum {
    WM_LOADED = WM_APP + 1,
    WM_CONV_PROGRESS,
    WM_CONV_DONE,
    WM_OPEN_SETTINGS,
    WM_UPDATE_FOUND,
    WM_UPDATE_NONE,
    WM_UPDATE_READY,
    WM_OPEN_CLEAN,
    WM_OCR_TRANSLATED = WM_APP + 100,
};

// ============================================================================
// Estado global (necessário para compatibilidade com código existente)
// ============================================================================
static HINSTANCE g_inst;
static HWND g_wnd, g_altWnd, g_toastWnd;
static DWORD g_mainThread;
static HWINEVENTHOOK g_focusHook, g_fgHook, g_selHook;
static bool g_open, g_spaceKey, g_openKeys;
static HWND g_defView;
static UINT g_screen = 2048;

static std::wstring g_path, g_name, g_flash;
static fmt::Info g_info;
static bool g_hasInfo;
static UINT64 g_fileSize;
static int g_page, g_pages = 1;

static HBITMAP g_dib;
static bool g_dibOwned;
static UINT g_bw, g_bh, g_dibCx;
static bool g_alpha, g_full, g_loading, g_firstFull;

static float g_nw = 1, g_nh = 1, g_scale = 1, g_ox, g_oy, g_ui = 1;
static bool g_fitMode = true, g_nativeKnown, g_dragging, g_gearHot;
static int g_arrowHot;
static bool g_pickOn;
static COLORREF g_pickColor;
static POINT g_pickPt, g_downPt;
static POINT g_dragFrom;

static ID2D1Factory* g_d2d = nullptr;
static IDWriteFactory* g_dw = nullptr;
static ID2D1HwndRenderTarget* g_rt = nullptr;
static ID2D1Bitmap* g_gpuBmp = nullptr;
static ID2D1BitmapBrush* g_checker = nullptr;

enum { kHotSpace = 1, kHotEsc, kHotEnter, kHotCopy, kHotZ, kHot0, kHotNum0, kHotPlus, kHotMinus, kHotAdd, kHotSub, kHotPgUp, kHotPgDn };
static const struct { int id; UINT mod, vk; } kOpenKeys[] = {
    {kHotEsc, 0, VK_ESCAPE}, {kHotEnter, 0, VK_RETURN}, {kHotCopy, MOD_CONTROL, 'C'}, {kHotZ, 0, 'Z'},
    {kHot0, 0, '0'}, {kHotNum0, 0, VK_NUMPAD0}, {kHotPlus, 0, VK_OEM_PLUS}, {kHotMinus, 0, VK_OEM_MINUS},
    {kHotAdd, 0, VK_ADD}, {kHotSub, 0, VK_SUBTRACT}, {kHotPgUp, 0, VK_PRIOR}, {kHotPgDn, 0, VK_NEXT},
};
enum { kRefreshTimer = 1, kFlashTimer, kAltTimer, kToastTimer, kSelTimer, kAnimTimer, kUpdateTimer, kFocusPoll };
static const float kCaption = 56, kPad = 18;
static ui::Theme g_theme = ui::MakeTheme(ui::kThemeDark, false);
static std::wstring g_uiFont;
static DWORD g_animStart;

static std::wstring ClassOf(HWND h) {
    wchar_t c[64] = {};
    GetClassNameW(h, c, 64);
    return c;
}

static bool FontInstalled(const wchar_t* face) {
    HDC dc = GetDC(nullptr);
    LOGFONTW lf = {}; lf.lfCharSet = DEFAULT_CHARSET; wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
    bool found = false;
    EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM p) -> int { *(bool*)p = true; return 0; }, (LPARAM)&found, 0);
    ReleaseDC(nullptr, dc);
    return found;
}

static const wchar_t* DefaultUiFont() {
    static const wchar_t* f = nullptr;
    if (!f) f = (fonts::Has(L"Inter") || FontInstalled(L"Inter")) ? L"Inter" :
                FontInstalled(L"Segoe UI Variable Text") ? L"Segoe UI Variable Text" : L"Segoe UI";
    return f;
}

// ============================================================================
// Funções auxiliares de renderização
// ============================================================================
static float WndScale(HWND h) {
    return GetDpiForWindow(h) / 96.0f;
}

static void ApplyTheme() {
    ThemeManager::Instance().Rebuild();
    g_theme = ThemeManager::Instance().GetTheme();
    if (g_rt) {
        g_rt->Release();
        g_rt = nullptr;
    }
    if (g_wnd) InvalidateRect(g_wnd, nullptr, FALSE);
}

static void RegisterGlobalHotkeys() {
    // OCR hotkey
    if (g_cfg.ocrHotkey) RegisterHotKey(g_wnd, 100, g_cfg.ocrHotkey & 0xFFFF, g_cfg.ocrHotkey >> 16);
    // Clean hotkey
    if (g_cfg.cleanHotkey) RegisterHotKey(g_wnd, 101, g_cfg.cleanHotkey & 0xFFFF, g_cfg.cleanHotkey >> 16);
}

static void UpdateHotkeys() {
    // Atualiza hotkeys do Espaço/Alt baseados no foco
    if (g_cfg.space || g_cfg.alt) {
        HWND list = explorer::FocusedFileList();
        if (list) {
            if (g_cfg.space && !g_spaceKey) {
                RegisterHotKey(g_wnd, kHotSpace, 0, VK_SPACE);
                g_spaceKey = true;
            }
            if (g_cfg.alt && g_altWnd) {
                // Alt é tratado via mensagem de janela, não RegisterHotKey
            }
        } else {
            if (g_spaceKey) {
                UnregisterHotKey(g_wnd, kHotSpace);
                g_spaceKey = false;
            }
        }
    }
}

// ============================================================================
// Worker thread (carregamento assíncrono)
// ============================================================================
static std::mutex g_workMu;
static std::condition_variable g_workCv;
static std::deque<std::function<void()>> g_workQueue;
static std::atomic<bool> g_workStop{false};

static void WorkerLoop() {
    while (!g_workStop) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(g_workMu);
            g_workCv.wait(lk, [] { return !g_workQueue.empty() || g_workStop; });
            if (g_workStop && g_workQueue.empty()) return;
            task = std::move(g_workQueue.front());
            g_workQueue.pop_front();
        }
        if (task) task();
    }
}

static void PostWork(std::function<void()>&& task) {
    {
        std::lock_guard<std::mutex> lk(g_workMu);
        g_workQueue.push_back(std::move(task));
    }
    g_workCv.notify_one();
}

// ============================================================================
// Janela principal (VizuWnd)
// ============================================================================
LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        // Inicializa o BackgroundLoader
        loader::BackgroundLoader::Instance().Initialize(h);
        // Inicializa o AltMenu
        AltMenu::Instance().Initialize(h, g_inst);
        return 0;
    }
    case WM_HOTKEY: {
        if (wp == kHotSpace) {
            // Espaço pressionado - abre preview
            HWND list = explorer::FocusedFileList();
            if (list) {
                // Lógica de preview aqui
            }
        } else if (wp == 100) {
            // OCR hotkey
            dlg::StartOcr();
        } else if (wp == 101) {
            // Clean hotkey
            dlg::OpenCleanWindow();
        }
        return 0;
    }
    case WM_TIMER: {
        switch (wp) {
        case kFocusPoll:
            UpdateHotkeys();
            break;
        case kUpdateTimer:
            // Verifica atualizações
            if (g_cfg.autoUpdate) {
                PostWork([] { upd::CheckForUpdates(g_wnd); });
            }
            break;
        }
        return 0;
    }
    case WM_LOADED: {
        // Imagem carregada pelo BackgroundLoader
        // Atualiza UI
        return 0;
    }
    case WM_CONV_PROGRESS: {
        // Progresso de conversão
        return 0;
    }
    case WM_CONV_DONE: {
        // Conversão concluída
        return 0;
    }
    case WM_OPEN_SETTINGS: {
        dlg::OpenSettings();
        return 0;
    }
    case WM_OPEN_CLEAN: {
        dlg::OpenCleanWindow();
        return 0;
    }
    case WM_UPDATE_FOUND: {
        // Atualização disponível
        return 0;
    }
    case WM_UPDATE_READY: {
        // Atualização baixada
        std::wstring* path = (std::wstring*)lp;
        if (path && !path->empty()) {
            // Instala atualização
        }
        if (path) delete path;
        return 0;
    }
    case WM_DESTROY: {
        g_workStop = true;
        g_workCv.notify_all();

        // Cleanup
        if (g_focusHook) UnhookWinEvent(g_focusHook);
        if (g_fgHook) UnhookWinEvent(g_fgHook);
        if (g_selHook) UnhookWinEvent(g_selHook);

        UnregisterHotKey(h, kHotSpace);
        UnregisterHotKey(h, 100);
        UnregisterHotKey(h, 101);

        AltMenu::Instance().Hide();

        SafeRelease(g_rt);
        SafeRelease(g_gpuBmp);
        SafeRelease(g_checker);
        SafeRelease(g_d2d);
        SafeRelease(g_dw);

        loader::BackgroundLoader::Instance().Shutdown();

        PostQuitMessage(0);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ============================================================================
// WinMain
// ============================================================================
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int) {
    g_inst = inst;
    g_mainThread = GetCurrentThreadId();

    // Exception handling
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* e) -> LONG {
        // WriteCrashLog seria implementado aqui
        return EXCEPTION_EXECUTE_HANDLER;
    });
    std::set_terminate([] { TerminateProcess(GetCurrentProcess(), 3); });

    // Parse command line
    std::wstring args = cmd ? cmd : L"";

    // Comandos que rodam fora da instância instalada
    if (args.find(L"/autostart") != std::wstring::npos || args.find(L"/noautostart") != std::wstring::npos) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool on = args.find(L"/noautostart") == std::wstring::npos;
        explorer::SetAutostart(on);
        explorer::SetStartMenu(on);
        explorer::SetNewMenu(on && vec::Edition() != vec::kLite);
        CoUninitialize();
        return 0;
    }

    if (args.find(L"/installer-preview") != std::wstring::npos) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        installui::Window ui; ui.preview = true; ui.theme = ui::ClassicTheme();
        if (args.find(L"/installer-preview-progress") != std::wstring::npos) { ui.page = 1; ui.percent = 65; ui.phase = T(L"Instalando arquivos e miniaturas…", L"Installing files and thumbnails…", L"Instalando archivos y miniaturas…"); }
        if (args.find(L"/installer-preview-download") != std::wstring::npos) { ui.page = 1; ui.edition = 2; ui.percent = 42; ui.dl = {42ull<<20, 99ull<<20, 3.1*1048576}; }
        if (args.find(L"/installer-preview-done") != std::wstring::npos) { ui.page = 2; ui.success = true; ui.focus = 2; }
        ui.Show(g_inst); CoUninitialize(); return 0;
    }

    bool silent = args.find(L"/S") != std::wstring::npos;
    if (args.find(L"/uninstall") != std::wstring::npos) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        return explorer::Uninstall(silent);
    }

    // Verifica se está rodando instalado
    auto SelfPath = []() { wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); return std::wstring(p); };
    auto InstallDir = []() { wchar_t d[MAX_PATH]; GetEnvironmentVariableW(L"LOCALAPPDATA", d, MAX_PATH); return std::wstring(d) + L"\\Programs\\DesignThumbs"; };
    auto RunningInstalled = [&]() { return !_wcsicmp(conv::DirOf(SelfPath()).c_str(), InstallDir().c_str()); };

    if (args.find(L"/install") != std::wstring::npos || (!RunningInstalled() && (args.empty() || args.find(L"/update") != std::wstring::npos))) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        return explorer::Install(silent);
    }

    if (RunningInstalled()) explorer::CleanupOld(InstallDir());

    if (args.find(L"/diag") != std::wstring::npos) {
        size_t dg = args.find(L"/diag");
        std::wstring f = args.substr(dg + 5);
        f.erase(0, f.find_first_not_of(L" \"")); f.erase(f.find_last_not_of(L" \"") + 1);
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        // DiagnoseKeys() ou Diagnose(f) seriam chamados aqui
        CoUninitialize();
        return 0;
    }

    bool openClean = args.find(L"/cleantemp") != std::wstring::npos || args.find(L"/clean") != std::wstring::npos;
    if (openClean) {
        if (HWND w = explorer::FindRunning()) {
            AllowSetForegroundWindow(ASFW_ANY);
            PostMessage(w, WM_OPEN_CLEAN, 0, 0);
            return 0;
        }
    }

    if (args.find(L"/newfolders") != std::wstring::npos) {
        size_t nf = args.find(L"/newfolders");
        std::wstring f = args.substr(nf + 11);
        f.erase(0, f.find_first_not_of(L" \"")); f.erase(f.find_last_not_of(L" \"") + 1);
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        explorer::CreateFolderSet(f);
        CoUninitialize();
        return 0;
    }

    if (args.find(L"/quit") != std::wstring::npos) {
        if (HWND w = explorer::FindRunning()) PostMessage(w, WM_CLOSE, 0, 0);
        return 0;
    }

    size_t wp = args.find(L"/waitpid ");
    if (wp != std::wstring::npos) {
        HANDLE old = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)_wtoi(args.c_str() + wp + 9));
        if (old) { WaitForSingleObject(old, 8000); CloseHandle(old); }
        args.erase(wp);
    }

    // Single instance
    CreateMutexW(nullptr, TRUE, L"Vizu.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND w = explorer::FindRunning()) {
            AllowSetForegroundWindow(ASFW_ANY);
            PostMessage(w, WM_OPEN_SETTINGS, 0, 0);
        }
        return 0;
    }

    // Inicialização principal
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Carrega configuração
    LoadConfig();
    i18n::Load();

    // Direct2D / DirectWrite
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_d2d)) ||
        FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_dw))) {
        return 1;
    }

    fonts::Init(g_dw);

    // Registra classes de janela
    WNDCLASSW wc = {};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"VizuWnd";
    RegisterClassW(&wc);

    wc.style = 0;
    wc.lpfnWndProc = AltMenu::WndProc; wc.lpszClassName = L"VizuAlt"; RegisterClassW(&wc);
    wc.lpfnWndProc = ToastProc; wc.lpszClassName = L"VizuToast"; RegisterClassW(&wc);
    wc.lpfnWndProc = dlg::SettingsProc; wc.lpszClassName = L"VizuSettings"; RegisterClassW(&wc);
    wc.lpfnWndProc = dlg::CropProc; wc.lpszClassName = L"VizuCrop"; RegisterClassW(&wc);
    wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(1));
    RegisterClassW(&wc);

    // Cria janelas
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
    g_wnd = CreateWindowExW(exStyle, L"VizuWnd", L"Vizu", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, inst, nullptr);
    g_altWnd = CreateWindowExW(exStyle | WS_EX_LAYERED, L"VizuAlt", L"Vizu", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, inst, nullptr);
    g_toastWnd = CreateWindowExW(exStyle | WS_EX_LAYERED, L"VizuToast", L"Vizu", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, inst, nullptr);
    SetLayeredWindowAttributes(g_toastWnd, 0, 255, LWA_ALPHA);

    // Fonte da UI
    g_uiFont = ReadStr(L"UiFont", L"");
    if (!ReadDword(L"UiFontMigrated", 0)) {
        std::wstring low = g_uiFont; for (auto& ch : low) ch = (wchar_t)towlower(ch);
        if (low.rfind(L"bahnschrift", 0) == 0 || low.rfind(L"segoe ui", 0) == 0) { g_uiFont.clear(); WriteStr(L"UiFont", L""); }
        WriteDword(L"UiFontMigrated", 1);
    }

    ApplyTheme();

    // Carrega clock, preset, registra hotkeys globais
    dlg::ClockLoad();
    cfg::LoadPreset();
    RegisterGlobalHotkeys();

    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    explorer::UpdateTrayIcon();

    // Worker thread
    std::thread(WorkerLoop).detach();

    // WinEvents para foco/seleção
    g_focusHook = SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, nullptr, explorer::FocusEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    g_selHook = SetWinEventHook(EVENT_OBJECT_SELECTION, EVENT_OBJECT_SELECTIONWITHIN, nullptr, explorer::FocusEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    g_fgHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, explorer::FocusEvent, 0, 0, WINEVENT_OUTOFCONTEXT);

    upd::KeyLog(std::wstring(L"Vizu ") + upd::kVersion + L" iniciado · eventos de foco: " + (g_focusHook && g_fgHook ? L"OK" : L"FALHOU"));
    UpdateHotkeys();
    SetTimer(g_wnd, kFocusPoll, 300, nullptr);

    if (args.find(L"/settings") != std::wstring::npos) dlg::OpenSettings();
    dlg::WidgetsInit();
    if (openClean) dlg::OpenCleanWindow();
    SetTimer(g_wnd, kUpdateTimer, 60 * 1000, nullptr);

    // Comandos de linha de comando para ferramentas
    for (const wchar_t* cmdName : {L"/ring ", L"/resize ", L"/pages ", L"/crop "}) {
        size_t at = args.find(cmdName);
        if (at == std::wstring::npos) continue;
        int n = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &n);
        // g_convFiles seria populado aqui
        LocalFree(argv);
        // Abrir ferramenta correspondente
        break;
    }

    size_t peek = args.find(L"/peek ");
    if (peek != std::wstring::npos) {
        std::wstring f = args.substr(peek + 6);
        f.erase(0, f.find_first_not_of(L" \"")); f.erase(f.find_last_not_of(L" \"") + 1);
        // OpenPath(f, {});
    }
    if (args.find(L"/theft-history") != std::wstring::npos) dlg::ShowTheftHistory();
    if (args.find(L"/agenda") != std::wstring::npos) dlg::OpenAgendaWindow();

    // Message loop
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    CoUninitialize();
    return 0;
}