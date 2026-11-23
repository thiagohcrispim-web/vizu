// CacheOptimized.h - Cache LRU otimizado para miniaturas e informações de arquivo
// Substitui o cache baseado em vector por implementação O(1) com list + unordered_map
#pragma once
#include <list>
#include <unordered_map>
#include <string>
#include <mutex>
#include <functional>
#include <memory>
#include <atomic>
#include "Resource.h"

namespace cache {

// ============================================================================
// Cache LRU genérico thread-safe O(1)
// ============================================================================
template <typename Key, typename Value>
class LRUCache {
    struct Node {
        Key key;
        Value value;
        std::chrono::steady_clock::time_point lastAccess;

        Node(const Key& k, const Value& v) : key(k), value(v), lastAccess(std::chrono::steady_clock::now()) {}
    };

    using List = std::list<Node>;
    using Iterator = typename List::iterator;
    using Map = std::unordered_map<Key, Iterator>;

    List list_;
    Map map_;
    size_t maxSize_;
    mutable std::mutex mu_;

public:
    explicit LRUCache(size_t maxSize = 6) : maxSize_(maxSize) {}

    // Non-copyable, movable
    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;
    LRUCache(LRUCache&&) = default;
    LRUCache& operator=(LRUCache&&) = default;

    // Obtém valor se existe, move para frente (MRU)
    bool Get(const Key& key, Value& out) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = map_.find(key);
        if (it == map_.end()) return false;

        // Move para frente (mais recente)
        list_.splice(list_.begin(), list_, it->second);
        it->second->lastAccess = std::chrono::steady_clock::now();
        out = it->second->value;
        return true;
    }

    // Obtém ponteiro para valor (para evitar cópia), move para frente
    Value* GetPtr(const Key& key) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = map_.find(key);
        if (it == map_.end()) return nullptr;

        list_.splice(list_.begin(), list_, it->second);
        it->second->lastAccess = std::chrono::steady_clock::now();
        return &it->second->value;
    }

    // Insere ou atualiza, move para frente
    void Put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = map_.find(key);
        if (it != map_.end()) {
            // Atualiza existente
            it->second->value = value;
            it->second->lastAccess = std::chrono::steady_clock::now();
            list_.splice(list_.begin(), list_, it->second);
            return;
        }

        // Novo item
        if (list_.size() >= maxSize_) {
            // Remove LRU (último)
            auto last = std::prev(list_.end());
            map_.erase(last->key);
            list_.pop_back();
        }

        list_.emplace_front(key, value);
        map_[key] = list_.begin();
    }

    // Remove item específico
    bool Remove(const Key& key) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        list_.erase(it->second);
        map_.erase(it);
        return true;
    }

    // Limpa tudo
    void Clear() {
        std::lock_guard<std::mutex> lk(mu_);
        list_.clear();
        map_.clear();
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lk(mu_);
        return list_.size();
    }

    size_t MaxSize() const { return maxSize_; }
    void SetMaxSize(size_t size) {
        std::lock_guard<std::mutex> lk(mu_);
        maxSize_ = size;
        while (list_.size() > maxSize_) {
            auto last = std::prev(list_.end());
            map_.erase(last->key);
            list_.pop_back();
        }
    }

    // Iteração (para debug/estatísticas)
    template <typename Func>
    void ForEach(Func&& func) const {
        std::lock_guard<std::mutex> lk(mu_);
        for (const auto& node : list_) {
            func(node.key, node.value, node.lastAccess);
        }
    }

    // Estatísticas
    struct Stats {
        size_t size = 0;
        size_t maxSize = 0;
        double loadFactor = 0.0;
    };

    Stats GetStats() const {
        std::lock_guard<std::mutex> lk(mu_);
        return {list_.size(), maxSize_, maxSize_ > 0 ? double(list_.size()) / maxSize_ : 0.0};
    }
};

// ============================================================================
// Chave composta para cache de miniaturas
// ============================================================================
struct ThumbKey {
    std::wstring path;
    int page = 0;
    UINT cx = 0;  // 0 = auto (tela)

    bool operator==(const ThumbKey& other) const {
        return path == other.path && page == other.page && cx == other.cx;
    }
};

