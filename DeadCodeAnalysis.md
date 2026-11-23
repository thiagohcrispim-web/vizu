# Dead Code Analysis - Vizu Refactoring (v9.14.4+)

## Resumo

Esta análise identifica código morto, duplicado ou obsoleto no `Vizu.cpp` original (~4500 linhas) que pode ser removido após a modularização.

---

## 1. Funções Completamente Movidas para Módulos

### Config (linhas 48-85)
- ✅ `ReadDword`, `WriteDword`
- ✅ `LoadConfig`, `SaveConfig`
- ✅ Struct `Config` → `cfg::Config`
- **Status**: Totalmente extraído para `Config.h/cpp` e `ConfigModern.h`

### Explorer Integration (linhas 159-343)
- ✅ `FocusedFileList`, `ViewFromBrowser`, `GetSelection`
- ✅ `FocusEvent`, `RegisterGlobalHotkeys`, `UpdateHotkeys`
- ✅ `UpdateTrayIcon`, `SetAutostart`, `SetStartMenu`, `SetNewMenu`
- ✅ `CreateFolderSet`, `FindRunning`, `AutostartEnabled`
- **Status**: Totalmente extraído para `ExplorerIntegration.h/cpp`

### Background Loader (linhas 345-424)
- ✅ `BgItem`, `BgQueue`, `WorkerLoop`
- ✅ `Enqueue`, `Cached`, `CachedInfo`, `CachePut`, `OnLoaded`, `RemovePending`, `Prefetch`
- **Status**: Totalmente extraído para `BackgroundLoader.h/cpp` e `BackgroundLoaderOptimized.h`

### Theme System (Ui.h + Vizu.cpp)
- ✅ `MakeTheme`, `ui::Theme`, `ui::Painter`
- ✅ `EnableGlass`, `EnableAcrylic`
- **Status**: Extraído para `ThemeManager.h/cpp` + `Ui.h`

### Alt Menu (Vizu.cpp linhas ~2000-2500)
- ✅ `BuildSides`, `RenderRing`, `UpdateRing`, `RingSegmentAt`
- ✅ `AltProc`, `AltTimer` handling
- **Status**: Extraído para `AltMenu.h/cpp` e `AltMenuModern.h`

### Dialogs (Vizu.cpp + .inc.h files)
- ✅ Toast, Crop, Settings, OCR Card/Selection, Clock, Agenda, Theft, Reminder, Clean, Widgets
- **Status**: Extraído para `Dialogs.h/cpp`

---

## 2. Código Morto Identificado no Vizu.cpp Original

### Variáveis Globais Não Utilizadas
```cpp
// Linhas ~87-150: Muitas variáveis movidas para módulos
static HBITMAP g_dib;           // → BackgroundLoader::CacheEntry
static ID2D1Bitmap* g_gpuBmp;   // → BackgroundLoader::CacheEntry
static ID2D1BitmapBrush* g_checker; // → Ui.cpp
static ui::Theme g_theme;       // → ThemeManager::Instance().GetTheme()
```

### Funções de Diagnóstico (Mantidas - úteis)
```cpp
// Linhas ~3900-4100: Diagnose(), DiagnoseKeys()
// STATUS: MANTER - Úteis para suporte/debug
```

### Funções de Instalação (Mantidas - necessárias)
```cpp
// Linhas ~4084-4305: Install(), Uninstall(), InstallFiles(), ReplaceFile()
// STATUS: MANTER - Parte do instalador single-file
```

### Crash Logging (Mantido)
```cpp
// Linhas ~4307-4342: WriteCrashLog(), CrashFilter()
// STATUS: MANTER - Importante para estabilidade
```

### wWinMain (Reescrito em VizuMain.cpp)
```cpp
// Linhas ~4344-4500: wWinMain original
// STATUS: SUBSTITUÍDO por VizuMain.cpp modernizado
```

---

## 3. Código Duplicado/Redundante

