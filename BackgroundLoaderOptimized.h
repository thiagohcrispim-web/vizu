// BackgroundLoaderOptimized.h - Background loader modernizado com cache O(1)
// Substitui BackgroundLoader.cpp/h por versão com RAII, cache otimizado e C++17
#pragma once
#include <windows.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include "CacheOptimized.h"
#include "Resource.h"
#include "Convert.h"

namespace bg {

// ============================================================================
// Job de carregamento
// ============================================================================
struct LoadJob {
    std::wstring path;
    UINT cx = 0;          // 0 = auto
    int page = 0;
    bool infoOnly = false;
    UINT screenCx = 2048;
    bool urgent = false;
};

// ============================================================================
// Resultado do carregamento
// ============================================================================
struct LoadResult {
    std::wstring path;
    int page = 0;
    UINT cx = 0;
    res::ScopedBitmap bitmap;
    bool alpha = false;
    int pages = 1;
    fmt::Info info;
    bool hasInfo = false;
    bool success = false;
    std::chrono::steady_clock::time_point loadedAt;
};

// ============================================================================
// Callback type
// ============================================================================
using LoadCallback = std::function<void(const LoadResult&)>;

// ============================================================================
// BackgroundLoader otimizado
// ============================================================================
class BackgroundLoader {
    // Singleton
    BackgroundLoader() = default;
    ~BackgroundLoader() { Shutdown(); }
    BackgroundLoader(const BackgroundLoader&) = delete;
    BackgroundLoader& operator=(const BackgroundLoader&) = delete;

public:
    static BackgroundLoader& Instance() {
        static BackgroundLoader instance;
        return instance;
    }

    // Inicialização
    bool Initialize(HWND targetWnd, UINT screenSize = 2048, size_t thumbCacheSize = 6, size_t infoCacheSize = 32) {
        std::lock_guard<std::mutex> lk(initMu_);
        if (running_) return false;

        targetWnd_ = targetWnd;
        screenCx_ = screenSize;
        running_ = true;

        // Inicializa caches
        cache_ = std::make_unique<cache::UnifiedCache>(thumbCacheSize, infoCacheSize);

        // Inicia worker thread
        worker_ = std::thread(&BackgroundLoader::WorkerLoop, this);
        return true;
    }

    void Shutdown() {
        {
            std::lock_guard<std::mutex> lk(queueMu_);
            running_ = false;
        }
        queueCv_.notify_all();

        if (worker_.joinable()) worker_.join();

        std::lock_guard<std::mutex> lk(initMu_);
        cache_.reset();
        targetWnd_ = nullptr;
    }

    // Enfileira job de carregamento
    void Enqueue(const std::wstring& path, UINT cx, int page, bool infoOnly, bool urgent, LoadCallback callback = nullptr) {
        LoadJob job{path, cx, page, infoOnly, screenCx_, urgent};

        // Chave do cache
        cache::ThumbKey key{path, page, cx ? cx : screenCx_};

        // Tenta cache primeiro
        cache::ThumbValue cached;
        if (!infoOnly && cache_->Thumbs().Get(key, cached)) {
            cache_->RecordHit();
            LoadResult result;
            result.path = path;
            result.page = page;
            result.cx = key.cx;
            result.bitmap = std::move(cached.bitmap); // Note: ScopedBitmap é move-only
            result.alpha = cached.alpha;
            result.pages = cached.pages;
            result.success = true;
            result.loadedAt = cached.loadedAt;
            if (callback) callback(result);
            return;
        }

        cache_->RecordMiss();

        // Registra callback pendente se fornecido
        if (callback) {
            std::lock_guard<std::mutex> lk(pendingMu_);
            pending_[key].push_back(std::move(callback));
        }

        // Adiciona à fila
        {
            std::lock_guard<std::mutex> lk(queueMu_);
            if (urgent) jobQueue_.push_front(std::move(job));
            else jobQueue_.push_back(std::move(job));
        }
        queueCv_.notify_one();
    }

    // Prefetch para lista de arquivos
    void Prefetch(const std::vector<std::wstring>& files) {
        for (const auto& f : files) {
            // Verifica se já tem no cache
            cache::ThumbKey key{f, 0, screenCx_};
            cache::ThumbValue dummy;
            if (!cache_->Thumbs().Get(key, dummy)) {
                Enqueue(f, 0, 0, cache_->Info().Get({f}, dummy) == false, false);
            }
        }
    }

    // Obtém do cache diretamente (síncrono)
    bool GetCached(const std::wstring& path, int page, UINT cx, cache::ThumbValue& out) {
        cache::ThumbKey key{path, page, cx ? cx : screenCx_};
        return cache_->Thumbs().Get(key, out);
    }

    bool GetCachedInfo(const std::wstring& path, fmt::Info& out) {
        cache::InfoKey key{path};
        return cache_->Info().Get(key, out);
    }

