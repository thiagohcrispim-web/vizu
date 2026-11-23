// BackgroundLoader.h - Carregamento assíncrono + cache LRU
#pragma once
#include <windows.h>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <set>
#include <vector>
#include <string>
#include <memory>
#include "Convert.h"

namespace bg {

struct Job {
    std::wstring path;
    UINT cx = 0;
    int page = 0;
    bool info = false;
    UINT screen = 0;
};

struct Loaded {
    std::wstring path;
    UINT cx = 0;
    int page = 0;
    HBITMAP bmp = nullptr;
    bool alpha = false;
    int pages = 1;
    bool hasInfo = false;
    fmt::Info info;
};

struct CacheEntry {
    std::wstring path;
    int page = 0;
    UINT cx = 0;
    HBITMAP bmp = nullptr;
    bool alpha = false;
    int pages = 1;
    DWORD used = 0;
};

class BackgroundLoader {
public:
    static BackgroundLoader& Instance();

    void Start(HWND targetWnd, UINT screenSize);
    void Stop();

    // Enfileira um job (urgent = push_front)
    void Enqueue(const std::wstring& path, UINT cx, int page, bool info, bool urgent);

    // Verifica se há entrada no cache
    CacheEntry* Cached(const std::wstring& path, int page);

    // Cache de informações
    const fmt::Info* CachedInfo(const std::wstring& path);

    // Coloca resultado no cache
    void CachePut(Loaded* L);

    // Processa item carregado (chamado da WM_LOADED)
    void OnLoaded(Loaded* L);

    // Remove pending key
    void RemovePending(const std::wstring& path, int page, UINT cx);

    // Prefetch de múltiplos arquivos
    void Prefetch(const std::vector<std::wstring>& files);

    // Define tamanho máximo do cache
    void SetMaxCacheSize(size_t size) { maxCacheSize_ = size; }

private:
    BackgroundLoader() = default;
    ~BackgroundLoader() = default;
    BackgroundLoader(const BackgroundLoader&) = delete;
    BackgroundLoader& operator=(const BackgroundLoader&) = delete;

    static std::wstring JobKey(const std::wstring& path, int page, UINT cx);

    void WorkerLoop();

    HWND targetWnd_ = nullptr;
    UINT screen_ = 2048;
    size_t maxCacheSize_ = 6;

    std::mutex qm_;
    std::condition_variable qcv_;
    std::deque<Job> queue_;
    std::set<std::wstring> pending_;

    std::vector<CacheEntry> cache_;
    std::vector<std::pair<std::wstring, fmt::Info>> infoCache_;

    std::thread worker_;
    std::atomic<bool> running_{false};
};

} // namespace bg