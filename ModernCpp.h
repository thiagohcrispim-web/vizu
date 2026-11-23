// ModernCpp.h - Utilitários C++17/20 para modernizar o código
// Substitui padrões C++98/03 por equivalentes modernos
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include <variant>
#include <memory>
#include <functional>
#include <algorithm>
#include <iterator>
#include <chrono>
#include <filesystem>
#include <format>
#include <span>
#include <concepts>
#include <ranges>
#include <expected>
#include <source_location>

namespace modern {

// ============================================================================
// String utilities (C++17/20)
// ============================================================================

// string_view para evitar cópias desnecessárias
using StringView = std::wstring_view;
using String = std::wstring;

// Converte wstring para string (UTF-8)
inline std::string ToUtf8(StringView sv) {
    if (sv.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, sv.data(), static_cast<int>(sv.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, sv.data(), static_cast<int>(sv.size()), result.data(), size, nullptr, nullptr);
    return result;
}

// Converte string (UTF-8) para wstring
inline String FromUtf8(std::string_view sv) {
    if (sv.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, sv.data(), static_cast<int>(sv.size()), nullptr, 0);
    String result(size, '\0');
    MultiByteToWideChar(CP_UTF8, 0, sv.data(), static_cast<int>(sv.size()), result.data(), size);
    return result;
}

// Formatação type-safe (C++20 std::format, fallback para fmtlib)
template <typename... Args>
inline String Format(StringView fmt, Args&&... args) {
    try {
        return std::vformat(fmt, std::make_wformat_args(args...));
    } catch (...) {
        return L"[format error]";
    }
}

// ============================================================================
// Filesystem (C++17)
// ============================================================================
namespace fs = std::filesystem;

inline String PathToString(const fs::path& p) { return p.wstring(); }
inline fs::path StringToPath(StringView s) { return fs::path(s); }

inline bool FileExists(StringView path) { return fs::exists(StringToPath(path)); }
inline bool IsDirectory(StringView path) { return fs::is_directory(StringToPath(path)); }
inline uintmax_t FileSize(StringView path) { return fs::file_size(StringToPath(path)); }

inline String ParentPath(StringView path) { return StringToPath(path).parent_path().wstring(); }
inline String FileName(StringView path) { return StringToPath(path).filename().wstring(); }
inline String Extension(StringView path) { return StringToPath(path).extension().wstring(); }
inline String Stem(StringView path) { return StringToPath(path).stem().wstring(); }

inline std::vector<String> ListDirectory(StringView path, bool recursive = false) {
    std::vector<String> result;
    try {
        if (recursive) {
            for (auto& p : fs::recursive_directory_iterator(StringToPath(path))) {
                if (p.is_regular_file()) result.push_back(p.path().wstring());
            }
        } else {
            for (auto& p : fs::directory_iterator(StringToPath(path))) {
                if (p.is_regular_file()) result.push_back(p.path().wstring());
            }
        }
    } catch (...) {}
    return result;
}

// ============================================================================
// Time utilities (C++20 chrono)
// ============================================================================
using TimePoint = std::chrono::steady_clock::time_point;
using SystemTimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::steady_clock::duration;

inline TimePoint Now() { return std::chrono::steady_clock::now(); }
inline SystemTimePoint SystemNow() { return std::chrono::system_clock::now(); }

inline double ElapsedSeconds(TimePoint start, TimePoint end = Now()) {
    return std::chrono::duration<double>(end - start).count();
}

inline uint64_t ElapsedMs(TimePoint start, TimePoint end = Now()) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

inline String FormatDuration(Duration d) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    if (ms < 1000) return Format(L"{}ms", ms);
    auto s = ms / 1000;
    if (s < 60) return Format(L"{:.1f}s", ms / 1000.0);
    auto m = s / 60;
    return Format(L"{}m {}s", m, s % 60);
}

// ============================================================================
// Optional/Expected patterns
// ============================================================================

// Helper para optional com valor padrão
template <typename T>
inline T ValueOr(const std::optional<T>& opt, T&& defaultValue) {
    return opt.value_or(std::forward<T>(defaultValue));
}

// Expected para error handling (C++23, polyfill para C++20)
template <typename T, typename E = std::error_code>
using Expected = std::expected<T, E>;

// ============================================================================
// Span para arrays não-owning (C++20)
// ============================================================================
template <typename T>
using Span = std::span<T>;

template <typename T, size_t N>
constexpr Span<T> MakeSpan(T (&arr)[N]) { return Span<T>(arr, N); }

template <typename T>
constexpr Span<T> MakeSpan(std::vector<T>& vec) { return Span<T>(vec.data(), vec.size()); }

template <typename T>
constexpr Span<const T> MakeSpan(const std::vector<T>& vec) { return Span<const T>(vec.data(), vec.size()); }

// ============================================================================
// Range algorithms (C++20)
// ============================================================================
namespace rv = std::ranges::views;

// Exemplo de uso:
// auto filtered = vec | rv::filter([](int x){ return x > 0; }) | rv::transform([](int x){ return x * 2; });

// ============================================================================
// Concepts para constraints (C++20)
// ============================================================================
template <typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept StringLike = std::convertible_to<T, StringView>;

template <typename T>
concept Callable = std::invocable<T>;

// ============================================================================
// Source location para debug (C++20)
// ============================================================================
using SourceLoc = std::source_location;

inline String CurrentLocation(SourceLoc loc = SourceLoc::current()) {
    return Format(L"{}:{} in {}", loc.file_name(), loc.line(), loc.function_name());
}

// ============================================================================
// Smart pointer helpers
// ============================================================================
template <typename T, typename... Args>
inline std::unique_ptr<T> MakeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
inline std::shared_ptr<T> MakeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// ============================================================================
// Scope guard (RAII para cleanup)
// ============================================================================
template <typename F>
class ScopeGuard {
    F func_;
    bool active_ = true;
public:
    explicit ScopeGuard(F&& f) : func_(std::forward<F>(f)) {}
    ~ScopeGuard() { if (active_) func_(); }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&& other) noexcept : func_(std::move(other.func_)), active_(other.active_) { other.active_ = false; }
    void Dismiss() { active_ = false; }
};