// Hash para ThumbKey
struct ThumbKeyHash {
    size_t operator()(const ThumbKey& k) const noexcept {
        size_t h = std::hash<std::wstring>{}(k.path);
        h ^= std::hash<int>{}(k.page) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<UINT>{}(k.cx) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// ============================================================================
// Valor do cache de miniatura
// ============================================================================
struct ThumbValue {
    res::ScopedBitmap bitmap;
    bool alpha = false;
    int pages = 1;
    std::chrono::steady_clock::time_point loadedAt;

    ThumbValue() = default;
    ThumbValue(HBITMAP bmp, bool a, int p) : bitmap(bmp), alpha(a), pages(p), loadedAt(std::chrono::steady_clock::now()) {}

    // Move-only
    ThumbValue(const ThumbValue&) = delete;
    ThumbValue& operator=(const ThumbValue&) = delete;
    ThumbValue(ThumbValue&&) = default;
    ThumbValue& operator=(ThumbValue&&) = default;
};

// ============================================================================
// Cache especializado para miniaturas
// ============================================================================
class ThumbnailCache : public LRUCache<ThumbKey, ThumbValue, ThumbKeyHash> {
public:
    explicit ThumbnailCache(size_t maxSize = 6) : LRUCache(maxSize) {}

    // Chave simplificada para busca
    ThumbKey MakeKey(const std::wstring& path, int page, UINT cx) const {
        return {path, page, cx};
    }

    // Busca com fallback para cx=0 (auto)
    bool GetBestMatch(const std::wstring& path, int page, UINT screenCx, ThumbValue& out) {
        // Primeiro tenta com cx exato
        ThumbKey keyExact{path, page, screenCx};
        if (Get(keyExact, out)) return true;

        // Fallback: procura qualquer cx para este path/page
        std::lock_guard<std::mutex> lk(this->mu_);
        for (auto it = this->list_.begin(); it != this->list_.end(); ++it) {
            if (it->key.path == path && it->key.page == page) {
                // Move para frente
                this->list_.splice(this->list_.begin(), this->list_, it);
                out = it->value;
                return true;
            }
        }
        return false;
    }

    // Pré-carrega thumbnails para lista de arquivos
    void Prefetch(const std::vector<std::wstring>& files, UINT screenCx,
                  std::function<bool(const std::wstring&, int, UINT, ThumbValue&)> loader) {
        for (const auto& file : files) {
            ThumbKey key{file, 0, screenCx};
            ThumbValue dummy;
            if (!Get(key, dummy)) {
                // Não está no cache - o loader será chamado assincronamente
            }
        }
    }
};

// ============================================================================
// Cache para informações de arquivo (fmt::Info)
// ============================================================================
struct InfoKey {
    std::wstring path;

    bool operator==(const InfoKey& other) const { return path == other.path; }
};

struct InfoKeyHash {
    size_t operator()(const InfoKey& k) const noexcept {
        return std::hash<std::wstring>{}(k.path);
    }
};

using FileInfoCache = LRUCache<InfoKey, fmt::Info, InfoKeyHash>;

// ============================================================================
// Cache unificado (miniaturas + info)
// ============================================================================
class UnifiedCache {
    ThumbnailCache thumbs_;
    FileInfoCache info_;
    std::atomic<size_t> hits_{0};
    std::atomic<size_t> misses_{0};

public:
    UnifiedCache(size_t thumbMax = 6, size_t infoMax = 32)
        : thumbs_(thumbMax), info_(infoMax) {}

    ThumbnailCache& Thumbs() { return thumbs_; }
    FileInfoCache& Info() { return info_; }

    void RecordHit() { hits_++; }
    void RecordMiss() { misses_++; }

    struct Stats {
        size_t thumbSize, thumbMax, infoSize, infoMax;
        size_t hits, misses;
        double hitRate;
    };

    Stats GetStats() const {
        auto ts = thumbs_.GetStats();
        auto is = info_.GetStats();
        size_t h = hits_.load(), m = misses_.load();
        return {
            ts.size, ts.maxSize, is.size, is.maxSize,
            h, m, (h + m > 0) ? double(h) / (h + m) : 0.0
        };
    }

    void ResetStats() { hits_ = 0; misses_ = 0; }
};

// ============================================================================
// Cache assíncrono com callback (para BackgroundLoader)
// ============================================================================
template <typename Key, typename Value>
class AsyncCache {
    LRUCache<Key, Value> cache_;
    std::mutex pendingMu_;
    std::unordered_map<Key, std::vector<std::function<void(const Value&)>>> pending_;

public:
    explicit AsyncCache(size_t maxSize = 6) : cache_(maxSize) {}

    // Tenta obter do cache; se não tem, registra callback para quando chegar
    bool GetOrWait(const Key& key, std::function<void(const Value&)> callback, Value* outImmediate = nullptr) {
        Value val;
        if (cache_.Get(key, val)) {
            if (outImmediate) *outImmediate = val;
            if (callback) callback(val);
            return true;
        }

        // Não está no cache - registra callback
        if (callback) {
            std::lock_guard<std::mutex> lk(pendingMu_);
            pending_[key].push_back(std::move(callback));
        }
        return false;
    }

    // Coloca valor no cache e notifica callbacks pendentes
    void Put(const Key& key, const Value& value) {
        cache_.Put(key, value);

        std::vector<std::function<void(const Value&)>> callbacks;
        {
            std::lock_guard<std::mutex> lk(pendingMu_);
            auto it = pending_.find(key);
            if (it != pending_.end()) {
                callbacks = std::move(it->second);
                pending_.erase(it);
            }
        }

        for (auto& cb : callbacks) {
            if (cb) cb(value);
        }
    }

    LRUCache<Key, Value>& Cache() { return cache_; }
};

} // namespace cache