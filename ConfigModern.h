// ConfigModern.h - Configuração modernizada (C++17/20)
// Substitui Config.h/cpp por versão type-safe com validação
#pragma once
#include <windows.h>
#include <string>
#include <optional>
#include <array>
#include <mutex>
#include "ModernCpp.h"

namespace cfg {

// ============================================================================
// Configuração type-safe
// ============================================================================
struct Config {
    // Preview
    bool spacePreview = true;
    bool altButtons = true;
    uint32_t altDelay = 500;      // ms, 150-3000
    uint32_t jpgQuality = 75;     // 50-100
    uint32_t webpQuality = 75;    // 30-100

    // Tema
    enum class ThemeMode : uint32_t {
        Windows = 0,  // Segue sistema
        Light   = 1,
        Dark    = 2,
        Crispy  = 3,
        Classic = 4,
        Count
    };
    ThemeMode themeMode = ThemeMode::Dark;

    // Cores
    uint32_t neonColor = 0xFF6633;  // RGB

    // Features
    bool trayIcon = true;
    bool autoUpdate = true;
    bool colorPicker = true;
    bool dpi300 = false;
    bool pdfRaw = false;
    bool vectorTextPaths = false;
    bool tempPermanent = false;

    // Hotkeys (packed: vk | (mod << 16))
    uint32_t ocrHotkey = 0;
    uint32_t cleanHotkey = 0;

    // UI
    std::wstring uiFont;  // Vazio = padrão (Inter)

    // Validação e clamping
    void Validate() {
        altDelay = std::clamp(altDelay, 150u, 3000u);
        jpgQuality = std::clamp(jpgQuality, 50u, 100u);
        webpQuality = std::clamp(webpQuality, 30u, 100u);
        themeMode = static_cast<ThemeMode>(std::min(
            static_cast<uint32_t>(themeMode),
            static_cast<uint32_t>(ThemeMode::Count) - 1));
        neonColor &= 0xFFFFFF;
    }
};

// ============================================================================
// Chaves do registro
// ============================================================================
inline constexpr std::wstring_view kRegKey = L"Software\\DesignThumbs";

struct RegKey {
    static constexpr std::wstring_view SpacePreview       = L"SpacePreview";
    static constexpr std::wstring_view AltButtons         = L"AltButtons";
    static constexpr std::wstring_view AltDelay           = L"AltDelay";
    static constexpr std::wstring_view JpgQuality         = L"JpgQuality";
    static constexpr std::wstring_view WebpQuality        = L"WebpQuality";
    static constexpr std::wstring_view Theme2             = L"Theme2";
    static constexpr std::wstring_view NeonColor          = L"NeonColor";
    static constexpr std::wstring_view TrayIcon           = L"TrayIcon";
    static constexpr std::wstring_view AutoUpdate         = L"AutoUpdate";
    static constexpr std::wstring_view ColorPicker        = L"ColorPicker";
    static constexpr std::wstring_view ConvertDpi         = L"ConvertDpi";
    static constexpr std::wstring_view PdfUncompressed    = L"PdfUncompressed";
    static constexpr std::wstring_view VectorTextPaths    = L"VectorTextPaths";
    static constexpr std::wstring_view TempPermanent      = L"TempPermanent";
    static constexpr std::wstring_view UiFont             = L"UiFont";
    static constexpr std::wstring_view UiFontMigrated     = L"UiFontMigrated";
    static constexpr std::wstring_view OcrHotkey          = L"OcrHotkey";
    static constexpr std::wstring_view CleanHotkey        = L"CleanHotkey";
    static constexpr std::wstring_view Edition            = L"Edition";
    static constexpr std::wstring_view VectorConverterInstalled = L"VectorConverterInstalled";
    static constexpr std::wstring_view SettingsPreset     = L"SettingsPreset";
};

// ============================================================================
// Registry helpers type-safe
// ============================================================================
inline std::optional<uint32_t> ReadDword(std::wstring_view name, uint32_t def = 0) {
    uint32_t value = def;
    DWORD size = sizeof(value);
    LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER, kRegKey.data(), name.data(),
        RRF_RT_REG_DWORD, nullptr, &value, &size
    );
    return status == ERROR_SUCCESS ? std::make_optional(value) : std::nullopt;
}

inline void WriteDword(std::wstring_view name, uint32_t value) {
    RegSetKeyValueW(
        HKEY_CURRENT_USER, kRegKey.data(), name.data(),
        REG_DWORD, &value, sizeof(value)
    );
}

inline std::optional<std::wstring> ReadString(std::wstring_view name) {
    wchar_t buffer[1024];
    DWORD size = sizeof(buffer);
    LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER, kRegKey.data(), name.data(),
        RRF_RT_REG_SZ, nullptr, buffer, &size
    );
    return status == ERROR_SUCCESS ? std::make_optional(std::wstring(buffer)) : std::nullopt;
}