### Theme Application
```cpp
// Vizu.cpp: ApplyTheme() linha ~1800
// Ui.h: EnableGlass/EnableAcrylic
// ThemeManager: BuildTheme()
// CONFLITO: Múltiplas implementações de tema
// RESOLUÇÃO: ThemeManager é a fonte única da verdade
```

### Hotkey Registration
```cpp
// Vizu.cpp: RegisterGlobalHotkeys() + UpdateHotkeys()
// ExplorerIntegration: UpdateHotkeys()
// CONFLITO: Duplicação
// RESOLUÇÃO: Centralizado em ExplorerIntegration
```

### Cache Logic
```cpp
// Vizu.cpp: g_thumbCache (vector<ThumbCacheEntry>)
// BackgroundLoader: cache_ (deque<CacheEntry>)
// CONFLITO: Dois caches LRU separados
// RESOLUÇÃO: UnifiedCache em BackgroundLoaderOptimized
```

---

## 4. Includes Desnecessários no Vizu.cpp Original

```cpp
// Removidos após modularização:
#include "Convert.h"      // → Módulos que precisam incluem diretamente
#include "Jobs.h"         // → Módulos que precisam incluem diretamente
#include "VectorBundle.h" // → Apenas Jobs/VectorConvert
#include "Update.h"       // → Apenas VizuMain/Dialogs
#include "InstallerUi.h"  // → Apenas VizuMain
#include "ColorInfo.h"    // → Apenas Dialogs/Jobs
// Mantidos em VizuMain.cpp apenas o necessário
```

---

## 5. Arquivos .inc.h Obsoletos

Os seguintes arquivos eram `#include`d no Vizu.cpp e agora estão em Dialogs.cpp:
- `Toast.inc.h` → `ToastProc`, `ShowToast`, `HideToast`
- `Crop.inc.h` → `CropProc`, `OpenCropEditor`, `PaintCrop`
- `Settings.inc.h` → `SettingsProc`, `OpenSettings`, `PaintSettings`
- `Card.inc.h` → `CardProc`, `ShowCard`, `CardTranslate`
- `Sel.inc.h` → `SelProc`, `StartOcr`, `SelFinish`
- `Clock.inc.h` → `ClockProc`, `ClockRender`, `ClockEnable`
- `Agenda.inc.h` → `AgendaProc`, `PaintAgenda`, `OpenAgendaWindow`
- `Theft.inc.h` → `TheftProc`, `PaintTheft`, `ShowTheftHistory`
- `Reminder.inc.h` → `ReminderProc`, `ShowReminder`, `CheckReminders`
- `Clean.inc.h` → `CleanProc`, `OpenCleanWindow`, `PaintClean`
- `Widgets.inc.h` → `WidgetProc`, `WidgetsInit`, `WidgetsRenderAll`

**Ação**: Estes arquivos `.inc.h` podem ser **DELETADOS** após verificação.

---

## 6. Código Comentado/Dead Branches

### Vizu.cpp - Busca por:
```cpp
// #if 0 ... #endif
// #ifdef DEAD_CODE ... #endif
// Comentários "OLD:", "DEPRECATED:", "REMOVED:"
```

### Encontrados:
- Linhas ~500: `// v9.0: antigo sistema de cache` - código comentado
- Linhas ~1200: `// v9.5: tema antigo` - código comentado
- Linhas ~2800: `// v9.8: menu Alt antigo` - código comentado

**Ação**: Remover todos os blocos comentados.

---

## 7. Constantes Mágicas para Remover/Substituir

```cpp
// Vizu.cpp - Substituídas por enums/constexpr nos módulos:
kCaption = 56           → ui::kCaption (Ui.h)
kPad = 18               → ui::kPad (Ui.h)
kHotSpace = 1           → HotkeyId::Space (ExplorerIntegration.h)
kRefreshTimer = 1       → TimerId::Refresh (VizuMain.cpp)
kFlashTimer = 2         → TimerId::Flash (VizuMain.cpp)
kAltTimer = 3           → TimerId::Alt (AltMenu.h)
kToastTimer = 4         → TimerId::Toast (Dialogs.h)
kSelTimer = 5           → TimerId::Selection (Dialogs.h)
kAnimTimer = 6          → TimerId::Animation (Ui.h)
kUpdateTimer = 7        → TimerId::UpdateCheck (VizuMain.cpp)
kFocusPoll = 8          → TimerId::FocusPoll (ExplorerIntegration.h)
```

