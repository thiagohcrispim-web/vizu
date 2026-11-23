// BackgroundLoader.cpp - Implementação do carregamento assíncrono + cache LRU
#include "BackgroundLoader.h"
#include <thread>

using conv::SafeRelease;
using conv::RenderFile;
using conv::IconImage;
using conv::CachedThumb;
using conv::PrepareAlpha;
using conv::ExportSize;
using conv::FileInfo;

namespace bg {

BackgroundLoader& BackgroundLoader::Instance() {
    static BackgroundLoader instance;
    return instance;
}

void BackgroundLoader::Start(HWND targetWnd, UINT screenSize) {
    targetWnd_ = targetWnd;
    screen_ = screenSize;
    running_ = true;
    worker_ = std::thread(&BackgroundLoader::WorkerLoop, this);
}

void BackgroundLoader::Stop() {
    running_ = false;
    qcv_.notify_one();
    if (worker_.joinable()) worker_.join();

    // Limpa cache
    for (auto& e : cache_) {
        if (e.bmp) DeleteObject(e.bmp);
    }
    cache_.clear();
    infoCache_.clear();
    pending_.clear();
}

std::wstring BackgroundLoader::JobKey(const std::wstring& path, int page, UINT cx) {
    return path + L"|" + std::to_wstring(page) + L"|" + std::to_wstring(cx);
}

void BackgroundLoader::Enqueue(const std::wstring& path, UINT cx, int page, bool info, bool urgent) {
    std::wstring key = JobKey(path, page, cx);
    if (!pending_.insert(key).second) return;

    std::lock_guard<std::mutex> lk(qm_);
    Job j{path, cx, page, info, screen_};
    if (urgent) queue_.push_front(j);
    else queue_.push_back(j);
    qcv_.notify_one();
}

CacheEntry* BackgroundLoader::Cached(const std::wstring& path, int page) {
    CacheEntry* best = nullptr;
    for (auto& e : cache_) {
        if (e.path == path && e.page == page && (!best || e.cx > best->cx))
            best = &e;
    }
    if (best) best->used = GetTickCount();
    return best;
}

const fmt::Info* BackgroundLoader::CachedInfo(const std::wstring& path) {
    for (auto& e : infoCache_)
        if (e.first == path) return &e.second;
    return nullptr;
}

void BackgroundLoader::CachePut(Loaded* L) {
    cache_.push_back({L->path, L->page, L->cx, L->bmp, L->alpha, L->pages, GetTickCount()});

    // LRU: remove o menos usado (nunca o exibido)
    while (cache_.size() > maxCacheSize_) {
        auto victim = cache_.end();
        for (auto it = cache_.begin(); it != cache_.end(); ++it)
            if (it->bmp != cache_.empty() ? nullptr : cache_[0].bmp &&
                (victim == cache_.end() || it->used < victim->used))
                victim = it;
        if (victim == cache_.end()) break;
        DeleteObject(victim->bmp);
        cache_.erase(victim);
    }

    if (L->hasInfo) {
        for (auto it = infoCache_.begin(); it != infoCache_.end(); ++it)
            if (it->first == L->path) { infoCache_.erase(it); break; }
        infoCache_.push_back({L->path, L->info});
        if (infoCache_.size() > 32) infoCache_.erase(infoCache_.begin());
    }
}

void BackgroundLoader::OnLoaded(Loaded* L) {
    RemovePending(L->path, L->page, L->cx);

    if (L->cx == 1) { // só informações
        if (L->bmp) DeleteObject(L->bmp);
        L->bmp = nullptr;
        // O tratamento do callback (atualizar UI) é feito no VizuMain
        delete L;
        return;
    }

    if (!L->bmp) { delete L; return; }

    CachePut(L);
    // O tratamento do callback (atualizar UI) é feito no VizuMain
    delete L;
}

void BackgroundLoader::RemovePending(const std::wstring& path, int page, UINT cx) {
    pending_.erase(JobKey(path, page, cx));
}

void BackgroundLoader::Prefetch(const std::vector<std::wstring>& files) {
    for (auto& f : files) {
        if (!Cached(f, 0))
            Enqueue(f, 0, 0, CachedInfo(f) == nullptr, false);
    }
}

void BackgroundLoader::WorkerLoop() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    for (;;) {
        Job j;
        {
            std::unique_lock<std::mutex> lk(qm_);
            qcv_.wait(lk, [this] { return !queue_.empty() || !running_; });
            if (!running_ && queue_.empty()) break;
            if (queue_.empty()) continue;

            j = queue_.front();
            queue_.pop_front();
        }

        auto* L = new Loaded;
        L->path = j.path;
        L->page = j.page;

        if (j.info) {
            FileInfo(j.path, &L->info);
            L->hasInfo = true;
        }

        UINT cx = j.cx;
        if (!cx) { // automático: tamanho da tela, sem passar do nativo
            fmt::Info in = L->info;
            cx = j.screen;
            if (L->hasInfo && in.w && in.h && in.note.empty())
                cx = min(cx, max(in.w, in.h));
        }
        L->cx = j.cx ? j.cx : 0;

        bool drawn = false;
        try {
            drawn = RenderFile(j.path, cx, j.page, &L->bmp, &L->alpha, &L->pages);
        } catch (...) {
            drawn = false;
        }

        if (!drawn) {
            L->bmp = IconImage(j.path);
            L->alpha = true;
        }

        if (!PostMessage(targetWnd_, WM_APP + 1, 0, (LPARAM)L)) {
            if (L->bmp) DeleteObject(L->bmp);
            delete L;
        }
    }

    CoUninitialize();
}

} // namespace bg