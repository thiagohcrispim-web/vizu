// ThemeManager.h - Gerenciamento dos 5 temas do Vizu
#pragma once
#include "Ui.h"
#include <string>

namespace theme {

enum ThemeId : DWORD {
    kThemeWindows = 0,  // Segue o sistema
    kThemeLight   = 1,  // Claro
    kThemeDark    = 2,  // Escuro
    kThemeCrispy  = 3,  // Neon (Crispy)
    kThemeClassic = 4   // Flat (Classic)
};

class ThemeManager {
public:
    static ThemeManager& Instance();

    void SetTheme(ThemeId id);
    ThemeId CurrentTheme() const { return currentTheme_; }
    const ui::Theme& GetTheme() const { return theme_; }
    ui::Theme& GetThemeMutable() { return theme_; }
    bool IsDark() const { return theme_.dark; }
    bool IsClassic() const { return theme_.classic; }

    // Aplica tema a partir do modo de configuração (0-4)
    void ApplyFromConfig(DWORD themeMode);

    // Atualiza cor neon
    void SetNeonColor(DWORD neon);

    // Força reconstrução do tema (após mudança de config)
    void Rebuild();

private:
    ThemeManager() = default;
    ~ThemeManager() = default;
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    void BuildTheme(ThemeId id);

    ThemeId currentTheme_ = kThemeDark;
    ui::Theme theme_ = ui::MakeTheme(kThemeDark, false);
};

} // namespace theme