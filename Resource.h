// Resource.h - RAII wrappers para recursos Windows/COM/Direct2D
// Substitui chamadas manuais de Release/DeleteObject por destruição automática
#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <memory>
#include <functional>

namespace res {

// ============================================================================
// Smart pointers COM (usando Microsoft::WRL::ComPtr internamente)
// ============================================================================
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// Helper para criar ComPtr a partir de ponteiro bruto (transfere ownership)
template <typename T>
ComPtr<T> MakeComPtr(T* ptr) {
    return ComPtr<T>(ptr);
}

// ============================================================================
// RAII para GDI Objects
// ============================================================================

// HBITMAP
class ScopedBitmap {
    HBITMAP bmp_ = nullptr;
public:
    ScopedBitmap() = default;
    explicit ScopedBitmap(HBITMAP b) : bmp_(b) {}
    ~ScopedBitmap() { if (bmp_) DeleteObject(bmp_); }
    ScopedBitmap(const ScopedBitmap&) = delete;
    ScopedBitmap& operator=(const ScopedBitmap&) = delete;
    ScopedBitmap(ScopedBitmap&& other) noexcept : bmp_(other.bmp_) { other.bmp_ = nullptr; }
    ScopedBitmap& operator=(ScopedBitmap&& other) noexcept {
        if (this != &other) { if (bmp_) DeleteObject(bmp_); bmp_ = other.bmp_; other.bmp_ = nullptr; }
        return *this;
    }
    HBITMAP Get() const { return bmp_; }
    HBITMAP Release() { HBITMAP b = bmp_; bmp_ = nullptr; return b; }
    void Reset(HBITMAP b = nullptr) { if (bmp_) DeleteObject(bmp_); bmp_ = b; }
    explicit operator bool() const { return bmp_ != nullptr; }
    HBITMAP operator->() const { return bmp_; }
};

// HBRUSH
class ScopedBrush {
    HBRUSH brush_ = nullptr;
public:
    ScopedBrush() = default;
    explicit ScopedBrush(HBRUSH b) : brush_(b) {}
    ~ScopedBrush() { if (brush_) DeleteObject(brush_); }
    ScopedBrush(const ScopedBrush&) = delete;
    ScopedBrush& operator=(const ScopedBrush&) = delete;
    ScopedBrush(ScopedBrush&& other) noexcept : brush_(other.brush_) { other.brush_ = nullptr; }
    ScopedBrush& operator=(ScopedBrush&& other) noexcept {
        if (this != &other) { if (brush_) DeleteObject(brush_); brush_ = other.brush_; other.brush_ = nullptr; }
        return *this;
    }
    HBRUSH Get() const { return brush_; }
    HBRUSH Release() { HBRUSH b = brush_; brush_ = nullptr; return b; }
    void Reset(HBRUSH b = nullptr) { if (brush_) DeleteObject(brush_); brush_ = b; }
    explicit operator bool() const { return brush_ != nullptr; }
};

// HPEN
class ScopedPen {
    HPEN pen_ = nullptr;
public:
    ScopedPen() = default;
    explicit ScopedPen(HPEN p) : pen_(p) {}
    ~ScopedPen() { if (pen_) DeleteObject(pen_); }
    ScopedPen(const ScopedPen&) = delete;
    ScopedPen& operator=(const ScopedPen&) = delete;
    ScopedPen(ScopedPen&& other) noexcept : pen_(other.pen_) { other.pen_ = nullptr; }
    ScopedPen& operator=(ScopedPen&& other) noexcept {
        if (this != &other) { if (pen_) DeleteObject(pen_); pen_ = other.pen_; other.pen_ = nullptr; }
        return *this;
    }
    HPEN Get() const { return pen_; }
    HPEN Release() { HPEN p = pen_; pen_ = nullptr; return p; }
    void Reset(HPEN p = nullptr) { if (pen_) DeleteObject(pen_); pen_ = p; }
    explicit operator bool() const { return pen_ != nullptr; }
};

// HFONT
class ScopedFont {
    HFONT font_ = nullptr;
public:
    ScopedFont() = default;
    explicit ScopedFont(HFONT f) : font_(f) {}
    ~ScopedFont() { if (font_) DeleteObject(font_); }
    ScopedFont(const ScopedFont&) = delete;
    ScopedFont& operator=(const ScopedFont&) = delete;
    ScopedFont(ScopedFont&& other) noexcept : font_(other.font_) { other.font_ = nullptr; }
    ScopedFont& operator=(ScopedFont&& other) noexcept {
        if (this != &other) { if (font_) DeleteObject(font_); font_ = other.font_; other.font_ = nullptr; }
        return *this;
    }
    HFONT Get() const { return font_; }
    HFONT Release() { HFONT f = font_; font_ = nullptr; return f; }
    void Reset(HFONT f = nullptr) { if (font_) DeleteObject(font_); font_ = f; }
    explicit operator bool() const { return font_ != nullptr; }
};

// HICON
class ScopedIcon {
    HICON icon_ = nullptr;
public:
    ScopedIcon() = default;
    explicit ScopedIcon(HICON i) : icon_(i) {}
    ~ScopedIcon() { if (icon_) DestroyIcon(icon_); }
    ScopedIcon(const ScopedIcon&) = delete;
    ScopedIcon& operator=(const ScopedIcon&) = delete;
    ScopedIcon(ScopedIcon&& other) noexcept : icon_(other.icon_) { other.icon_ = nullptr; }
    ScopedIcon& operator=(ScopedIcon&& other) noexcept {
        if (this != &other) { if (icon_) DestroyIcon(icon_); icon_ = other.icon_; other.icon_ = nullptr; }
        return *this;
    }
    HICON Get() const { return icon_; }
    HICON Release() { HICON i = icon_; icon_ = nullptr; return i; }
    void Reset(HICON i = nullptr) { if (icon_) DestroyIcon(icon_); icon_ = i; }
    explicit operator bool() const { return icon_ != nullptr; }
};

// HCURSOR
class ScopedCursor {
    HCURSOR cursor_ = nullptr;
public:
    ScopedCursor() = default;
    explicit ScopedCursor(HCURSOR c) : cursor_(c) {}
    ~ScopedCursor() { if (cursor_) DestroyCursor(cursor_); }
    ScopedCursor(const ScopedCursor&) = delete;
    ScopedCursor& operator=(const ScopedCursor&) = delete;
    ScopedCursor(ScopedCursor&& other) noexcept : cursor_(other.cursor_) { other.cursor_ = nullptr; }
    ScopedCursor& operator=(ScopedCursor&& other) noexcept {
        if (this != &other) { if (cursor_) DestroyCursor(cursor_); cursor_ = other.cursor_; other.cursor_ = nullptr; }
        return *this;
    }
    HCURSOR Get() const { return cursor_; }
    HCURSOR Release() { HCURSOR c = cursor_; cursor_ = nullptr; return c; }
    void Reset(HCURSOR c = nullptr) { if (cursor_) DestroyCursor(cursor_); cursor_ = c; }
    explicit operator bool() const { return cursor_ != nullptr; }
};

// HRGN
class ScopedRegion {
    HRGN rgn_ = nullptr;
public:
    ScopedRegion() = default;
    explicit ScopedRegion(HRGN r) : rgn_(r) {}
    ~ScopedRegion() { if (rgn_) DeleteObject(rgn_); }
    ScopedRegion(const ScopedRegion&) = delete;
    ScopedRegion& operator=(const ScopedRegion&) = delete;
    ScopedRegion(ScopedRegion&& other) noexcept : rgn_(other.rgn_) { other.rgn_ = nullptr; }
    ScopedRegion& operator=(ScopedRegion&& other) noexcept {
        if (this != &other) { if (rgn_) DeleteObject(rgn_); rgn_ = other.rgn_; other.rgn_ = nullptr; }
        return *this;
    }
    HRGN Get() const { return rgn_; }
    HRGN Release() { HRGN r = rgn_; rgn_ = nullptr; return r; }
    void Reset(HRGN r = nullptr) { if (rgn_) DeleteObject(rgn_); rgn_ = r; }
    explicit operator bool() const { return rgn_ != nullptr; }
};

// ============================================================================
// RAII para Kernel Objects
// ============================================================================

// HANDLE genérico (CloseHandle)
class ScopedHandle {
    HANDLE h_ = nullptr;
public:
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE h) : h_(h) {}
    ~ScopedHandle() { if (h_ && h_ != INVALID_HANDLE_VALUE) CloseHandle(h_); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : h_(other.h_) { other.h_ = nullptr; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) { if (h_ && h_ != INVALID_HANDLE_VALUE) CloseHandle(h_); h_ = other.h_; other.h_ = nullptr; }
        return *this;
    }
    HANDLE Get() const { return h_; }
    HANDLE Release() { HANDLE h = h_; h_ = nullptr; return h; }
    void Reset(HANDLE h = nullptr) { if (h_ && h_ != INVALID_HANDLE_VALUE) CloseHandle(h_); h_ = h; }
    explicit operator bool() const { return h_ && h_ != INVALID_HANDLE_VALUE; }
};

