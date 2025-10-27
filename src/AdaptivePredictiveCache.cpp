#include "D:\\projects\\Enhanced-Redis\\include\\AdaptivePredictiveCache.h"
#include <algorithm> // For std::min, std::max

void AdaptivePredictiveCache::recordAccess(const std::string& key) {
    // If the key doesn't exist, create it with default stats.
    // If it exists, update its stats.
    KeyStats& stats = meta_store[key]; 
    stats.access_count++;
    stats.last_access = getCurrentTime();
    updateScore(key);
}

void AdaptivePredictiveCache::setTTL(const std::string& key, double ttl_seconds) {
    KeyStats& stats = meta_store[key];
    stats.ttl_initial_seconds = ttl_seconds;
    stats.ttl_set_time = getCurrentTime();
    stats.last_access = getCurrentTime(); // TTL setting is also an access
    updateScore(key);
}

double AdaptivePredictiveCache::getTTLRemaining(const std::string& key) const {
    auto it = meta_store.find(key);
    if (it == meta_store.end() || it->second.ttl_initial_seconds <= 0) {
        return 0.0; // No TTL set or key doesn't exist
    }

    const KeyStats& s = it->second;
    auto now = getCurrentTime();
    double elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - s.ttl_set_time).count();
    
    // The remaining TTL should not go below zero
    return std::max(0.0, s.ttl_initial_seconds - elapsed_seconds);
}

void AdaptivePredictiveCache::updateScore(const std::string& key) {
    auto it = meta_store.find(key);
    if (it == meta_store.end()) {
        // Key not in meta_store, cannot update score
        return;
    }

    KeyStats& s = it->second;
    auto now = getCurrentTime();

    // RecencyFactor = 1 / (1 + time_since_last_access)
    double time_since_last_access_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - s.last_access).count();
    double recency_factor = 1.0 / (1.0 + time_since_last_access_seconds);

    // FrequencyFactor = log(1 + access_count)
    double frequency_factor = std::log1p(s.access_count);

    // TTLFactor = ttl_remaining / ttl_total (if TTL exists)
    double ttl_factor = 0.0;
    if (s.ttl_initial_seconds > 0) {
        double current_ttl_remaining = getTTLRemaining(key);
        if (current_ttl_remaining <= 0) {
            // Key has expired, assign a very low score
            s.score = -std::numeric_limits<double>::max(); // Effectively mark for immediate eviction
            return;
        }
        ttl_factor = current_ttl_remaining / s.ttl_initial_seconds;
    }

    s.score = ALPHA * recency_factor + BETA * frequency_factor + GAMMA * ttl_factor;
}

std::string AdaptivePredictiveCache::evictCandidate() {
    std::string lowestKey;
    double lowestScore = std::numeric_limits<double>::max();
    bool foundCandidate = false;

    // Iterate through all keys to find the one with the lowest score
    // Also update scores for accuracy, especially TTL-based ones
    for (auto& pair : meta_store) {
        const std::string& key = pair.first;
        updateScore(key); // Ensure score is up-to-date

        // Check if the key has effectively expired
        if (getTTLRemaining(key) <= 0 && meta_store[key].ttl_initial_seconds > 0) {
            lowestKey = key;
            return lowestKey; // Prioritize already expired keys for eviction
        }
        
        if (meta_store[key].score < lowestScore) {
            lowestScore = meta_store[key].score;
            lowestKey = key;
            foundCandidate = true;
        }
    }
    
    if (!foundCandidate && !meta_store.empty()) {
        // Fallback: if no score was explicitly lower, just pick the first one
        // This case might happen if all scores are identical (e.g., all 0.0 for new keys)
        return meta_store.begin()->first;
    }

    return lowestKey;
}

void AdaptivePredictiveCache::removeKey(const std::string& key) {
    meta_store.erase(key);
}

bool AdaptivePredictiveCache::contains(const std::string& key) const {
    return meta_store.count(key) > 0;
}

double AdaptivePredictiveCache::getScore(const std::string& key) {
    updateScore(key); // Ensure score is up-to-date before returning
    auto it = meta_store.find(key);
    if (it != meta_store.end()) {
        return it->second.score;
    }
    return 0.0; // Or some other default/error value
}

void AdaptivePredictiveCache::clear() {
    meta_store.clear();
}