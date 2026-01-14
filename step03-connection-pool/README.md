# Step 03: Connection Pool

Building on Step 02's database wrapper, this step adds thread-safe connection pooling.

## What's New in This Step

- Thread-safe connection pool
- Scoped connection acquisition (RAII)
- Pool statistics and monitoring
- Automatic connection health checks
- Configurable pool sizing

## Why Connection Pooling?

Without pooling:
- Each request opens a new connection (expensive)
- No limit on concurrent connections
- No connection reuse

With pooling:
- Connections are pre-created and reused
- Bounded resource usage
- Faster request handling

## Key Concepts

### 1. Pool Architecture

```
┌─────────────────────────────────────────────────┐
│              ConnectionPool                     │
│  ┌─────────────────────────────────────────┐    │
│  │         Available Connections           │    │
│  │  ┌────┐ ┌────┐ ┌────┐ ┌────┐            │    │
│  │  │ C1 │ │ C2 │ │ C3 │ │ C4 │            │    │
│  │  └────┘ └────┘ └────┘ └────┘            │    │
│  └─────────────────────────────────────────┘    │
│                      │                          │
│              acquire() │ release()              │
│                      ▼                          │
│  ┌─────────────────────────────────────────┐    │
│  │          In-Use Connections             │    │
│  │  ┌────┐ ┌────┐                          │    │
│  │  │ C5 │ │ C6 │                          │    │
│  │  └────┘ └────┘                          │    │
│  └─────────────────────────────────────────┘    │
└─────────────────────────────────────────────────┘
```

### 2. Scoped Connection

```cpp
{
    auto conn = pool.acquire();  // Get connection from pool
    conn->execute("INSERT INTO ...");
    // Connection automatically returned to pool
}
```

### 3. Pool Configuration

```cpp
PoolConfig config{
    .db_path = "app.db",
    .min_connections = 2,       // Pre-warm pool
    .max_connections = 10,      // Hard limit
    .acquire_timeout_ms = 5000, // Wait for available connection
    .idle_timeout_ms = 60000,   // Close idle connections
    .health_check_interval_ms = 30000
};
```

## Project Structure

```
step03-connection-pool/
├── CMakeLists.txt
├── vcpkg.json
├── include/
│   └── pool/
│       ├── connection_pool.hpp   # Main pool class
│       ├── pooled_connection.hpp # Scoped connection wrapper
│       └── pool_config.hpp       # Configuration
├── src/
│   ├── connection_pool.cpp
│   └── main.cpp
└── tests/
    └── pool_test.cpp
```

## Thread Safety

The pool uses:
- `std::mutex` for pool state protection
- `std::condition_variable` for blocking acquisition
- `std::atomic` for statistics counters

```cpp
class ConnectionPool {
    std::mutex mutex_;
    std::condition_variable available_;
    std::deque<std::unique_ptr<Database>> pool_;
    std::atomic<size_t> total_connections_{0};
    std::atomic<size_t> active_connections_{0};
};
```

## Building

```bash
cmake --preset=default
cmake --build build
./build/step03_demo
```

## Usage Example

```cpp
// Create pool
ConnectionPool pool(PoolConfig{
    .db_path = "multitenant.db",
    .max_connections = 10
});

// In request handler (any thread)
{
    auto conn = pool.acquire();  // Blocks if pool exhausted
    conn->execute("SELECT ...");
}  // Connection returned automatically

// Check pool health
auto stats = pool.stats();
spdlog::info("Active: {}/{}", stats.active, stats.total);
```

## Pool Statistics

```cpp
struct PoolStats {
    size_t total_connections;     // Total connections created
    size_t active_connections;    // Currently in use
    size_t available_connections; // Ready for use
    size_t waiting_threads;       // Threads waiting for connection
    size_t total_acquisitions;    // Lifetime acquisition count
    size_t total_releases;        // Lifetime release count
    size_t timeouts;              // Acquisition timeouts
};
```

## Next Step

Step 04 adds the Repository pattern for type-safe data access.
