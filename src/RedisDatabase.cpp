#include "D:\\projects\\Enhanced-Redis\\include\\RedisDatabase.h"
#include <fstream> // file stream
#include <sstream>
#include <algorithm>
#include <iterator>
#include <chrono>
#include <unordered_set> // For consolidating keys in `keys()` command

// Singleton accessor
RedisDatabase& RedisDatabase::getInstance() {
    static RedisDatabase instance;
    return instance;
}

// Private helper to get total key count across all stores
size_t RedisDatabase::getTotalKeyCount() const {
    // Note: This counts the number of keys in each store, not unique keys across all stores.
    // For a more accurate unique key count, one would need to iterate and add to a set.
    // However, for eviction trigger purposes, this approximation is often sufficient.
    return kv_store.size() + list_store.size() + hash_store.size();
}

// Private helper for internal deletion without locking or expiration checks
bool RedisDatabase::delInternal(const std::string& key) {
    bool erased = false;
    erased |= kv_store.erase(key) > 0;
    erased |= list_store.erase(key) > 0;
    erased |= hash_store.erase(key) > 0;
    if (erased) {
        predictive_cache.removeKey(key);
    }
    return erased;
}

// Private helper to check if a key is expired based on APC data
bool RedisDatabase::isExpired(const std::string& key) {
    if (!predictive_cache.contains(key)) {
        return false; // Key is not managed by APC for expiration (no TTL set)
    }
    // Update score to ensure TTL remaining is current before checking
    predictive_cache.updateScore(key); // Recalculate score (which also updates internal TTL remaining)
    return predictive_cache.getTTLRemaining(key) <= 0 && predictive_cache.meta_store[key].ttl_initial_seconds > 0;
}

// New method for cache eviction
void RedisDatabase::checkAndEvict() {
    // If the total number of distinct keys exceeds the max cache size
    if (getTotalKeyCount() <= max_cache_size) {
        return; // No eviction needed yet
    }

    std::string keyToEvict = predictive_cache.evictCandidate();
    if (keyToEvict.empty()) {
        // This can happen if meta_store is empty, but getTotalKeyCount() > 0 (e.g., keys without any access/TTL)
        // Or if all keys have extremely high scores (less likely with current scoring)
        // Fallback: if APC can't decide, just remove an arbitrary key (e.g., from kv_store if not empty)
        if (!kv_store.empty()) {
            keyToEvict = kv_store.begin()->first;
        } else if (!list_store.empty()) {
            keyToEvict = list_store.begin()->first;
        } else if (!hash_store.empty()) {
            keyToEvict = hash_store.begin()->first;
        } else {
            return; // Really nothing to evict
        }
    }

    // Remove the chosen key from all data stores and the predictive cache
    delInternal(keyToEvict);
}

bool RedisDatabase::flushAll() {
    std::lock_guard<std::mutex> lock(db_mutex);
    kv_store.clear();
    list_store.clear();
    hash_store.clear();
    predictive_cache.clear(); // Clear all metadata from the predictive cache
    return true;
}

// Key/value operations
void RedisDatabase::set(const std::string& key, const std::string& value, double ttl_seconds) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // If the key exists but is expired, remove it first (Redis SET behavior)
    if (predictive_cache.contains(key) && isExpired(key)) {
        delInternal(key);
    }

    kv_store[key] = value;
    predictive_cache.recordAccess(key); // Record access for scoring

    if (ttl_seconds > 0) {
        predictive_cache.setTTL(key, ttl_seconds);
    } else {
        // If TTL is set to 0 or not provided, remove any existing TTL
        if (predictive_cache.contains(key)) {
            predictive_cache.setTTL(key, 0); // Effectively removes TTL and resets related factors
        }
    }
    checkAndEvict(); // Check for eviction after adding/updating a key
}

bool RedisDatabase::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key); // Remove expired key
        return false;
    }

    auto it = kv_store.find(key);
    if (it != kv_store.end()) {
        predictive_cache.recordAccess(key); // Record access for scoring
        value = it->second;
        return true;
    }
    return false;
}

std::vector<std::string> RedisDatabase::keys() {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<std::string> result;

    // Iterate through all possible keys and filter out expired ones
    std::unordered_set<std::string> unique_keys;
    for (const auto& pair : kv_store) unique_keys.insert(pair.first);
    for (const auto& pair : list_store) unique_keys.insert(pair.first);
    for (const auto& pair : hash_store) unique_keys.insert(pair.first);

    for (const std::string& key : unique_keys) {
        if (!isExpired(key)) {
            result.push_back(key);
            predictive_cache.recordAccess(key); // Accessing key via KEYS also counts as an access
        } else {
            delInternal(key); // Remove expired key found during KEYS command
        }
    }
    return result;
}

