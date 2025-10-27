#ifndef ADAPTIVE_PREDICTIVE_CACHE_H
#define ADAPTIVE_PREDICTIVE_CACHE_H

#include <string>
#include <unordered_map>
#include <chrono>
#include <cmath> // For std::log1p
#include <limits> // For std::numeric_limits
#include <vector> // Potentially for handling multiple eviction candidates or iteration

struct KeyStats {
    int access_count = 0;
    std::chrono::steady_clock::time_point last_access = std::chrono::steady_clock::now();
    double ttl_initial_seconds = 0; // The initial TTL duration when set
    std::chrono::steady_clock::time_point ttl_set_time = std::chrono::steady_clock::now(); // Time when TTL was set/refreshed
    double score = 0.0;
};

class AdaptivePredictiveCache {
private:
    std::unordered_map<std::string, KeyStats> meta_store;
    constexpr static double ALPHA = 0.5;
    constexpr static double BETA  = 0.3;
    constexpr static double GAMMA = 0.2;

    // Helper to get current time point
    std::chrono::steady_clock::time_point getCurrentTime() const {
        return std::chrono::steady_clock::now();
    }

public:
    // Records an access for a given key, updates its stats and score.
    void recordAccess(const std::string& key);

    // Sets or updates the TTL for a key, updates its stats and score.
    void setTTL(const std::string& key, double ttl_seconds);

    // Explicitly updates the score for a key.
    void updateScore(const std::string& key);

    // Calculates the current remaining TTL for a key based on initial TTL and set time.
    double getTTLRemaining(const std::string& key) const;

    // Identifies and returns the key with the lowest score for eviction.
    std::string evictCandidate();

    // Removes a key's stats from the cache (e.g., after actual eviction or deletion).
    void removeKey(const std::string& key);

    // Checks if a key exists in the meta_store.
    bool contains(const std::string& key) const;

    // Gets the current score of a key.
    double getScore(const std::string& key);

    // Clears all key stats from the cache.
    void clear();
};

#endif // ADAPTIVE_PREDICTIVE_CACHE_H