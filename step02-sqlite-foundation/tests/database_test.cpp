/**
 * Step 02: Database Tests
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "db/database.hpp"

using namespace db;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("Database can be opened", "[database]") {
    SECTION("In-memory database") {
        Database db(":memory:");
        REQUIRE(db.handle() != nullptr);
    }

    SECTION("With explicit config") {
        DatabaseConfig config{
            .path = ":memory:",
            .enable_foreign_keys = true
        };
        Database db(config);
        REQUIRE(db.handle() != nullptr);
    }
}

TEST_CASE("Database executes SQL", "[database]") {
    Database db(":memory:");

    SECTION("Create table") {
        REQUIRE_NOTHROW(db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY)"));
        REQUIRE(db.table_exists("test"));
    }

    SECTION("Insert and query") {
        db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
        db.execute("INSERT INTO test (value) VALUES ('hello')");

        auto result = db.query_single<std::string>("SELECT value FROM test");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == "hello");
    }
}

TEST_CASE("Prepared statements work", "[statement]") {
    Database db(":memory:");
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT, score REAL)");

    SECTION("Positional binding") {
        auto stmt = db.prepare("INSERT INTO test (name, score) VALUES (?, ?)");
        stmt.bind(1, "Alice");
        stmt.bind(2, 95.5);
        stmt.step();

        REQUIRE(db.last_insert_rowid() == 1);
    }

    SECTION("Named binding") {
        auto stmt = db.prepare("INSERT INTO test (name, score) VALUES (:name, :score)");
        stmt.bind(":name", "Bob");
        stmt.bind(":score", 87.3);
        stmt.step();

        REQUIRE(db.last_insert_rowid() == 1);
    }

    SECTION("Column access") {
        db.execute("INSERT INTO test (name, score) VALUES ('Test', 100.0)");

        auto stmt = db.prepare("SELECT id, name, score FROM test");
        REQUIRE(stmt.step());

        REQUIRE(stmt.column<int64_t>(0) == 1);
        REQUIRE(stmt.column<std::string>(1) == "Test");
        REQUIRE(stmt.column<double>(2) == 100.0);
    }

    SECTION("Statement reuse") {
        auto stmt = db.prepare("INSERT INTO test (name) VALUES (?)");

        stmt.bind(1, "First");
        stmt.step();

        stmt.reset();
        stmt.clear_bindings();
        stmt.bind(1, "Second");
        stmt.step();

        auto count = db.query_single<int>("SELECT COUNT(*) FROM test");
        REQUIRE(count.value() == 2);
    }
}

TEST_CASE("Transactions work", "[transaction]") {
    Database db(":memory:");
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");

    SECTION("Commit persists data") {
        {
            auto tx = db.transaction();
            db.execute("INSERT INTO test (value) VALUES ('committed')");
            tx.commit();
        }

        auto count = db.query_single<int>("SELECT COUNT(*) FROM test");
        REQUIRE(count.value() == 1);
    }

    SECTION("Rollback on scope exit") {
        {
            auto tx = db.transaction();
            db.execute("INSERT INTO test (value) VALUES ('rolled back')");
            // No commit - should rollback
        }

        auto count = db.query_single<int>("SELECT COUNT(*) FROM test");
        REQUIRE(count.value() == 0);
    }

    SECTION("Explicit rollback") {
        auto tx = db.transaction();
        db.execute("INSERT INTO test (value) VALUES ('will rollback')");
        tx.rollback();

        auto count = db.query_single<int>("SELECT COUNT(*) FROM test");
        REQUIRE(count.value() == 0);
    }
}

TEST_CASE("Savepoints work", "[savepoint]") {
    Database db(":memory:");
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT UNIQUE)");

    SECTION("Release commits changes") {
        auto tx = db.transaction();
        db.execute("INSERT INTO test (value) VALUES ('outer')");

        {
            auto sp = db.savepoint("inner");
            db.execute("INSERT INTO test (value) VALUES ('inner')");
            sp.release();
        }

        tx.commit();

        auto count = db.query_single<int>("SELECT COUNT(*) FROM test");
        REQUIRE(count.value() == 2);
    }

    SECTION("Rollback undoes savepoint changes") {
        auto tx = db.transaction();
        db.execute("INSERT INTO test (value) VALUES ('outer')");

        {
            auto sp = db.savepoint("inner");
            db.execute("INSERT INTO test (value) VALUES ('inner')");
            sp.rollback();
        }

        tx.commit();

        auto count = db.query_single<int>("SELECT COUNT(*) FROM test");
        REQUIRE(count.value() == 1);
    }
}

TEST_CASE("Exceptions are thrown correctly", "[exceptions]") {
    Database db(":memory:");
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT UNIQUE)");
    db.execute("INSERT INTO test (value) VALUES ('unique')");

    SECTION("Constraint violation") {
        REQUIRE_THROWS_AS(
            db.execute("INSERT INTO test (value) VALUES ('unique')"),
            ConstraintException
        );
    }

    SECTION("Syntax error") {
        REQUIRE_THROWS_AS(
            db.execute("INVALID SQL"),
            DatabaseException
        );
    }
}

TEST_CASE("Nullable columns", "[statement]") {
    Database db(":memory:");
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
    db.execute("INSERT INTO test (value) VALUES (NULL)");
    db.execute("INSERT INTO test (value) VALUES ('not null')");

    SECTION("is_null check") {
        auto stmt = db.prepare("SELECT value FROM test ORDER BY id");

        REQUIRE(stmt.step());
        REQUIRE(stmt.is_null(0));

        REQUIRE(stmt.step());
        REQUIRE_FALSE(stmt.is_null(0));
    }

    SECTION("column_optional") {
        auto stmt = db.prepare("SELECT value FROM test ORDER BY id");

        REQUIRE(stmt.step());
        auto val1 = stmt.column_optional<std::string>(0);
        REQUIRE_FALSE(val1.has_value());

        REQUIRE(stmt.step());
        auto val2 = stmt.column_optional<std::string>(0);
        REQUIRE(val2.has_value());
        REQUIRE(val2.value() == "not null");
    }
}

TEST_CASE("Query callback", "[database]") {
    Database db(":memory:");
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
    db.execute("INSERT INTO test (value) VALUES ('a'), ('b'), ('c')");

    std::vector<std::string> values;
    db.query("SELECT value FROM test ORDER BY id", [&](Statement& stmt) {
        values.push_back(stmt.column<std::string>(0));
    });

    REQUIRE(values.size() == 3);
    REQUIRE(values[0] == "a");
    REQUIRE(values[1] == "b");
    REQUIRE(values[2] == "c");
}