std::string RedisDatabase::type(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key); // Remove expired key
        return "none";
    }
    
    // An access to check type also updates recency/frequency
    if (predictive_cache.contains(key) || kv_store.count(key) || list_store.count(key) || hash_store.count(key)) {
        predictive_cache.recordAccess(key); 
    }


    if (kv_store.count(key)) {
        return "string";
    }
    if (list_store.count(key)) {
        return "list";
    }
    if (hash_store.count(key)) {
        return "hash";
    }
    return "none";
}

bool RedisDatabase::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    return delInternal(key); // Use internal helper for deletion
}

bool RedisDatabase::expire(const std::string& key, int seconds) {
    std::lock_guard<std::mutex> lock(db_mutex);
    // Check if the key exists in any store or is tracked by APC (could have been expired already but still in meta_store)
    if (!predictive_cache.contains(key) && !kv_store.count(key) && !list_store.count(key) && !hash_store.count(key)) {
        return false; // Key doesn't exist to set TTL on
    }

    if (seconds > 0) {
        predictive_cache.setTTL(key, static_cast<double>(seconds));
        predictive_cache.recordAccess(key); // Setting TTL also counts as an access
    } else { // EXPIRE key 0 means remove TTL or expire immediately
        predictive_cache.setTTL(key, 0); // Effectively marks for immediate eviction/removal in APC logic
        delInternal(key); // Immediately delete it from actual stores
    }
    return true;
}

bool RedisDatabase::rename(const std::string& oldKey, const std::string& newKey) {
    std::lock_guard<std::mutex> lock(db_mutex);

    // Check for expiration of oldKey
    if (isExpired(oldKey)) {
        delInternal(oldKey);
        return false; // Cannot rename an expired key
    }

    // If newKey already exists, it should be deleted first (Redis behavior)
    if (kv_store.count(newKey) || list_store.count(newKey) || hash_store.count(newKey)) {
        delInternal(newKey);
    }

    bool found = false;
    KeyStats oldStats; // To temporarily hold stats if oldKey has APC data

    if (predictive_cache.contains(oldKey)) {
        oldStats = predictive_cache.meta_store[oldKey];
        predictive_cache.removeKey(oldKey); // Remove old key's metadata from APC
    }

    // Handle string keys
    auto itKv = kv_store.find(oldKey);
    if (itKv != kv_store.end()) {
        kv_store[newKey] = itKv->second;
        kv_store.erase(itKv);
        found = true;
    }

    // Handle list keys
    auto itList = list_store.find(oldKey);
    if (itList != list_store.end()) {
        list_store[newKey] = itList->second;
        list_store.erase(itList);
        found = true;
    }

    // Handle hash keys
    auto itHash = hash_store.find(oldKey);
    if (itHash != hash_store.end()) {
        hash_store[newKey] = itHash->second;
        hash_store.erase(itHash);
        found = true;
    }

    if (found) {
        // If oldKey had APC stats, transfer them to newKey
        predictive_cache.recordAccess(newKey); // Ensures newKey is in meta_store
        if (oldStats.access_count > 0) { // Check if oldStats was actually populated
            predictive_cache.meta_store[newKey].access_count = oldStats.access_count;
            predictive_cache.meta_store[newKey].last_access = oldStats.last_access;
            predictive_cache.meta_store[newKey].ttl_initial_seconds = oldStats.ttl_initial_seconds;
            predictive_cache.meta_store[newKey].ttl_set_time = oldStats.ttl_set_time;
        }
        predictive_cache.updateScore(newKey); // Recalculate score for new key with copied stats
        predictive_cache.recordAccess(newKey); // Rename itself is an access to newKey
    }
    return found;
}

// List operations
std::vector<std::string> RedisDatabase::lget(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return {};
    }
    auto it = list_store.find(key);
    if (it != list_store.end()) {
        predictive_cache.recordAccess(key);
        return it->second;
    }
    return {};
}

ssize_t RedisDatabase::llen(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return 0;
    }
    auto it = list_store.find(key);
    if (it != list_store.end()) {
        predictive_cache.recordAccess(key);
        return it->second.size();
    }
    return 0;
}

