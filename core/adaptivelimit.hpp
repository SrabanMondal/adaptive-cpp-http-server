#include<chrono>
#include<cmath>
#include<iostream>
#include<string>
#include<unordered_map>
#include<vector>
#include<mutex>
#include<functional>
#include<cstddef>

struct AdaptiveState {
    double mu = 0.0;
    double var = 1.0;
    double alpha = 0.2;
    int baseLimit = 5;
    int minLimit = 10;
    double multiplier = 1.0;
    double k = 3.0;

    std::chrono::steady_clock::time_point windowStart = std::chrono::steady_clock::now();
    int windowCount = 0;
    int windowSeconds = 20;

    AdaptiveState() = default;

    bool allowRequest() {
        using namespace std::chrono;
        auto now = steady_clock::now();
        auto elapsed = duration_cast<seconds>(now - windowStart).count();
        if (elapsed >= windowSeconds) {
            update(windowCount);
            windowCount = 0;
            windowStart = now;
        }
        windowCount++;

        double stddev = std::sqrt(var);
        double threshold = baseLimit + (mu > 0.0 ? (multiplier * mu + k * stddev) : baseLimit);
        if (threshold < minLimit) threshold = minLimit;
        return (double)windowCount <= threshold;
    }

    void update(int x) {
        if (mu == 0.0 && var == 1.0) {
            mu = (double)x;
            var = 1.0;
            return;
        }
        double delta = (double)x - mu;
        mu += alpha * delta;
        var = (1.0 - alpha) * var + alpha * delta * delta;
        if (mu < 0.0) mu = 0.0;
        if (var < 1e-6) var = 1e-6;
    }
};


static constexpr size_t DEFAULT_NUM_SHARDS = 32;

class RateLimiterShard {
    struct Entry {
        AdaptiveState state;
        std::chrono::steady_clock::time_point lastSeen;
    };

    std::unordered_map<std::string, Entry> map_;
    mutable std::mutex mtx_;
    std::chrono::seconds ttl_{300};  // 5 min expiry

public:
    // Called under lock — fast path
    bool allowRequest(const std::string& ip) {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lg(mtx_);
        auto& e = map_[ip];
        e.lastSeen = now;
        return e.state.allowRequest();
    }

    void evictStale() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lg(mtx_);
        for (auto it = map_.begin(); it != map_.end(); ) {
            if (now - it->second.lastSeen > ttl_)
                it = map_.erase(it);
            else
                ++it;
        }
    }
};


// Sharded IP Rate Limiter
class IpRateLimiter {
    std::vector<RateLimiterShard> shards_;
    size_t numShards_;
    bool enabled_;                       // runtime toggle

    static size_t shardIndex(const std::string& ip, size_t n) {
        return std::hash<std::string>{}(ip) % n;
    }

public:
    explicit IpRateLimiter(size_t numShards = DEFAULT_NUM_SHARDS, bool enabled = true)
        : shards_(numShards), numShards_(numShards), enabled_(enabled) {}

    void setEnabled(bool v) noexcept { enabled_ = v; }
    bool isEnabled() const noexcept   { return enabled_; }

    bool allowRequest(const std::string& ip) {
        if (!enabled_) return true;      // fast path: rate limiting disabled
        return shards_[shardIndex(ip, numShards_)].allowRequest(ip);
    }

    void evictStale() {
        for (auto& s : shards_)
            s.evictStale();
    }
};