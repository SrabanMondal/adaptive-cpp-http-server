#include<chrono>
#include<cmath>
#include<iostream>

struct AdaptiveState {
    double mu = 0.0;
    double var = 1.0;
    double alpha = 0.2;
    int baseLimit = 5; //Base value with EMA
    int minLimit = 10; //Minimum limit 10 reqs per 20sec
    double multiplier = 1.0;
    double k = 3.0;

    std::chrono::steady_clock::time_point windowStart = std::chrono::steady_clock::now();
    int windowCount = 0;
    int windowSeconds = 20; //20sec check

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
            mu = x;
            var = 1.0;
            return;
        }
        double delta = x - mu;
        mu += alpha * delta;
        var = (1.0 - alpha) * var + alpha * delta * delta;
        if (mu < 0.0) mu = 0.0;
        if (var < 1e-6) var = 1e-6;
    }
};


class IpRateLimiter {
    struct Entry {
        AdaptiveState state;
        std::chrono::steady_clock::time_point lastSeen;
    };
    std::unordered_map<std::string, Entry> map;
    std::mutex mtx;
    std::chrono::seconds ttl {300}; //5 minutes expiry

public:
    bool allowRequest(const std::string &ip) {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lg(mtx);
        auto &e = map[ip];
        e.lastSeen = now;
        return e.state.allowRequest();
    }

    void evictStale() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lg(mtx);
        for (auto it = map.begin(); it != map.end(); ) {
            if (now - it->second.lastSeen > ttl) it = map.erase(it);
            else ++it;
        }
    }
};
