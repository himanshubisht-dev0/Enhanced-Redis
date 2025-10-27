# SmartCacheDB — Redis Re-imagined with Adaptive Predictive Caching

A high-performance, AI-inspired Redis-compatible in-memory data store written in C++. SmartCacheDB features an Adaptive Predictive Cache (APC) system that predicts which keys should be retained or evicted based on access patterns, without any explicit machine learning training. It supports strings, lists, and hashes, full Redis Serialization Protocol (RESP) parsing, multi-client concurrency via a thread pool, and periodic disk persistence.

## Description

**Name:** SmartCacheDB (Executable: `my_redis_server`)
**Default Port:** 6379

This project implements an intelligent Redis clone in C++, providing common Redis commands over a plain TCP socket using the RESP protocol. Its core innovation is the Adaptive Predictive Cache.

### Key Features

*   **C++17 Core Engine:** Fast, thread-safe in-memory key-value database.
*   **Thread Pool Architecture:** Efficiently handles multiple client connections concurrently, optimizing resource utilization by reusing worker threads.
*   **Adaptive Predictive Cache (APC):**
    *   **Intelligent Eviction:** Replaces traditional LRU, LFU, or simple TTL-based eviction.
    *   **Dynamic Scoring:** Computes a per-key "retention score" for each entry using a heuristic formula:
        `score = α * RecencyFactor + β * FrequencyFactor + γ * TTLFactor`
        *   `RecencyFactor = 1 / (1 + time_since_last_access)`
        *   `FrequencyFactor = log(1 + access_count)`
        *   `TTLFactor = ttl_remaining / ttl_total` (if TTL exists)
    *   **Adaptive Intelligence:** Mimics ML-like behavior, dynamically adapting to new workload patterns and key usage without explicit training data or models.
    *   **Real-Time Metadata Tracking:** Utilizes `KeyStats` to store `access_count`, `last_access` timestamp, `ttl_initial_seconds`, `ttl_set_time`, and the computed `score`.
    *   **Eviction Policy:** Selects the lowest-scoring key for removal when memory limits are reached.
*   **Scalable Design:** Modular components including `RedisDatabase`, `RedisServer`, `RedisCommandHandler`, `AdaptivePredictiveCache`, and `ThreadPool`.

### Supported Data Types & Commands

SmartCacheDB supports the following Redis-compatible commands:

*   **Common Commands:** `PING`, `ECHO`, `FLUSHALL`
*   **Key/Value:** `SET`, `GET`, `KEYS`, `TYPE`, `DEL`/`UNLINK`, `EXPIRE`, `RENAME`
*   **List:** `LGET`, `LLEN`, `LPUSH`/`RPUSH` (multi-element), `LPOP`/`RPOP`, `LREM`, `LINDEX`, `LSET`
*   **Hash:** `HSET`, `HGET`, `HEXISTS`, `HDEL`, `HKEYS`, `HVALS`, `HLEN`, `HGETALL`, `HMSET`

### Persistence

Data is automatically dumped to `dump.my_rdb` on graceful shutdown (e.g., Ctrl+C) and is loaded from this file at startup. This ensures data durability across server restarts.

## Repository Structure

```
├── include/                # Public headers
│   ├── RedisCommandHandler.h
│   ├── RedisDatabase.h
│   ├── RedisServer.h
│   ├── AdaptivePredictiveCache.h      # Predictive cache header
│   └── ThreadPool.h                   # Thread pool header
├── src/                    # Implementation files
│   ├── RedisCommandHandler.cpp
│   ├── RedisDatabase.cpp
│   ├── RedisServer.cpp
│   ├── AdaptivePredictiveCache.cpp    # APC implementation
│   ├── ThreadPool.cpp                 # Thread pool implementation
│   └── main.cpp            # Entry point
├── Concepts,UseCases&Tests.md    # Design concepts and command use cases
├── Makefile                # Build rules
├── README.md               # This documentation
└── test_all.sh             # Bash script for all tests
```

## Installation

This project uses a simple Makefile. Ensure you have a C++17 (or later) compliant compiler (like g++).

```bash
make
make clean
```

Or compile manually:

```bash
g++ -std=c++17 -pthread -Iinclude src/*.cpp -o my_redis_server
```

## Usage

After compiling the server, you can run it and interact with it using a Redis client.

### Running the Server

Start the server on the default port (6379) or specify a port:

```bash
./my_redis_server            # listens on 6379
./my_redis_server 6380       # listens on 6380
```

On startup, the server attempts to load `dump.my_rdb` if present:

```
Database loaded from dump.my_rdb
# or
No dump found or load failed; starting with an empty database.
```

To gracefully shutdown and persist immediately, press `Ctrl+C`.

### Using the Server

You can connect with the standard `redis-cli` or any RESP-compatible client.

```bash
# Using redis-cli:
redis-cli -p 6379

# Example session:
127.0.0.1:6379> PING
PONG

127.0.0.1:6379> SET foo "bar" EX 60
OK

127.0.0.1:6379> GET foo
"bar"

127.0.0.1:6379> HSET myhash field1 "value1"
(integer) 1

127.0.0.1:6379> HGET myhash field1
"value1"
```

## Design & Architecture

*   **Concurrency:** Client connections are handled by a `ThreadPool` (`std::thread::hardware_concurrency()` threads, or 4 by default), improving efficiency over per-client threads.
*   **Synchronization:** A single `std::mutex db_mutex` guards all in-memory data stores to ensure thread safety.
*   **Data Stores:**
    *   `kv_store` (`std::unordered_map<std::string, std::string>`) for strings.
    *   `list_store` (`std::unordered_map<std::string, std::vector<std::string>>`) for lists.
    *   `hash_store` (`std::unordered_map<std::string, std::unordered_map<std::string, std::string>>`) for hashes.
*   **Expiration & Eviction:** Managed by the `AdaptivePredictiveCache`. Keys are lazily evicted upon access if expired, and proactively evicted based on their calculated retention score when the cache size limit is reached.
*   **Persistence:** Simplified RDB-like text-based dump/load mechanism in `dump.my_rdb`.
*   **Singleton Pattern:** `RedisDatabase::getInstance()` enforces a single shared instance of the database.
*   **RESP Parsing:** Custom parser in `RedisCommandHandler` supports both inline and array formats.

This "AI-inspired adaptive cache policy" computes key retention scores based on temporal and frequency-based heuristics, aiming to significantly improve cache efficiency and memory utilization compared to traditional caching strategies.
