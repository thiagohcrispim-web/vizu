// ExplorerIntegration.cpp - Implementação da integração com Windows Explorer
#include "ExplorerIntegration.h"
#include "Convert.h"
#include <exdisp.h>
#include <shobjidl.h>
#include <algorithm>
#include <set>

using conv::SafeRelease;

namespace explorer {

static std::wstring g_viewProbe;
static HWND g_orderView = nullptr;
static std::vector<std::wstring> g_selOrder;

std::wstring ClassOf(HWND h) {
    wchar_t c[64] = {};
    GetClassNameW(h, c, 64);
    return c;
}

// Retorna a SHELLDLL_DefView que tem o foco (lista de arquivos), ou nullptr
HWND FocusedFileList() {
    HWND fg = GetForegroundWindow();
    if (!fg) return nullptr;
    GUITHREADINFO gti = {sizeof(gti)};
    if (!GetGUIThreadInfo(GetWindowThreadProcessId(fg, nullptr), &gti)) return nullptr;

    std::wstring fgc = ClassOf(fg);
    bool desk = fgc == L"Progman" || fgc == L"WorkerW";

    if (gti.flags & (GUI_POPUPMENUMODE | GUI_INMOVESIZE)) return nullptr;
    if ((gti.flags & GUI_INMENUMODE) && !(desk && GetAsyncKeyState(VK_MENU) < 0)) return nullptr;

    HWND f = gti.hwndFocus;
    if (!f) return nullptr;

    std::wstring fc = ClassOf(f);
    if (fc != L"DirectUIHWND" && fc != L"SysListView32") return nullptr;

    if (gti.hwndCaret && gti.hwndCaret != f) return nullptr;

    HWND parent = GetParent(f);
    return parent && ClassOf(parent) == L"SHELLDLL_DefView" ? parent : nullptr;
}

static IShellView* ViewFromBrowser(IDispatch* disp, HWND defView) {
    IServiceProvider* sp = nullptr;
    IShellBrowser* sb = nullptr;
    IShellView* sv = nullptr;

    if (SUCCEEDED(disp->QueryInterface(IID_PPV_ARGS(&sp))) &&
        SUCCEEDED(sp->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&sb))) &&
        SUCCEEDED(sb->QueryActiveShellView(&sv))) {
        HWND h = nullptr;
        sv->GetWindow(&h);
        if (h != defView) SafeRelease(sv);
    }
    SafeRelease(sb);
    SafeRelease(sp);
    return sv;
}

IShellView* ShellViewFor(HWND defView) {
    IShellWindows* sw = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&sw));
    if (FAILED(hr)) {
        wchar_t b[64];
        swprintf(b, 64, L"ShellWindows falhou (0x%08X)", (unsigned)hr);
        g_viewProbe = b;
        return nullptr;
    }

    IShellView* view = nullptr;
    long count = 0;
    sw->get_Count(&count);

    for (long i = 0; i < count && !view; ++i) {
        VARIANT v; VariantInit(&v); v.vt = VT_I4; v.lVal = i;
        IDispatch* d = nullptr;
        if (SUCCEEDED(sw->Item(v, &d)) && d) {
            view = ViewFromBrowser(d, defView);
            d->Release();
        }
    }

    const wchar_t* how = view ? L"exata" : nullptr;

    if (!view) {
        HWND root = GetAncestor(defView, GA_ROOT);
        for (long i = 0; i < count && !view; ++i) {
            VARIANT v; VariantInit(&v); v.vt = VT_I4; v.lVal = i;
            IDispatch* d = nullptr;
            IWebBrowser2* wb = nullptr;
            if (SUCCEEDED(sw->Item(v, &d)) && d && SUCCEEDED(d->QueryInterface(IID_PPV_ARGS(&wb)))) {
                SHANDLE_PTR hw = 0;
                if (SUCCEEDED(wb->get_HWND(&hw)) && (HWND)hw == root) {
                    IServiceProvider* sp = nullptr;
                    IShellBrowser* sb = nullptr;
                    IShellView* sv = nullptr;
                    if (SUCCEEDED(d->QueryInterface(IID_PPV_ARGS(&sp))) &&
                        SUCCEEDED(sp->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&sb))) &&
                        SUCCEEDED(sb->QueryActiveShellView(&sv))) {
                        HWND h = nullptr;
                        sv->GetWindow(&h);
                        if (h && IsWindowVisible(h)) {
                            view = sv;
                            sv = nullptr;
                            how = L"pela janela principal";
                        }
                    }
                    SafeRelease(sv);
                    SafeRelease(sb);
                    SafeRelease(sp);
                }
            }
            SafeRelease(wb);
            SafeRelease(d);
        }
    }

    if (!view) {
        VARIANT empty; VariantInit(&empty);
        long hwnd;
        IDispatch* d = nullptr;
        if (sw->FindWindowSW(&empty, &empty, SWC_DESKTOP, &hwnd, SWFO_NEEDDISPATCH, &d) == S_OK && d) {
            view = ViewFromBrowser(d, defView);
            d->Release();
            if (view) how = L"área de trabalho";
        }
    }

    sw->Release();
    wchar_t b[160];
    swprintf(b, 160, L"janelas do Explorer: %ld · vista da lista: %ls", count, how ? how : L"NÃO encontrada");
    g_viewProbe = b;
    return view;
}

