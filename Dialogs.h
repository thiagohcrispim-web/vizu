// Dialogs.h - Todos os diálogos do Vizu extraídos do Vizu.cpp
#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include "Ui.h"

namespace dlg {

// ---------- Toast (notificações) ----------
struct ToastState {
    HWND wnd = nullptr;
    ui::Surface surf;
    std::wstring title, text, log;
    int state = 0;          // 0 progresso, 1 pronto, 2 erro
    int action = 0;         // 0 revelar, 1 baixar update, 3 abrir log
    bool converting = false;
    ULONGLONG lastHover = 0;
    int hot = -1;
    float s = 1;
    std::vector<std::wstring> outputs;
    int done = 0, total = 0;
};

extern ToastState g_toast;

void ShowToast(const std::wstring& title, const std::wstring& text, int timeout = 4000, int action = 0);
void HideToast(HWND h);
void ToastAnimStep(HWND h);
LRESULT CALLBACK ToastProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

// ---------- Crop (recorte) ----------
struct CropState {
    HWND wnd = nullptr;
    ui::Surface surf;
    ID2D1Bitmap* gpu = nullptr;
    HBITMAP bmp = nullptr;
    int index = 0;
    std::vector<bool> done;
    int aspect = 0;         // 0 livre, 1-5 proporções
    D2D1_RECT_F rect = {0, 0, 1, 1}, startRect{};
    D2D1_RECT_F imgRect{};
    int drag = 0;           // 0 nenhum, 1 mover, 2-5 cantos/bordas
    int hot = 0;
    POINT from{};
    std::vector<std::pair<int, D2D1_RECT_F>> hits;
    bool aspectLock = true;
};

extern CropState g_crop;

void OpenCropEditor();
void CropLoad();
void CropApplyAspect(bool first);
void CropNext(bool forward);
int CropGrab(POINT pt);
void PaintCrop(HWND h);
LRESULT CALLBACK CropProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

// ---------- Settings (configurações) ----------
struct SettingsState {
    HWND wnd = nullptr;
    HWND dimWnd = nullptr;     // fundo escuro
    ui::Surface surf;
    std::vector<std::pair<int, D2D1_RECT_F>> hits;
    int hot = 0;
    float scroll = 0, contentH = 0;
    POINT mouse = {-100000, -100000};
    bool down = false, closing = false;
    ui::Anim anim;             // animação abrir/fechar
    int page = 0;              // 0 configurações, 1 sobre, 2 agradecimentos
    int capHot = 0;            // capturando atalho: kHkOcr ou kHkClean
    bool capFail = false;
    int capLast = 0;
    bool advOpen[6] = {};      // seções avançadas recolhidas
    static const int kSecCount = 6;
};

extern SettingsState g_set;

void OpenSettings();
void CloseSettings();
void PaintSettings(HWND h);
void SettingsAction(int id);
std::wstring HotkeyName(DWORD k);
void CaptureHotkey(int id);
LRESULT CALLBACK SettingsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

// ---------- Dim (fundo escuro das configurações) ----------
LRESULT CALLBACK DimProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);
HWND ShowDim(HMONITOR mon);

// ---------- OCR (seleção e cartão) ----------
struct OcrCard {
    HWND wnd = nullptr;
    std::wstring original, shown, lang, status, target;
    int state = 0;            // 0 copiado, 1 traduzindo, 2 traduzido, 3 erro
    bool joined = false;
    int hot = -1;
    D2D1_RECT_F r[8]{};       // 0 PT, 1 ES, 2 EN, 3 juntar, 4 web, 5 fechar, 6 original
    POINT anchor{};
    float s = 1;
    ULONGLONG lastHover = 0;
};

struct OcrSel {
    HWND wnd = nullptr;
    HBITMAP shot = nullptr, dim = nullptr;
    BYTE* shotBits = nullptr;
    int vx = 0, vy = 0, vw = 0, vh = 0;
    bool drag = false;
    POINT a{}, b{};
    COLORREF accent = RGB(255, 255, 255);
};

extern OcrCard g_card;
extern OcrSel g_sel;

void ShowCard(const std::wstring& text, const std::wstring& lang, POINT anchor);
void CardTranslate(const wchar_t* to);
void CardClose();
void CardRender();
int CardHit(POINT pt);
LRESULT CALLBACK CardProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

void StartOcr();
void SelClose();
RECT SelRect();
void SelFinish();
LRESULT CALLBACK SelProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

// ---------- Clock (relógio na área de trabalho) ----------
struct ClockState {
    HWND wnd = nullptr;
    bool on = false;
    std::wstring font = L"Bahnschrift", app = L"https://chatgpt.com";
    DWORD fontPx = 38, pomoTimes = 0, pomoColors = 0;
    int phase = 0;            // 0 parado, 1 foco, 2 pausa, 3 pausa longa
    int done = 0;
    bool paused = false;
    ULONGLONG phaseEnd = 0, phaseLen = 1, pausedLeft = 0;
    bool theft = false;
    ULONGLONG theftStart = 0;
    SYSTEMTIME theftWall = {};
    int hot = -1, lastSec = -1;
    D2D1_RECT_F rTri{}, rChip[5]{};
    float s = 1;
    bool reminderOpen = false;
    bool ocr = true, clean = true, chat = true, ocrBtn = true;
    bool theftOn = true, pomoOn = true, agendaOn = true;
    DWORD scalePct = 100;
    bool docked = false;
    RECT tb{};
};

extern ClockState g_clock;