    // Cache stats
    cache::UnifiedCache::Stats GetStats() const {
        return cache_ ? cache_->GetStats() : cache::UnifiedCache::Stats{};
    }

    void SetCacheSizes(size_t thumbMax, size_t infoMax) {
        if (cache_) {
            cache_->Thumbs().SetMaxSize(thumbMax);
            cache_->Info().SetMaxSize(infoMax);
        }
    }

    // Remove item específico do cache
    void RemoveFromCache(const std::wstring& path, int page, UINT cx) {
        if (cache_) {
            cache::ThumbKey key{path, page, cx};
            cache_->Thumbs().Remove(key);
        }
    }

    void ClearCache() {
        if (cache_) cache_->Thumbs().Clear();
    }

    bool IsRunning() const { return running_; }
    HWND TargetWnd() const { return targetWnd_; }

private:
    // Chave para mapa de callbacks pendentes
    using PendingKey = cache::ThumbKey;
    using PendingMap = std::unordered_map<PendingKey, std::vector<LoadCallback>, cache::ThumbKeyHash>;

    // Worker loop
    void WorkerLoop() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        while (running_) {
            LoadJob job;
            {
                std::unique_lock<std::mutex> lk(queueMu_);
                queueCv_.wait(lk, [this] { return !jobQueue_.empty() || !running_; });
                if (!running_ && jobQueue_.empty()) break;
                if (jobQueue_.empty()) continue;

                job = std::move(jobQueue_.front());
                jobQueue_.pop_front();
            }

            // Processa job
            LoadResult result;
            result.path = job.path;
            result.page = job.page;
            result.cx = job.cx ? job.cx : screenCx_;
            result.loadedAt = std::chrono::steady_clock::now();

            try {
                if (job.infoOnly) {
                    // Só informações
                    FileInfo(job.path, &result.info);
                    result.hasInfo = true;
                    result.success = true;

                    // Cache info
                    if (cache_) {
                        cache::InfoKey ikey{job.path};
                        cache_->Info().Put(ikey, result.info);
                    }
                } else {
                    // Carrega imagem
                    UINT cx = job.cx ? job.cx : screenCx_;
                    if (!job.cx && result.hasInfo && result.info.w && result.info.h && result.info.note.empty()) {
                        cx = min(cx, max(result.info.w, result.info.h));
                    }

                    HBITMAP bmp = nullptr;
                    bool alpha = false;
                    int pages = 0;

                    bool drawn = RenderFile(job.path, cx, job.page, &bmp, &alpha, &pages);

                    if (drawn && bmp) {
                        result.bitmap.Reset(bmp);
                        result.alpha = alpha;
                        result.pages = pages;
                        result.success = true;

                        // Cache thumbnail
                        if (cache_) {
                            cache::ThumbKey key{job.path, job.page, job.cx ? job.cx : screenCx_};
                            cache::ThumbValue tval(bmp, alpha, pages);
                            // Note: tval.bitmap takes ownership of bmp
                            cache_->Thumbs().Put(key, std::move(tval));
                        }
                    } else {
                        // Fallback: ícone do arquivo
                        bmp = IconImage(job.path);
                        if (bmp) {
                            result.bitmap.Reset(bmp);
                            result.alpha = true;
                            result.pages = 1;
                            result.success = true;
                        }
                    }
                }
            } catch (...) {
                result.success = false;
            }

            // Notifica callbacks pendentes
            if (cache_ && result.success && !job.infoOnly) {
                cache::ThumbKey key{job.path, job.page, job.cx ? job.cx : screenCx_};
                std::vector<LoadCallback> callbacks;
                {
                    std::lock_guard<std::mutex> lk(pendingMu_);
                    auto it = pending_.find(key);
                    if (it != pending_.end()) {
                        callbacks = std::move(it->second);
                        pending_.erase(it);
                    }
                }

                for (auto& cb : callbacks) {
                    if (cb) cb(result);
                }
            }

            // Posta mensagem para UI thread se tem janela alvo
            if (targetWnd_ && result.success && !job.infoOnly) {
                // Cria cópia do resultado para a UI thread
                auto* uiResult = new LoadResult(result);
                // Note: bitmap é move-only, então precisamos copiar o handle
                // A UI thread vai assumir ownership
                if (!PostMessage(targetWnd_, WM_APP + 1, 0, reinterpret_cast<LPARAM>(uiResult))) {
                    delete uiResult;
                }
            }
        }

        CoUninitialize();
    }

    // Estado
    mutable std::mutex initMu_;
    HWND targetWnd_ = nullptr;
    UINT screenCx_ = 2048;
    std::atomic<bool> running_{false};

    // Caches
    std::unique_ptr<cache::UnifiedCache> cache_;

    // Fila de jobs
    std::mutex queueMu_;
    std::condition_variable queueCv_;
    std::deque<LoadJob> jobQueue_;

    // Callbacks pendentes
    std::mutex pendingMu_;
    PendingMap pending_;

    // Worker thread
    std::thread worker_;
};

} // namespace bg