inline void WriteString(std::wstring_view name, std::wstring_view value) {
    RegSetKeyValueW(
        HKEY_CURRENT_USER, kRegKey.data(), name.data(),
        REG_SZ, value.data(), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))
    );
}

inline void DeleteKey(std::wstring_view name) {
    RegDeleteKeyValueW(HKEY_CURRENT_USER, kRegKey.data(), name.data());
}

// ============================================================================
// ConfigManager - Singleton thread-safe
// ============================================================================
class ConfigManager {
    Config config_;
    mutable std::mutex mu_;
    bool loaded_ = false;

    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void LoadFromRegistry() {
        Config cfg;

        // Preview
        cfg.spacePreview = ReadDword(RegKey::SpacePreview, 1).value_or(1) != 0;
        cfg.altButtons = ReadDword(RegKey::AltButtons, 1).value_or(1) != 0;
        cfg.altDelay = ReadDword(RegKey::AltDelay, 500).value_or(500);
        cfg.jpgQuality = ReadDword(RegKey::JpgQuality, 75).value_or(75);
        cfg.webpQuality = ReadDword(RegKey::WebpQuality, 75).value_or(75);

        // Tema (migração v9.2)
        auto oldTheme = ReadDword(L"Theme", 0).value_or(0);
        static constexpr std::array<ThemeMode, 4> kMigration = {
            ThemeMode::Windows, ThemeMode::Light, ThemeMode::Dark, ThemeMode::Crispy
        };
        cfg.themeMode = static_cast<ThemeMode>(
            ReadDword(RegKey::Theme2, static_cast<uint32_t>(kMigration[std::min<uint32_t>(oldTheme, 3)])).value_or(
                static_cast<uint32_t>(ThemeMode::Dark))
        );

        // Cores
        cfg.neonColor = ReadDword(RegKey::NeonColor, 0xFF6633).value_or(0xFF6633) & 0xFFFFFF;

        // Features
        cfg.trayIcon = ReadDword(RegKey::TrayIcon, 1).value_or(1) != 0;
        cfg.autoUpdate = ReadDword(RegKey::AutoUpdate, 1).value_or(1) != 0;
        cfg.colorPicker = ReadDword(RegKey::ColorPicker, 1).value_or(1) != 0;
        cfg.dpi300 = ReadDword(RegKey::ConvertDpi, 72).value_or(72) == 300;
        cfg.pdfRaw = ReadDword(RegKey::PdfUncompressed, 0).value_or(0) != 0;
        cfg.vectorTextPaths = ReadDword(RegKey::VectorTextPaths, 0).value_or(0) != 0;
        cfg.tempPermanent = ReadDword(RegKey::TempPermanent, 0).value_or(0) != 0;

        // UI
        cfg.uiFont = ReadString(RegKey::UiFont).value_or(L"");

        // Hotkeys
        cfg.ocrHotkey = ReadDword(RegKey::OcrHotkey, 0).value_or(0);
        cfg.cleanHotkey = ReadDword(RegKey::CleanHotkey, 0).value_or(0);

        cfg.Validate();
        config_ = std::move(cfg);
        loaded_ = true;
    }

public:
    static ConfigManager& Instance() {
        static ConfigManager instance;
        return instance;
    }

    // Carrega configuração (thread-safe, idempotente)
    void Load() {
        std::lock_guard<std::mutex> lk(mu_);
        if (!loaded_) LoadFromRegistry();
    }

    // Salva configuração atual
    void Save() const {
        std::lock_guard<std::mutex> lk(mu_);

        WriteDword(RegKey::SpacePreview, config_.spacePreview ? 1 : 0);
        WriteDword(RegKey::AltButtons, config_.altButtons ? 1 : 0);
        WriteDword(RegKey::AltDelay, config_.altDelay);
        WriteDword(RegKey::JpgQuality, config_.jpgQuality);
        WriteDword(RegKey::WebpQuality, config_.webpQuality);
        WriteDword(RegKey::Theme2, static_cast<uint32_t>(config_.themeMode));
        WriteDword(RegKey::NeonColor, config_.neonColor);
        WriteDword(RegKey::TrayIcon, config_.trayIcon ? 1 : 0);
        WriteDword(RegKey::AutoUpdate, config_.autoUpdate ? 1 : 0);
        WriteDword(RegKey::ColorPicker, config_.colorPicker ? 1 : 0);
        WriteDword(RegKey::ConvertDpi, config_.dpi300 ? 300 : 72);
        WriteDword(RegKey::PdfUncompressed, config_.pdfRaw ? 1 : 0);
        WriteDword(RegKey::VectorTextPaths, config_.vectorTextPaths ? 1 : 0);
        WriteDword(RegKey::TempPermanent, config_.tempPermanent ? 1 : 0);

        if (config_.uiFont.empty()) DeleteKey(RegKey::UiFont);
        else WriteString(RegKey::UiFont, config_.uiFont);

        WriteDword(RegKey::OcrHotkey, config_.ocrHotkey);
        WriteDword(RegKey::CleanHotkey, config_.cleanHotkey);
    }

