#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace repository {

/**
 * Base class for entities with common fields
 */
struct EntityBase {
  int64_t id = 0;
  std::string created_at;
  std::string updated_at;
};

/**
 * Concept for entities that have required metadata
 */
template <typename T>
concept Entity = requires {
  { T::table_name } -> std::convertible_to<std::string_view>;
  { T::primary_key } -> std::convertible_to<std::string_view>;
};

/**
 * Helper to get field count from tuple
 */
template <typename T> struct field_count;

template <typename... Args> struct field_count<std::tuple<Args...>> {
  static constexpr size_t value = sizeof...(Args);
};

/**
 * Field descriptor for entity mapping
 */
template <typename T, typename Field> struct FieldDescriptor {
  std::string_view name;
  Field T::*member;
  bool is_primary_key = false;
  bool is_auto_increment = false;
  bool is_nullable = false;
};

/**
 * Entity metadata holder
 */
template <typename T> struct EntityMeta {
  static inline std::vector<std::string> field_names;
  static inline std::string table_name;
  static inline std::string primary_key;
};

// ==================== Reflection-like Macros ====================

/**
 * Macro to define entity field names
 * Usage: ENTITY_FIELDS(User, id, name, email)
 */
#define ENTITY_FIELDS(Type, ...)                                               \
  template <>                                                                  \
  inline std::vector<std::string> repository::EntityMeta<Type>::field_names =  \
      {STRINGIFY_EACH(__VA_ARGS__)};

#define STRINGIFY_EACH(...) STRINGIFY_EACH_IMPL(__VA_ARGS__)
#define STRINGIFY_EACH_IMPL(...) STRINGIFY_EACH_EXPAND(__VA_ARGS__)
#define STRINGIFY_EACH_EXPAND(...) STRINGIFY_EACH_HELPER(__VA_ARGS__, SENTINEL)

#define STRINGIFY_EACH_HELPER(first, ...)                                      \
  #first __VA_OPT__(, STRINGIFY_EACH_HELPER(__VA_ARGS__))

/**
 * Macro to get a field as a tuple member
 */
#define FIELD_PTR(Type, field) &Type::field

/**
 * Complete entity registration
 */
#define REGISTER_ENTITY(Type, ...)                                             \
  namespace {                                                                  \
  inline auto _register_##Type = []() {                                        \
    repository::EntityMeta<Type>::field_names = {                              \
        FOR_EACH_STRINGIFY(__VA_ARGS__)};                                      \
    repository::EntityMeta<Type>::table_name = Type::table_name;               \
    repository::EntityMeta<Type>::primary_key = Type::primary_key;             \
    return true;                                                               \
  }();                                                                         \
  }

// Helper macros for FOR_EACH
#define PARENS ()

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...)                                                   \
  __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))

#define FOR_EACH_HELPER(macro, a1, ...)                                        \
  macro(a1) __VA_OPT__(, FOR_EACH_AGAIN PARENS(macro, __VA_ARGS__))

#define FOR_EACH_AGAIN() FOR_EACH_HELPER

#define STRINGIFY(x) #x
#define FOR_EACH_STRINGIFY(...) FOR_EACH(STRINGIFY, __VA_ARGS__)

} // namespace repository