void RedisDatabase::lpush(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    // If expired, remove first (LPUSH on an expired key creates a new key)
    if (isExpired(key)) {
        delInternal(key);
    }
    list_store[key].insert(list_store[key].begin(), value);
    predictive_cache.recordAccess(key);
    checkAndEvict();
}

void RedisDatabase::rpush(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    // If expired, remove first
    if (isExpired(key)) {
        delInternal(key);
    }
    list_store[key].push_back(value);
    predictive_cache.recordAccess(key);
    checkAndEvict();
}

bool RedisDatabase::rpop(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return false;
    }
    auto it = list_store.find(key);
    if (it != list_store.end() && !it->second.empty()) {
        predictive_cache.recordAccess(key);
        value = it->second.back();
        it->second.pop_back();
        if (it->second.empty()) { // If list becomes empty, delete its entry (like Redis)
            delInternal(key);
        }
        return true;
    }
    return false;
}

bool RedisDatabase::lpop(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return false;
    }
    auto it = list_store.find(key);
    if (it != list_store.end() && !it->second.empty()) {
        predictive_cache.recordAccess(key);
        value = it->second.front();
        it->second.erase(it->second.begin());
        if (it->second.empty()) { // If list becomes empty, delete its entry
            delInternal(key);
        }
        return true;
    }
    return false;
}

int RedisDatabase::lrem(const std::string& key, int count, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return 0;
    }
    int removed = 0;
    auto it = list_store.find(key);
    if (it == list_store.end()) {
        return 0;
    }
    auto& lst = it->second;

    if (count == 0) {
        auto new_end = std::remove(lst.begin(), lst.end(), value);
        removed = std::distance(new_end, lst.end());
        lst.erase(new_end, lst.end());
    } else if (count > 0) {
        for (auto iter = lst.begin(); iter != lst.end() && removed < count;) {
            if (*iter == value) {
                iter = lst.erase(iter);
                ++removed;
            } else {
                ++iter;
            }
        }
    } else { // count < 0, remove from tail to head
        for (auto riter = lst.rbegin(); riter != lst.rend() && removed < (-count);) {
            if (*riter == value) {
                auto fwditerator = riter.base();
                --fwditerator;
                fwditerator = lst.erase(fwditerator);
                ++removed;
                riter = std::reverse_iterator<std::vector<std::string>::iterator>(fwditerator);
            } else {
                ++riter;
            }
        }
    }

    if (removed > 0) {
        predictive_cache.recordAccess(key);
        if (lst.empty()) {
            delInternal(key); // If list becomes empty, delete its entry
        }
    }
    return removed;
}

bool RedisDatabase::lindex(const std::string& key, int index, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return false;
    }
    auto it = list_store.find(key);
    if (it == list_store.end()) {
        return false;
    }
    const auto& lst = it->second;
    if (index < 0) {
        index = lst.size() + index;
    }
    if (index < 0 || index >= static_cast<int>(lst.size())) {
        return false;
    }
    predictive_cache.recordAccess(key);
    value = lst[index];
    return true;
}

bool RedisDatabase::lset(const std::string& key, int index, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return false;
    }
    auto it = list_store.find(key);
    if (it == list_store.end()) {
        return false;
    }
    auto& lst = it->second;
    if (index < 0) {
        index = lst.size() + index;
    }
    if (index < 0 || index >= static_cast<int>(lst.size())) {
        return false;
    }
    lst[index] = value;
    predictive_cache.recordAccess(key);
    return true;
}

// Hash operations
bool RedisDatabase::hset(const std::string& key, const std::string& field, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    // If expired, remove first (HSET on an expired key creates a new key)
    if (isExpired(key)) {
        delInternal(key);
    }
    hash_store[key][field] = value;
    predictive_cache.recordAccess(key);
    checkAndEvict();
    return true;
}

bool RedisDatabase::hget(const std::string& key, const std::string& field, std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return false;
    }
    auto it = hash_store.find(key);
    if (it != hash_store.end()) {
        auto f = it->second.find(field);
        if (f != it->second.end()) {
            predictive_cache.recordAccess(key);
            value = f->second;
            return true;
        }
    }
    return false;
}

bool RedisDatabase::hexists(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return false;
    }
    auto it = hash_store.find(key);
    if (it != hash_store.end()) {
        predictive_cache.recordAccess(key);
        return it->second.count(field) > 0;
    }
    return false;
}

bool RedisDatabase::hdel(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return false;
    }
    auto it = hash_store.find(key);
    if (it != hash_store.end()) {
        predictive_cache.recordAccess(key);
        bool erased = it->second.erase(field) > 0;
        if (it->second.empty()) { // If hash becomes empty, delete its entry
            delInternal(key);
        }
        return erased;
    }
    return false;
}

