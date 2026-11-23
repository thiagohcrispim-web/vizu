// ThemeManager.cpp - Implementação do gerenciamento de temas
#include "ThemeManager.h"
#include "Config.h"
#include "Ui.h"

namespace theme {

ThemeManager& ThemeManager::Instance() {
    static ThemeManager instance;
    return instance;
}

void ThemeManager::SetTheme(ThemeId id) {
    currentTheme_ = id;
    BuildTheme(id);
}

void ThemeManager::ApplyFromConfig(DWORD themeMode) {
    ThemeId id = static_cast<ThemeId>(std::min<DWORD>(themeMode, 4));
    SetTheme(id);
}

void ThemeManager::SetNeonColor(DWORD neon) {
    ui::NeonColor() = neon;
    if (currentTheme_ == kThemeCrispy) {
        BuildTheme(kThemeCrispy);
    }
}

void ThemeManager::Rebuild() {
    BuildTheme(currentTheme_);
}

void ThemeManager::BuildTheme(ThemeId id) {
    bool useAcrylic = true;
    theme_ = ui::MakeTheme(id, useAcrylic);
    currentTheme_ = id;

    // Atualiza a cor neon global se for tema Crispy
    if (id == kThemeCrispy) {
        ui::NeonColor() = cfg::g_cfg.neon;
    }
}

} // namespace theme