template <typename F>
inline ScopeGuard<F> MakeScopeGuard(F&& f) {
    return ScopeGuard<F>(std::forward<F>(f));
}

// ============================================================================
// Bit flags type-safe (C++20)
// ============================================================================
template <typename Enum>
concept EnumType = std::is_enum_v<Enum>;

template <EnumType Enum>
class Flags {
    using Underlying = std::underlying_type_t<Enum>;
    Underlying value_ = 0;
public:
    constexpr Flags() = default;
    constexpr Flags(Enum e) : value_(static_cast<Underlying>(e)) {}
    constexpr Flags(Underlying v) : value_(v) {}

    constexpr Flags& operator|=(Enum e) { value_ |= static_cast<Underlying>(e); return *this; }
    constexpr Flags& operator&=(Enum e) { value_ &= static_cast<Underlying>(e); return *this; }
    constexpr Flags& operator^=(Enum e) { value_ ^= static_cast<Underlying>(e); return *this; }

    constexpr friend Flags operator|(Flags a, Enum b) { return a |= b; }
    constexpr friend Flags operator&(Flags a, Enum b) { return a &= b; }
    constexpr friend Flags operator^(Flags a, Enum b) { return a ^= b; }
    constexpr friend Flags operator~(Flags a) { return Flags(~a.value_); }

    constexpr explicit operator bool() const { return value_ != 0; }
    constexpr bool Has(Enum e) const { return (value_ & static_cast<Underlying>(e)) != 0; }
    constexpr Underlying Value() const { return value_; }
};

// ============================================================================
// Result type para operações que podem falhar
// ============================================================================
template <typename T>
class Result {
    std::variant<T, std::wstring> value_;
public:
    Result(const T& v) : value_(v) {}
    Result(T&& v) : value_(std::move(v)) {}
    Result(const std::wstring& err) : value_(err) {}
    Result(std::wstring&& err) : value_(std::move(err)) {}

    bool Ok() const { return std::holds_alternative<T>(value_); }
    bool Error() const { return std::holds_alternative<std::wstring>(value_); }

    T& Value() { return std::get<T>(value_); }
    const T& Value() const { return std::get<T>(value_); }
    const std::wstring& ErrorMsg() const { return std::get<std::wstring>(value_); }

    T ValueOr(const T& def) const { return Ok() ? Value() : def; }
    T ValueOr(T&& def) const { return Ok() ? Value() : std::move(def); }
};

// ============================================================================
// Enumeração de arquivos com filter (C++20 ranges)
// ============================================================================
inline auto EnumerateFiles(StringView path, StringView ext = L"", bool recursive = false) {
    auto files = ListDirectory(path, recursive);
    if (ext.empty()) return files | rv::filter([](const String&){ return true; });

    return files | rv::filter([ext](const String& f) {
        return Extension(f) == ext;
    });
}