// HANDLE para mutex (ReleaseMutex)
class ScopedMutex {
    HANDLE h_ = nullptr;
public:
    ScopedMutex() = default;
    explicit ScopedMutex(HANDLE h) : h_(h) {}
    ~ScopedMutex() { if (h_) ReleaseMutex(h_); }
    ScopedMutex(const ScopedMutex&) = delete;
    ScopedMutex& operator=(const ScopedMutex&) = delete;
    ScopedMutex(ScopedMutex&& other) noexcept : h_(other.h_) { other.h_ = nullptr; }
    ScopedMutex& operator=(ScopedMutex&& other) noexcept {
        if (this != &other) { if (h_) ReleaseMutex(h_); h_ = other.h_; other.h_ = nullptr; }
        return *this;
    }
    HANDLE Get() const { return h_; }
    HANDLE Release() { HANDLE h = h_; h_ = nullptr; return h; }
    void Reset(HANDLE h = nullptr) { if (h_) ReleaseMutex(h_); h_ = h; }
    explicit operator bool() const { return h_ != nullptr; }
};

// HANDLE para file mapping (UnmapViewOfFile + CloseHandle)
class ScopedFileMapping {
    HANDLE h_ = nullptr;
    void* view_ = nullptr;
public:
    ScopedFileMapping() = default;
    ScopedFileMapping(HANDLE h, void* view) : h_(h), view_(view) {}
    ~ScopedFileMapping() { if (view_) UnmapViewOfFile(view_); if (h_) CloseHandle(h_); }
    ScopedFileMapping(const ScopedFileMapping&) = delete;
    ScopedFileMapping& operator=(const ScopedFileMapping&) = delete;
    ScopedFileMapping(ScopedFileMapping&& other) noexcept : h_(other.h_), view_(other.view_) { other.h_ = nullptr; other.view_ = nullptr; }
    ScopedFileMapping& operator=(ScopedFileMapping&& other) noexcept {
        if (this != &other) { if (view_) UnmapViewOfFile(view_); if (h_) CloseHandle(h_); h_ = other.h_; view_ = other.view_; other.h_ = nullptr; other.view_ = nullptr; }
        return *this;
    }
    HANDLE Handle() const { return h_; }
    void* View() const { return view_; }
    explicit operator bool() const { return h_ != nullptr; }
};