std::wstring ItemPath(IShellItem* item) {
    PWSTR p = nullptr;
    std::wstring s;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
        s = p;
        CoTaskMemFree(p);
    }
    return s;
}

bool IsFile(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<std::wstring> SelectedFiles(HWND defView) {
    std::vector<std::wstring> files;
    IShellView* sv = ShellViewFor(defView);
    IShellItemArray* arr = nullptr;

    if (sv && SUCCEEDED(sv->GetItemObject(SVGIO_SELECTION, IID_PPV_ARGS(&arr)))) {
        DWORD n = 0;
        arr->GetCount(&n);
        for (DWORD i = 0; i < n; ++i) {
            IShellItem* it = nullptr;
            if (SUCCEEDED(arr->GetItemAt(i, &it))) {
                std::wstring p = ItemPath(it);
                if (IsFile(p)) files.push_back(p);
                it->Release();
            }
        }
    }

    KeyLog(L"seleção: " + std::to_wstring(files.size()) + L" arquivo(s) · " + g_viewProbe);
    SafeRelease(arr);
    SafeRelease(sv);
    return files;
}

void SyncSelectionOrder(HWND defView) {
    std::vector<std::wstring> cur = SelectedFiles(defView);
    if (defView != g_orderView) {
        g_orderView = defView;
        g_selOrder = cur;
        return;
    }

    std::set<std::wstring> now(cur.begin(), cur.end());
    std::vector<std::wstring> kept;
    for (auto& f : g_selOrder)
        if (now.count(f)) kept.push_back(f);

    std::set<std::wstring> had(kept.begin(), kept.end());
    for (auto& f : cur)
        if (!had.count(f)) kept.push_back(f);

    g_selOrder.swap(kept);
}

std::wstring FocusedFile(HWND defView, std::vector<std::wstring>* neighbors) {
    std::wstring path;
    IShellView* sv = ShellViewFor(defView);
    IFolderView2* fv = nullptr;

    if (sv && SUCCEEDED(sv->QueryInterface(IID_PPV_ARGS(&fv)))) {
        int idx = -1, count = 0;
        if (SUCCEEDED(fv->GetSelectedItem(-1, &idx)) && idx >= 0) {
            int focused = -1;
            if (SUCCEEDED(fv->GetFocusedItem(&focused)) && focused >= 0) {
                IShellItemArray* sel = nullptr;
                BOOL isSel = FALSE;
                if (SUCCEEDED(fv->GetSelection(FALSE, &sel))) {
                    IShellItem* fi = nullptr;
                    if (SUCCEEDED(fv->GetItem(focused, IID_PPV_ARGS(&fi)))) {
                        DWORD n = 0;
                        sel->GetCount(&n);
                        for (DWORD i = 0; i < n && !isSel; ++i) {
                            IShellItem* si = nullptr;
                            int order = 1;
                            if (SUCCEEDED(sel->GetItemAt(i, &si))) {
                                si->Compare(fi, SICHINT_CANONICAL, &order);
                                si->Release();
                            }
                            isSel = order == 0;
                        }
                        fi->Release();
                    }
                    sel->Release();
                }
                if (isSel) idx = focused;
            }
            IShellItem* it = nullptr;
            if (SUCCEEDED(fv->GetItem(idx, IID_PPV_ARGS(&it)))) {
                path = ItemPath(it);
                it->Release();
            }
            fv->ItemCount(SVGIO_ALLVIEW, &count);
            if (neighbors)
                for (int d : {1, -1, 2}) {
                    int j = idx + d;
                    if (j < 0 || j >= count) continue;
                    IShellItem* ni = nullptr;
                    if (SUCCEEDED(fv->GetItem(j, IID_PPV_ARGS(&ni)))) {
                        std::wstring np = ItemPath(ni);
                        if (IsFile(np)) neighbors->push_back(np);
                        ni->Release();
                    }
                }
        }
    }
    SafeRelease(fv);
    SafeRelease(sv);
    return path;
}

void KeyLog(const std::wstring& line) {
    static std::wstring last;
    if (line == last) return;
    last = line;

    std::wstring path = L"";
    wchar_t t[MAX_PATH];
    GetTempPathW(MAX_PATH, t);
    path = std::wstring(t) + L"Vizu-teclado.log";

    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa) && fa.nFileSizeLow > 64 * 1024)
        DeleteFileW(path.c_str());

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t ts[32];
    swprintf(ts, 32, L"%02d:%02d:%02d ", st.wHour, st.wMinute, st.wSecond);
    std::wstring w = ts + line + L"\r\n";

    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string u(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &u[0], n, nullptr, nullptr);

    HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD wr;
        WriteFile(f, u.data(), (DWORD)u.size(), &wr, nullptr);
        CloseHandle(f);
    }
}

} // namespace explorer