    // Acesso à configuração (read-only)
    const Config& Get() const {
        std::lock_guard<std::mutex> lk(mu_);
        return config_;
    }

    // Modificadores com auto-save opcional
    template <typename F>
    void Modify(F&& fn, bool autoSave = true) {
        std::lock_guard<std::mutex> lk(mu_);
        fn(config_);
        config_.Validate();
        if (autoSave) Save();
    }

    // Helpers para campos específicos
    void SetTheme(Config::ThemeMode theme, bool autoSave = true) {
        Modify([theme](Config& c) { c.themeMode = theme; }, autoSave);
    }

    void SetNeonColor(uint32_t rgb, bool autoSave = true) {
        Modify([rgb](Config& c) { c.neonColor = rgb & 0xFFFFFF; }, autoSave);
    }

    void SetSpacePreview(bool on, bool autoSave = true) {
        Modify([on](Config& c) { c.spacePreview = on; }, autoSave);
    }

    void SetAltButtons(bool on, bool autoSave = true) {
        Modify([on](Config& c) { c.altButtons = on; }, autoSave);
    }

    void SetAltDelay(uint32_t ms, bool autoSave = true) {
        Modify([ms](Config& c) { c.altDelay = std::clamp(ms, 150u, 3000u); }, autoSave);
    }

    void SetJpgQuality(uint32_t q, bool autoSave = true) {
        Modify([q](Config& c) { c.jpgQuality = std::clamp(q, 50u, 100u); }, autoSave);
    }

    void SetWebpQuality(uint32_t q, bool autoSave = true) {
        Modify([q](Config& c) { c.webpQuality = std::clamp(q, 30u, 100u); }, autoSave);
    }

    void SetTrayIcon(bool on, bool autoSave = true) {
        Modify([on](Config& c) { c.trayIcon = on; }, autoSave);
    }

    void SetAutoUpdate(bool on, bool autoSave = true) {
        Modify([on](Config& c) { c.autoUpdate = on; }, autoSave);
    }

    void SetUiFont(std::wstring_view font, bool autoSave = true) {
        Modify([font](Config& c) { c.uiFont = font; }, autoSave);
    }

    void SetOcrHotkey(uint32_t hk, bool autoSave = true) {
        Modify([hk](Config& c) { c.ocrHotkey = hk; }, autoSave);
    }

    void SetCleanHotkey(uint32_t hk, bool autoSave = true) {
        Modify([hk](Config& c) { c.cleanHotkey = hk; }, autoSave);
    }

    // Verifica se é primeira execução (para presets)
    bool IsFirstRun() const {
        return !ReadDword(RegKey::SettingsPreset, 0xFFFFFFFF).has_value();
    }

    void MarkFirstRunDone() {
        WriteDword(RegKey::SettingsPreset, 100);  // Preset "Design"
    }

    // Edição
    enum class Edition : uint32_t { Lite = 0, Vizu = 1, Ultra = 2 };
    Edition GetEdition() const {
        return static_cast<Edition>(ReadDword(RegKey::Edition, 1).value_or(1));
    }
    void SetEdition(Edition ed) {
        WriteDword(RegKey::Edition, static_cast<uint32_t>(ed));
    }

    bool IsVectorConverterInstalled() const {
        return ReadDword(RegKey::VectorConverterInstalled, 0).value_or(0) != 0;
    }
    void SetVectorConverterInstalled(bool installed) {
        WriteDword(RegKey::VectorConverterInstalled, installed ? 1 : 0);
    }
};

// ============================================================================
// Convenience functions (compatibilidade)
// ============================================================================
inline void LoadConfig() { ConfigManager::Instance().Load(); }
inline void SaveConfig() { ConfigManager::Instance().Save(); }
inline const Config& GetConfig() { return ConfigManager::Instance().Get(); }

// Legacy struct para compatibilidade gradual
struct LegacyConfig {
    DWORD themeMode = 0, webpQuality = 75, neon = 0xFF6633;
    bool tray = true, space = true, alt = true, picker = true;
    bool autoUpdate = true, dpi300 = false, pdfRaw = false;
    bool vectorTextPaths = false, tempPermanent = false;
    DWORD altDelay = 500, quality = 75;
};

inline LegacyConfig ToLegacy(const Config& c) {
    LegacyConfig lc;
    lc.themeMode = static_cast<DWORD>(c.themeMode);
    lc.webpQuality = c.webpQuality;
    lc.neon = c.neonColor;
    lc.tray = c.trayIcon;
    lc.space = c.spacePreview;
    lc.alt = c.altButtons;
    lc.picker = c.colorPicker;
    lc.autoUpdate = c.autoUpdate;
    lc.dpi300 = c.dpi300;
    lc.pdfRaw = c.pdfRaw;
    lc.vectorTextPaths = c.vectorTextPaths;
    lc.tempPermanent = c.tempPermanent;
    lc.altDelay = c.altDelay;
    lc.quality = c.jpgQuality;
    return lc;
}

} // namespace cfg