// ============================================================================
// RAII para COM (além do ComPtr)
// ============================================================================

// BSTR (SysFreeString)
class ScopedBSTR {
    BSTR str_ = nullptr;
public:
    ScopedBSTR() = default;
    explicit ScopedBSTR(BSTR s) : str_(s) {}
    ~ScopedBSTR() { if (str_) SysFreeString(str_); }
    ScopedBSTR(const ScopedBSTR&) = delete;
    ScopedBSTR& operator=(const ScopedBSTR&) = delete;
    ScopedBSTR(ScopedBSTR&& other) noexcept : str_(other.str_) { other.str_ = nullptr; }
    ScopedBSTR& operator=(ScopedBSTR&& other) noexcept {
        if (this != &other) { if (str_) SysFreeString(str_); str_ = other.str_; other.str_ = nullptr; }
        return *this;
    }
    BSTR Get() const { return str_; }
    BSTR Release() { BSTR s = str_; str_ = nullptr; return s; }
    void Reset(BSTR s = nullptr) { if (str_) SysFreeString(str_); str_ = s; }
    explicit operator bool() const { return str_ != nullptr; }
};

// SAFEARRAY (SafeArrayDestroy)
class ScopedSafeArray {
    SAFEARRAY* arr_ = nullptr;
public:
    ScopedSafeArray() = default;
    explicit ScopedSafeArray(SAFEARRAY* a) : arr_(a) {}
    ~ScopedSafeArray() { if (arr_) SafeArrayDestroy(arr_); }
    ScopedSafeArray(const ScopedSafeArray&) = delete;
    ScopedSafeArray& operator=(const ScopedSafeArray&) = delete;
    ScopedSafeArray(ScopedSafeArray&& other) noexcept : arr_(other.arr_) { other.arr_ = nullptr; }
    ScopedSafeArray& operator=(ScopedSafeArray&& other) noexcept {
        if (this != &other) { if (arr_) SafeArrayDestroy(arr_); arr_ = other.arr_; other.arr_ = nullptr; }
        return *this;
    }
    SAFEARRAY* Get() const { return arr_; }
    SAFEARRAY* Release() { SAFEARRAY* a = arr_; arr_ = nullptr; return a; }
    void Reset(SAFEARRAY* a = nullptr) { if (arr_) SafeArrayDestroy(arr_); arr_ = a; }
    explicit operator bool() const { return arr_ != nullptr; }
};