---

## 8. Métricas de Redução Estimada

| Componente | Linhas Originais | Linhas Novas | Redução |
|------------|-----------------|--------------|---------|
| Vizu.cpp (monolítico) | ~4500 | 0 (removido) | 100% |
| Config | 38 | 120 (moderno) | -216% (mais features) |
| ExplorerIntegration | 185 | 250 | -35% |
| BackgroundLoader | 80 | 300 (otimizado) | -275% |
| ThemeManager | 50 | 80 | -60% |
| AltMenu | 300 | 280 (moderno) | 7% |
| Dialogs | 2000+ | 1500 (organizado) | 25% |
| **Total módulos** | **~2653** | **~2530** | **~5%** |
| VizuMain.cpp | 0 | 400 | Novo |
| Resource.h | 0 | 350 | Novo |
| CacheOptimized.h | 0 | 280 | Novo |
| ModernCpp.h | 0 | 380 | Novo |
| ConfigModern.h | 0 | 220 | Novo |
| BackgroundLoaderOptimized.h | 0 | 320 | Novo |
| AltMenuModern.h | 0 | 280 | Novo |
| CMakeLists.txt | 0 | 180 | Novo |
| vcpkg.json | 0 | 50 | Novo |
| GitHub Workflows | 0 | 400 | Novo |

**Código removido (morto)**: ~1500 linhas (comentários, duplicatas, .inc.h, dead branches)
**Código novo (moderno)**: ~2560 linhas (melhor arquitetura, type-safety, RAII)

---

## 9. Checklist de Limpeza Final

### Arquivos para DELETAR
- [ ] `Vizu.cpp` (original monolítico)
- [ ] `Toast.inc.h`
- [ ] `Crop.inc.h`
- [ ] `Settings.inc.h`
- [ ] `Card.inc.h`
- [ ] `Sel.inc.h`
- [ ] `Clock.inc.h`
- [ ] `Agenda.inc.h`
- [ ] `Theft.inc.h`
- [ ] `Reminder.inc.h`
- [ ] `Clean.inc.h`
- [ ] `Widgets.inc.h`
- [ ] `build.sh` (substituído por CMake)
- [ ] `.github/workflows/*.yml` antigos (substituídos por build.yml + release.yml)

### Arquivos para MANTER (compatibilidade temporária)
- [ ] `Config.h/cpp` → migrar para `ConfigModern.h` gradualmente
- [ ] `BackgroundLoader.h/cpp` → migrar para `BackgroundLoaderOptimized.h`
- [ ] `AltMenu.h/cpp` → migrar para `AltMenuModern.h`
- [ ] `Ui.h` → manter (tipos base: Theme, Painter, Surface)

### Verificações
- [ ] Build compila no Windows (MSVC + MinGW)
- [ ] Build compila no Linux (cross-compile MinGW)
- [ ] Testes unitários passam
- [ ] Testes de integração (vector, preview) passam
- [ ] Instalador funciona
- [ ] Auto-update funciona
- [ ] Thumbnail provider registra corretamente
- [ ] Preview handler registra corretamente
- [ ] Menu Alt funciona
- [ ] Todos os diálogos abrem/funcionam
- [ ] Temas alternam corretamente
- [ ] Cache LRU funciona (hit rate > 80%)
- [ ] Memória estável (sem leaks em 1h uso)
- [ ] Performance: preview abre < 200ms

---

## 10. Próximos Passos

1. **Substituir includes** no VizuMain.cpp para usar módulos modernos
2. **Deletar Vizu.cpp** e arquivos .inc.h
3. **Atualizar CMakeLists.txt** para usar fontes modernas
4. **Rodar build completo** e corrigir erros
5. **Executar suite de testes**
6. **Benchmark de memória/performance**
7. **Tag v9.15.0** com refatoração completa