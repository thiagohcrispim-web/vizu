// ExplorerIntegration.h - Integração com Windows Explorer (foco, seleção, IShellView)
#pragma once
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <vector>
#include <string>

namespace explorer {

// Retorna a SHELLDLL_DefView que tem o foco (lista de arquivos), ou nullptr
HWND FocusedFileList();

// Obtém IShellView para uma SHELLDLL_DefView
IShellView* ShellViewFor(HWND defView);

// Retorna caminhos dos arquivos selecionados na ordem de seleção
std::vector<std::wstring> SelectedFiles(HWND defView);

// Sincroniza ordem de seleção (itens novos vão para o fim)
void SyncSelectionOrder(HWND defView);

// Arquivo com foco + vizinhos (para pré-carregar)
std::wstring FocusedFile(HWND defView, std::vector<std::wstring>* neighbors = nullptr);

// Utilitário: caminho de um IShellItem
std::wstring ItemPath(IShellItem* item);

// Verifica se é arquivo (não diretório)
bool IsFile(const std::wstring& p);

// Classe da janela (para diagnóstico)
std::wstring ClassOf(HWND h);

// Log de diagnóstico (em %TEMP%\Vizu-teclado.log)
void KeyLog(const std::wstring& line);

} // namespace explorer