// PROPVARIANT (PropVariantClear)
class ScopedPropVariant {
    PROPVARIANT var_;
    bool initialized_ = false;
public:
    ScopedPropVariant() { PropVariantInit(&var_); initialized_ = true; }
    ~ScopedPropVariant() { if (initialized_) PropVariantClear(&var_); }
    ScopedPropVariant(const ScopedPropVariant&) = delete;
    ScopedPropVariant& operator=(const ScopedPropVariant&) = delete;
    ScopedPropVariant(ScopedPropVariant&& other) noexcept : var_(other.var_), initialized_(other.initialized_) { other.initialized_ = false; }
    ScopedPropVariant& operator=(ScopedPropVariant&& other) noexcept {
        if (this != &other) { if (initialized_) PropVariantClear(&var_); var_ = other.var_; initialized_ = other.initialized_; other.initialized_ = false; }
        return *this;
    }
    PROPVARIANT* Get() { return &var_; }
    const PROPVARIANT* Get() const { return &var_; }
    explicit operator bool() const { return initialized_; }
};

// CoTaskMemFree
class ScopedCoTaskMem {
    void* ptr_ = nullptr;
public:
    ScopedCoTaskMem() = default;
    explicit ScopedCoTaskMem(void* p) : ptr_(p) {}
    ~ScopedCoTaskMem() { if (ptr_) CoTaskMemFree(ptr_); }
    ScopedCoTaskMem(const ScopedCoTaskMem&) = delete;
    ScopedCoTaskMem& operator=(const ScopedCoTaskMem&) = delete;
    ScopedCoTaskMem(ScopedCoTaskMem&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ScopedCoTaskMem& operator=(ScopedCoTaskMem&& other) noexcept {
        if (this != &other) { if (ptr_) CoTaskMemFree(ptr_); ptr_ = other.ptr_; other.ptr_ = nullptr; }
        return *this;
    }
    void* Get() const { return ptr_; }
    void* Release() { void* p = ptr_; ptr_ = nullptr; return p; }
    template <typename T> T* GetAs() const { return static_cast<T*>(ptr_); }
    void Reset(void* p = nullptr) { if (ptr_) CoTaskMemFree(ptr_); ptr_ = p; }
    explicit operator bool() const { return ptr_ != nullptr; }
};

// ============================================================================
// RAII para Windows Objects
// ============================================================================

// HWND (DestroyWindow)
class ScopedWindow {
    HWND hwnd_ = nullptr;
public:
    ScopedWindow() = default;
    explicit ScopedWindow(HWND h) : hwnd_(h) {}
    ~ScopedWindow() { if (hwnd_) DestroyWindow(hwnd_); }
    ScopedWindow(const ScopedWindow&) = delete;
    ScopedWindow& operator=(const ScopedWindow&) = delete;
    ScopedWindow(ScopedWindow&& other) noexcept : hwnd_(other.hwnd_) { other.hwnd_ = nullptr; }
    ScopedWindow& operator=(ScopedWindow&& other) noexcept {
        if (this != &other) { if (hwnd_) DestroyWindow(hwnd_); hwnd_ = other.hwnd_; other.hwnd_ = nullptr; }
        return *this;
    }
    HWND Get() const { return hwnd_; }
    HWND Release() { HWND h = hwnd_; hwnd_ = nullptr; return h; }
    void Reset(HWND h = nullptr) { if (hwnd_) DestroyWindow(hwnd_); hwnd_ = h; }
    explicit operator bool() const { return hwnd_ != nullptr; }
};

// HACCEL (DestroyAcceleratorTable)
class ScopedAccelerator {
    HACCEL accel_ = nullptr;
public:
    ScopedAccelerator() = default;
    explicit ScopedAccelerator(HACCEL a) : accel_(a) {}
    ~ScopedAccelerator() { if (accel_) DestroyAcceleratorTable(accel_); }
    ScopedAccelerator(const ScopedAccelerator&) = delete;
    ScopedAccelerator& operator=(const ScopedAccelerator&) = delete;
    ScopedAccelerator(ScopedAccelerator&& other) noexcept : accel_(other.accel_) { other.accel_ = nullptr; }
    ScopedAccelerator& operator=(ScopedAccelerator&& other) noexcept {
        if (this != &other) { if (accel_) DestroyAcceleratorTable(accel_); accel_ = other.accel_; other.accel_ = nullptr; }
        return *this;
    }
    HACCEL Get() const { return accel_; }
    HACCEL Release() { HACCEL a = accel_; accel_ = nullptr; return a; }
    void Reset(HACCEL a = nullptr) { if (accel_) DestroyAcceleratorTable(accel_); accel_ = a; }
    explicit operator bool() const { return accel_ != nullptr; }
};

// HMENU (DestroyMenu)
class ScopedMenu {
    HMENU menu_ = nullptr;
public:
    ScopedMenu() = default;
    explicit ScopedMenu(HMENU m) : menu_(m) {}
    ~ScopedMenu() { if (menu_) DestroyMenu(menu_); }
    ScopedMenu(const ScopedMenu&) = delete;
    ScopedMenu& operator=(const ScopedMenu&) = delete;
    ScopedMenu(ScopedMenu&& other) noexcept : menu_(other.menu_) { other.menu_ = nullptr; }
    ScopedMenu& operator=(ScopedMenu&& other) noexcept {
        if (this != &other) { if (menu_) DestroyMenu(menu_); menu_ = other.menu_; other.menu_ = nullptr; }
        return *this;
    }
    HMENU Get() const { return menu_; }
    HMENU Release() { HMENU m = menu_; menu_ = nullptr; return m; }
    void Reset(HMENU m = nullptr) { if (menu_) DestroyMenu(menu_); menu_ = m; }
    explicit operator bool() const { return menu_ != nullptr; }
};

// ============================================================================
// RAII para DC/HDc
// ============================================================================

// HDC de janela (ReleaseDC)
class ScopedWindowDC {
    HWND hwnd_ = nullptr;
    HDC dc_ = nullptr;
public:
    ScopedWindowDC() = default;
    ScopedWindowDC(HWND h, HDC dc) : hwnd_(h), dc_(dc) {}
    ~ScopedWindowDC() { if (hwnd_ && dc_) ReleaseDC(hwnd_, dc_); }
    ScopedWindowDC(const ScopedWindowDC&) = delete;
    ScopedWindowDC& operator=(const ScopedWindowDC&) = delete;
    ScopedWindowDC(ScopedWindowDC&& other) noexcept : hwnd_(other.hwnd_), dc_(other.dc_) { other.hwnd_ = nullptr; other.dc_ = nullptr; }
    ScopedWindowDC& operator=(ScopedWindowDC&& other) noexcept {
        if (this != &other) { if (hwnd_ && dc_) ReleaseDC(hwnd_, dc_); hwnd_ = other.hwnd_; dc_ = other.dc_; other.hwnd_ = nullptr; other.dc_ = nullptr; }
        return *this;
    }
    HDC Get() const { return dc_; }
    explicit operator bool() const { return dc_ != nullptr; }
};

// HDC de memória (DeleteDC)
class ScopedMemDC {
    HDC dc_ = nullptr;
public:
    ScopedMemDC() = default;
    explicit ScopedMemDC(HDC dc) : dc_(dc) {}
    ~ScopedMemDC() { if (dc_) DeleteDC(dc_); }
    ScopedMemDC(const ScopedMemDC&) = delete;
    ScopedMemDC& operator=(const ScopedMemDC&) = delete;
    ScopedMemDC(ScopedMemDC&& other) noexcept : dc_(other.dc_) { other.dc_ = nullptr; }
    ScopedMemDC& operator=(ScopedMemDC&& other) noexcept {
        if (this != &other) { if (dc_) DeleteDC(dc_); dc_ = other.dc_; other.dc_ = nullptr; }
        return *this;
    }
    HDC Get() const { return dc_; }
    HDC Release() { HDC d = dc_; dc_ = nullptr; return d; }
    void Reset(HDC dc = nullptr) { if (dc_) DeleteDC(dc_); dc_ = dc; }
    explicit operator bool() const { return dc_ != nullptr; }
};

// ============================================================================
// RAII para GDI Selection (SelectObject)
// ============================================================================

template <typename T>
class ScopedSelect {
    HDC dc_ = nullptr;
    T old_ = nullptr;
public:
    ScopedSelect() = default;
    ScopedSelect(HDC dc, T obj) : dc_(dc), old_(static_cast<T>(SelectObject(dc, obj))) {}
    ~ScopedSelect() { if (dc_ && old_) SelectObject(dc_, old_); }
    ScopedSelect(const ScopedSelect&) = delete;
    ScopedSelect& operator=(const ScopedSelect&) = delete;
    ScopedSelect(ScopedSelect&& other) noexcept : dc_(other.dc_), old_(other.old_) { other.dc_ = nullptr; other.old_ = nullptr; }
    ScopedSelect& operator=(ScopedSelect&& other) noexcept {
        if (this != &other) { if (dc_ && old_) SelectObject(dc_, old_); dc_ = other.dc_; old_ = other.old_; other.dc_ = nullptr; other.old_ = nullptr; }
        return *this;
    }
    T Old() const { return old_; }
    explicit operator bool() const { return dc_ != nullptr; }
};

// ============================================================================
// Helpers para criação de recursos
// ============================================================================

inline ScopedBitmap CreateBitmap(int w, int h, int planes = 1, int bits = 32, const void* bits_ptr = nullptr) {
    return ScopedBitmap(CreateBitmap(w, h, planes, bits, bits_ptr));
}

inline ScopedBitmap CreateCompatibleBitmap(HDC dc, int w, int h) {
    return ScopedBitmap(::CreateCompatibleBitmap(dc, w, h));
}

inline ScopedBrush CreateSolidBrush(COLORREF color) {
    return ScopedBrush(::CreateSolidBrush(color));
}

inline ScopedPen CreatePen(int style, int width, COLORREF color) {
    return ScopedPen(::CreatePen(style, width, color));
}

inline ScopedFont CreateFont(int height, int width, int escapement, int orientation, int weight,
                             DWORD italic, DWORD underline, DWORD strikeout, DWORD charset,
                             DWORD outPrecision, DWORD clipPrecision, DWORD quality,
                             DWORD pitchAndFamily, const wchar_t* face) {
    return ScopedFont(::CreateFontW(height, width, escapement, orientation, weight,
                                     italic, underline, strikeout, charset,
                                     outPrecision, clipPrecision, quality,
                                     pitchAndFamily, face));
}

inline ScopedIcon LoadIcon(HINSTANCE inst, LPCWSTR name) {
    return ScopedIcon(::LoadIconW(inst, name));
}

inline ScopedCursor LoadCursor(HINSTANCE inst, LPCWSTR name) {
    return ScopedCursor(::LoadCursorW(inst, name));
}

inline ScopedRegion CreateRectRgn(int x1, int y1, int x2, int y2) {
    return ScopedRegion(::CreateRectRgn(x1, y1, x2, y2));
}

inline ScopedRegion CreateRoundRectRgn(int x1, int y1, int x2, int y2, int w, int h) {
    return ScopedRegion(::CreateRoundRectRgn(x1, y1, x2, y2, w, h));
}

inline ScopedHandle CreateEvent(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initial, LPCWSTR name) {
    return ScopedHandle(::CreateEventW(sa, manual, initial, name));
}

inline ScopedHandle CreateMutex(LPSECURITY_ATTRIBUTES sa, BOOL initial, LPCWSTR name) {
    return ScopedHandle(::CreateMutexW(sa, initial, name));
}

inline ScopedHandle OpenProcess(DWORD access, BOOL inherit, DWORD pid) {
    return ScopedHandle(::OpenProcess(access, inherit, pid));
}

inline ScopedWindow CreateWindowEx(DWORD exStyle, LPCWSTR className, LPCWSTR windowName,
                                    DWORD style, int x, int y, int w, int h,
                                    HWND parent, HMENU menu, HINSTANCE inst, LPVOID param) {
    return ScopedWindow(::CreateWindowExW(exStyle, className, windowName, style, x, y, w, h, parent, menu, inst, param));
}

// ============================================================================
// Deleter personalizado para std::unique_ptr
// ============================================================================

struct BitmapDeleter { void operator()(HBITMAP b) const { if (b) DeleteObject(b); } };
struct BrushDeleter { void operator()(HBRUSH b) const { if (b) DeleteObject(b); } };
struct PenDeleter { void operator()(HPEN p) const { if (p) DeleteObject(p); } };
struct FontDeleter { void operator()(HFONT f) const { if (f) DeleteObject(f); } };
struct IconDeleter { void operator()(HICON i) const { if (i) DestroyIcon(i); } };
struct CursorDeleter { void operator()(HCURSOR c) const { if (c) DestroyCursor(c); } };
struct RegionDeleter { void operator()(HRGN r) const { if (r) DeleteObject(r); } };
struct HandleDeleter { void operator()(HANDLE h) const { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); } };
struct WindowDeleter { void operator()(HWND h) const { if (h) DestroyWindow(h); } };
struct MenuDeleter { void operator()(HMENU m) const { if (m) DestroyMenu(m); } };
struct DcDeleter { void operator()(HDC dc) const { if (dc) DeleteDC(dc); } };
struct BstrDeleter { void operator()(BSTR s) const { if (s) SysFreeString(s); } };
struct CoTaskMemDeleter { void operator()(void* p) const { if (p) CoTaskMemFree(p); } };