std::unordered_map<std::string, std::string> RedisDatabase::hgetall(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return {};
    }
    auto it = hash_store.find(key);
    if (it != hash_store.end()) {
        predictive_cache.recordAccess(key);
        return it->second;
    }
    return {};
}

std::vector<std::string> RedisDatabase::hkeys(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<std::string> fields;
    if (isExpired(key)) {
        delInternal(key);
        return fields;
    }
    auto it = hash_store.find(key);
    if (it != hash_store.end()) {
        predictive_cache.recordAccess(key);
        for (const auto& pair : it->second) {
            fields.push_back(pair.first);
        }
    }
    return fields;
}

std::vector<std::string> RedisDatabase::hvals(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<std::string> values;
    if (isExpired(key)) {
        delInternal(key);
        return values;
    }
    auto it = hash_store.find(key);
    if (it != hash_store.end()) {
        predictive_cache.recordAccess(key);
        for (const auto& pair : it->second) {
            values.push_back(pair.second); // Corrected to push_back pair.second for values
        }
    }
    return values;
}

ssize_t RedisDatabase::hlen(const std::string& key) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (isExpired(key)) {
        delInternal(key);
        return 0;
    }
    auto it = hash_store.find(key);
    if (it != hash_store.end()) {
        predictive_cache.recordAccess(key);
        return it->second.size();
    }
    return 0;
}

bool RedisDatabase::hmset(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fieldValues) {
    std::lock_guard<std::mutex> lock(db_mutex);
    // If expired, remove first (HMSET on an expired key creates a new key)
    if (isExpired(key)) {
        delInternal(key);
    }
    for (const auto& pair : fieldValues) {
        hash_store[key][pair.first] = pair.second;
    }
    predictive_cache.recordAccess(key);
    checkAndEvict();
    return true;
}

// Persistent: Dump /load the database from a file.
bool RedisDatabase::dump(const std::string& filename) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::ofstream ofs(filename, std::ios::binary); // open file in binary mode
    if (!ofs) return false; // error opening file

    // Only dump non-expired keys
    for (const auto& kv : kv_store) {
        if (!isExpired(kv.first)) {
            ofs << "K " << kv.first << " " << kv.second << "\\n";
        }
    }
    for (const auto& kv : list_store) {
        if (!isExpired(kv.first)) {
            ofs << "L " << kv.first;
            for (const auto& item : kv.second) {
                ofs << " " << item;
            }
            ofs << "\\n";
        }
    }
    for (const auto& kv : hash_store) {
        if (!isExpired(kv.first)) {
            ofs << "H " << kv.first;
            for (const auto& field_val : kv.second) {
                ofs << " " << field_val.first << " " << field_val.second;
            }
            ofs << "\\n";
        }
    }
    // TODO: Consider dumping APC metadata for more robust persistence of scores/TTL
    // For now, TTL is handled during load implicitly by setting it again if present.
    return true;
}

bool RedisDatabase::load(const std::string& filename) {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) return false; // error opening file

    // Clear existing data before loading
    kv_store.clear();
    list_store.clear();
    hash_store.clear();
    predictive_cache.clear();

    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        char type_char;
        iss >> type_char; // read type

        std::string key_str;
        iss >> key_str; // read key

        if (type_char == 'K') {
            std::string value;
            iss >> value;
            set(key_str, value); // Use SET to automatically record access and handle potential TTL if we dumped it
        } else if (type_char == 'L') {
            std::string item;
            // Need to reconstruct the list from space-separated items
            std::vector<std::string> list_elements;
            while (iss >> item) {
                list_elements.push_back(item);
            }
            // Use rpush to add elements, ensuring APC records access for the list key
            for(const auto& elem : list_elements) {
                rpush(key_str, elem);
            }
        } else if (type_char == 'H') {
            std::unordered_map<std::string, std::string> hash_map_elements;
            std::string field, value;
            while (iss >> field >> value) { // Read field and value pairs
                 hash_map_elements[field] = value;
            }
            // Use hset to add elements, ensuring APC records access for the hash key
            for(const auto& pair : hash_map_elements) {
                hset(key_str, pair.first, pair.second);
            }
        }
        // No explicit TTL is dumped or loaded yet, so keys loaded this way won't have TTL unless explicitly set later.
        // This is a simplification; a full Redis RDB would include expiration times.
    }
    return true;
}