# Step 02: SQLite Foundation

Building on Step 01's vcpkg setup, this step introduces SQLite with a modern C++ wrapper.

## What's New in This Step

- SQLite3 dependency via vcpkg
- RAII-based database connection wrapper
- Prepared statements with type-safe binding
- Query result iteration
- Transaction support

## Key Concepts

### 1. RAII Connection Management

```cpp
class Database {
    sqlite3* db_ = nullptr;
public:
    Database(const std::string& path);
    ~Database() { if (db_) sqlite3_close(db_); }
    
    // Move-only semantics
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;
    Database(const Database&) = delete;
};
```

### 2. Prepared Statements

Prepared statements prevent SQL injection and improve performance:

```cpp
auto stmt = db.prepare("SELECT * FROM users WHERE tenant_id = ?");
stmt.bind(1, tenant_id);

while (stmt.step()) {
    auto id = stmt.column<int64_t>(0);
    auto name = stmt.column<std::string>(1);
}
```

### 3. Type-Safe Column Access

Template-based column extraction:

```cpp
template<typename T>
T Statement::column(int index);

// Specializations for:
// - int, int64_t
// - double
// - std::string
// - std::vector<uint8_t> (BLOB)
// - std::optional<T> (nullable)
```

### 4. Transaction Support

```cpp
{
    auto tx = db.transaction();
    db.execute("INSERT INTO ...");
    db.execute("UPDATE ...");
    tx.commit();  // Rollback if not committed
}
```

## Project Structure

```
step02-sqlite-foundation/
├── CMakeLists.txt
├── vcpkg.json
├── include/
│   └── db/
│       ├── database.hpp      # Main database class
│       ├── statement.hpp     # Prepared statements
│       ├── transaction.hpp   # Transaction wrapper
│       └── exceptions.hpp    # Custom exceptions
├── src/
│   ├── database.cpp
│   ├── statement.cpp
│   └── main.cpp
└── tests/
    └── database_test.cpp
```

## Building

```bash
# From step02-sqlite-foundation/
cmake --preset=default
cmake --build build
./build/step02_demo
```

## Code Highlights

### Exception Hierarchy

```cpp
class DatabaseException : public std::runtime_error {
    int error_code_;
public:
    DatabaseException(int code, const std::string& msg);
    int error_code() const { return error_code_; }
};

class ConstraintException : public DatabaseException { };
class BusyException : public DatabaseException { };
```

### Statement Binding

```cpp
// Positional binding (1-indexed)
stmt.bind(1, 42);
stmt.bind(2, "hello");
stmt.bind(3, 3.14);
stmt.bind(4, nullptr);  // NULL

// Named binding
stmt.bind(":tenant_id", tenant_id);
stmt.bind(":name", name);
```

## SQLite Configuration

Optimal settings for multi-tenant use:

```cpp
db.execute("PRAGMA journal_mode = WAL");      // Write-Ahead Logging
db.execute("PRAGMA synchronous = NORMAL");     // Balance safety/speed
db.execute("PRAGMA foreign_keys = ON");        // Enforce relationships
db.execute("PRAGMA busy_timeout = 5000");      // 5s timeout on locks
```

## Next Step

Step 03 builds a thread-safe connection pool on top of this foundation.