using UniqueBitmap = std::unique_ptr<std::remove_pointer<HBITMAP>::type, BitmapDeleter>;
using UniqueBrush = std::unique_ptr<std::remove_pointer<HBRUSH>::type, BrushDeleter>;
using UniquePen = std::unique_ptr<std::remove_pointer<HPEN>::type, PenDeleter>;
using UniqueFont = std::unique_ptr<std::remove_pointer<HFONT>::type, FontDeleter>;
using UniqueIcon = std::unique_ptr<std::remove_pointer<HICON>::type, IconDeleter>;
using UniqueCursor = std::unique_ptr<std::remove_pointer<HCURSOR>::type, CursorDeleter>;
using UniqueRegion = std::unique_ptr<std::remove_pointer<HRGN>::type, RegionDeleter>;
using UniqueHandle = std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleDeleter>;
using UniqueWindow = std::unique_ptr<std::remove_pointer<HWND>::type, WindowDeleter>;
using UniqueMenu = std::unique_ptr<std::remove_pointer<HMENU>::type, MenuDeleter>;
using UniqueMemDC = std::unique_ptr<std::remove_pointer<HDC>::type, DcDeleter>;
using UniqueBSTR = std::unique_ptr<std::remove_pointer<BSTR>::type, BstrDeleter>;
using UniqueCoTaskMem = std::unique_ptr<void, CoTaskMemDeleter>;

} // namespace res