void ClockEnable(bool on);
void ClockLoad();
void ClockSave();
void ClockRender();
void ClockDefaultPos();
bool TaskbarRect(RECT* r);
bool TaskbarDark();
void ClockDock(bool on);
void SetTrayClock(bool docking);
int ClockHit(POINT pt);
void ClockMenu(POINT pt);
void OpenAgendaWindow();
void ShowTheftHistory();
void CheckReminders();
void PomoStart(int phase);
void PomoStop();
void PomoTick();
void PomoPauseResume();
LRESULT CALLBACK ClockProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

// ---------- Agenda ----------
struct AgendaItem {
    SYSTEMTIME when;
    std::wstring title;
    int dur = 30;
    int alerts = 0; // kAlert10 | kAlert5
};

struct AgendaUi {
    HWND wnd = nullptr;
    ui::Surface surf;
    SYSTEMTIME day{};
    std::vector<std::pair<int, D2D1_RECT_F>> hits;
    int hot = 0, field = 0;
    std::wstring f[3];
    int dur = 30, alerts = 0;
    int editIdx = -1;
    std::wstring msg;
    bool closing = false, down = false;
    POINT mouse = {-100000, -100000};
};

enum { kAlert10 = 1, kAlert5 = 2, kAlert0 = 4 };
extern AgendaUi g_ag;
extern bool g_agClosing, g_agDown;
extern POINT g_agMouse;

std::vector<AgendaItem> LoadAgenda();
bool SaveAgenda(const std::vector<AgendaItem>& v);
void AgResetForm();
void AgClose(HWND h);
void PaintAgenda(HWND h);
void AgAction(int id);
void AgChar(wchar_t c);
LRESULT CALLBACK AgendaProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

// ---------- Theft History (Roubo de Tempo) ----------
struct TheftUi {
    HWND wnd = nullptr;
    ui::Surface surf;
    bool week = false;
    int hot = 0;
    D2D1_RECT_F btn[3]{};
};

extern TheftUi g_tv;

void PaintTheft(HWND h);
void TheftResize();
void TheftExport();
LRESULT CALLBACK TheftProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

// ---------- Reminder (avisos da agenda) ----------
struct Reminder {
    HWND wnd = nullptr;
    AgendaItem item;
    int level = 10;
    ULONGLONG shownAt = 0;
    int hot = -1;
    D2D1_RECT_F r[3]{};
    float s = 1;
};

extern Reminder g_rem;
extern std::set<std::wstring> g_remFired;
extern std::vector<std::pair<LONGLONG, AgendaItem>> g_remSnooze;

void ReminderRender();
void ReminderClose();
void ShowReminder(const AgendaItem& a, int level);
LRESULT CALLBACK ReminderProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

// ---------- Clean (Limpeza) ----------
struct CleanUi {
    HWND wnd = nullptr;
    ui::Surface surf;
    int phase = 0, overlay = 0;
    bool map = false;
    // clean::Result res, shared; // forward declared in Clean.h
    std::mutex mu;
    int scanGroup = 0;
    std::thread worker;
    std::atomic<bool> stop{false};
    std::set<int> openG, openI;
    float scroll = 0, contentH = 0, listTop = 0, listBottom = 0;
    std::vector<std::pair<int, D2D1_RECT_F>> hits;
    std::vector<std::wstring> running;
    std::vector<std::wstring> askQueue, toClose, skipped;
    std::atomic<ULONGLONG> doneBytes{0};
    ULONGLONG planBytes = 0;
    std::wstring status;
    int cleaningG = -1;
    std::map<int, int> out; // Outcome forward declared
    std::set<int> skippedLeaves, stillOpen;
    ULONGLONG removed = 0, kept = 0, recycledBytes = 0;
    // clean::Disk disk;
    float shown = 0;
    bool closeAfter = false;
    ULONGLONG shownTarget = 0;
};

extern CleanUi g_cl;

void ClRefreshRunning();
void ClRefreshDisk();
void ClJoin();
void ClStartScan();
void ClPullScan();
void ClStartClean();
void ClAction(int id);
void PaintClean(HWND h);
LRESULT CALLBACK CleanProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

// ---------- Widgets (flutuantes) ----------
enum { kWDisk = 0, kWPerf = 1, kWAi = 2, kWBat = 3, kWCount = 4 };

struct Widget {
    HWND wnd = nullptr;
    bool open = false, pressed = false, dragging = false;
    POINT down{};
    int hot = 0;
    int mode = 0;
    bool docked = false;
    D2D1_RECT_F rBtn{}, rClose{}, rOpt{}, rApply{}, rBack{};
    std::vector<D2D1_RECT_F> rItems;
    LONG ax = 0, ay = 0;
    UINT w = 0, h = 0;
    ULONGLONG leaveAt = 0;
};

extern Widget g_wd[kWCount];

void WidgetsInit();
void WidgetsRenderAll();
void WidgetOpen(int k);
void WidgetClose(int k);
void WidgetPaint(int k, HWND h);
LRESULT CALLBACK WidgetProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

// Forward declarations for types used in dialogs
namespace clean {
struct Result;
struct Disk;
struct GroupRes;
struct Item;
struct Leaf;
struct Target;
struct Outcome;
enum { kGroupCount = 8, kTargetCount = 50 };
int TargetOf(const Leaf&);
std::wstring LowerPath(const std::wstring&);
std::vector<std::wstring> Split(const std::wstring&);
std::vector<std::wstring> RunningExes();
Disk SystemDisk();
int Actionable(const Leaf&);
void CloseProgram(const std::wstring&);
void Scan(Result&, int, bool, std::atomic<bool>&, std::function<void(int)>);
} // namespace clean

} // namespace dlg