// ============================================================================
// Byte utilities
// ============================================================================
inline std::vector<uint8_t> ReadFileBytes(StringView path) {
    std::vector<uint8_t> data;
    HANDLE h = CreateFileW(path.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return data;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) { CloseHandle(h); return data; }

    data.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    ReadFile(h, data.data(), static_cast<DWORD>(data.size()), &read, nullptr);
    CloseHandle(h);
    if (read != data.size()) data.resize(read);
    return data;
}

inline bool WriteFileBytes(StringView path, std::span<const uint8_t> data) {
    HANDLE h = CreateFileW(path.data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    bool ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
    FlushFileBuffers(h);
    CloseHandle(h);
    return ok && written == data.size();
}

// ============================================================================
// Windows API wrappers modernizados
// ============================================================================

// GetWindowRect -> RECT
inline RECT GetWindowRect(HWND hwnd) {
    RECT rc; ::GetWindowRect(hwnd, &rc); return rc;
}

inline RECT GetClientRect(HWND hwnd) {
    RECT rc; ::GetClientRect(hwnd, &rc); return rc;
}

// GetCursorPos -> POINT
inline POINT GetCursorPos() {
    POINT pt; ::GetCursorPos(&pt); return pt;
}

// ScreenToClient
inline POINT ScreenToClient(HWND hwnd, POINT pt) {
    ::ScreenToClient(hwnd, &pt); return pt;
}

// ============================================================================
// DPI helpers
// ============================================================================
inline UINT GetDpi(HWND hwnd) { return GetDpiForWindow(hwnd); }
inline float GetScale(HWND hwnd) { return GetDpiForWindow(hwnd) / 96.0f; }
inline D2D1_SIZE_F GetScaleD2D(HWND hwnd) {
    float s = GetScale(hwnd);
    return D2D1::SizeF(s, s);
}

// ============================================================================
// COM smart pointer aliases
// ============================================================================
#include <wrl/client.h>
template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

inline ComPtr<IUnknown> MakeComPtr(IUnknown* ptr) { return ComPtr<IUnknown>(ptr); }

// ============================================================================
// Threading helpers
// ============================================================================
inline void RunAsync(std::function<void()>&& fn) {
    std::thread([fn = std::move(fn)]() mutable { fn(); }).detach();
}

template <typename F, typename... Args>
inline auto RunAsyncWithResult(F&& f, Args&&... args) {
    using Ret = std::invoke_result_t<F, Args...>;
    std::packaged_task<Ret()> task(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto future = task.get_future();
    std::thread(std::move(task)).detach();
    return future;
}

// ============================================================================
// Algorithm helpers
// ============================================================================
template <typename Container, typename Pred>
inline bool Contains(const Container& c, Pred&& pred) {
    return std::ranges::any_of(c, std::forward<Pred>(pred));
}

template <typename Container, typename T>
inline bool Contains(const Container& c, const T& value) {
    return std::ranges::contains(c, value);
}

template <typename Container, typename Pred>
inline auto FindIf(Container& c, Pred&& pred) {
    return std::ranges::find_if(c, std::forward<Pred>(pred));
}

template <typename Container, typename T>
inline auto Find(Container& c, const T& value) {
    return std::ranges::find(c, value);
}

template <typename Container, typename Pred>
inline size_t EraseIf(Container& c, Pred&& pred) {
    return std::erase_if(c, std::forward<Pred>(pred));
}

// ============================================================================
// Math helpers
// ============================================================================
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
inline float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }
inline float SmootherStep(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

// ============================================================================
// Color helpers
// ============================================================================
inline D2D1_COLOR_F MakeColor(uint32_t argb, float alpha = 1.0f) {
    return D2D1::ColorF(
        ((argb >> 16) & 0xFF) / 255.0f,
        ((argb >> 8) & 0xFF) / 255.0f,
        (argb & 0xFF) / 255.0f,
        alpha
    );
}

inline D2D1_COLOR_F MakeColor(uint8_t r, uint8_t g, uint8_t b, float a = 1.0f) {
    return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

inline uint32_t ToArgb(D2D1_COLOR_F c) {
    return (static_cast<uint32_t>(c.a * 255) << 24) |
           (static_cast<uint32_t>(c.r * 255) << 16) |
           (static_cast<uint32_t>(c.g * 255) << 8) |
           static_cast<uint32_t>(c.b * 255);
}

} // namespace modern