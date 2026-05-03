#pragma once

#include "common.h"

namespace simdjsonpy {

struct ondemand_document_state {
  nb::ft_mutex mutex{};
  simdjson::ondemand::parser parser{};
  padded_string_ptr input{};
  simdjson::ondemand::document document{};

  explicit ondemand_document_state(size_t max_capacity)
      : parser(max_capacity) {}
};

struct ondemand_stream_state {
  nb::ft_mutex mutex{};
  simdjson::ondemand::parser parser{};
  padded_string_ptr input{};
  simdjson::ondemand::document_stream stream{};
  bool started{false};
  size_t generation{0};
  size_t item_revision{0};

  explicit ondemand_stream_state(size_t max_capacity)
      : parser(max_capacity) {}
};

struct ondemand_document_reference_state {
  std::shared_ptr<ondemand_stream_state> stream{};
  size_t generation{0};
  size_t item_revision{0};
  simdjson::ondemand::document_reference document{};
};

using ondemand_owner =
    std::variant<std::shared_ptr<ondemand_document_state>,
                 std::shared_ptr<ondemand_document_reference_state>>;

struct ondemand_number_wrapper {
  simdjson::ondemand::number number{};

  ondemand_number_wrapper() = default;
  explicit ondemand_number_wrapper(simdjson::ondemand::number number_)
      : number(number_) {}

  simdjson::ondemand::number_type type() const noexcept;
  bool is_uint64() const noexcept;
  uint64_t get_uint64() const noexcept;
  bool is_int64() const noexcept;
  int64_t get_int64() const noexcept;
  bool is_double() const noexcept;
  double get_double() const noexcept;
  double as_double() const noexcept;
  std::string repr() const;
};

template <typename Fn>
decltype(auto) with_ondemand_owner(const ondemand_owner &owner, Fn &&fn) {
  if (const auto *document =
          std::get_if<std::shared_ptr<ondemand_document_state>>(&owner)) {
    nb::ft_lock_guard guard((*document)->mutex);
    return std::forward<Fn>(fn)();
  }

  const auto &reference =
      *std::get<std::shared_ptr<ondemand_document_reference_state>>(owner);
  nb::ft_lock_guard guard(reference.stream->mutex);
  if (reference.generation != reference.stream->generation ||
      reference.item_revision != reference.stream->item_revision) {
    raise_stale_reference("On-Demand stream item");
  }
  return std::forward<Fn>(fn)();
}

struct ondemand_value_wrapper;
struct ondemand_array_wrapper;
struct ondemand_object_wrapper;
struct ondemand_field_wrapper;
struct ondemand_raw_json_string_wrapper;

struct ondemand_raw_json_string_wrapper {
  ondemand_owner owner;
  simdjson::ondemand::raw_json_string value{};

  ondemand_raw_json_string_wrapper() = default;
  ondemand_raw_json_string_wrapper(ondemand_owner owner_,
                                   simdjson::ondemand::raw_json_string value_)
      : owner(std::move(owner_)), value(value_) {}

  std::string raw() const;
  std::string unescape(bool allow_replacement) const;
  std::string unescape_wobbly() const;
  bool is_equal(std::string_view other) const;
  std::string repr() const;
};

struct ondemand_value_wrapper {
  ondemand_owner owner;
  mutable simdjson::ondemand::value value{};

  ondemand_value_wrapper() = default;
  ondemand_value_wrapper(ondemand_owner owner_, simdjson::ondemand::value value_)
      : owner(std::move(owner_)), value(value_) {}

  ondemand_array_wrapper get_array() const;
  ondemand_object_wrapper get_object() const;
  uint64_t get_uint64() const;
  uint64_t get_uint64_in_string() const;
  int64_t get_int64() const;
  int64_t get_int64_in_string() const;
  uint32_t get_uint32() const;
  int32_t get_int32() const;
  double get_double() const;
  double get_double_in_string() const;
  std::string get_string(bool allow_replacement) const;
  std::string get_wobbly_string() const;
  ondemand_raw_json_string_wrapper get_raw_json_string() const;
  bool get_bool() const;
  bool is_null() const;
  size_t count_elements() const;
  size_t count_fields() const;
  ondemand_value_wrapper at(size_t index) const;
  ondemand_value_wrapper find_field(std::string_view key) const;
  ondemand_value_wrapper find_field_unordered(std::string_view key) const;
  ondemand_value_wrapper at_pointer(std::string_view json_pointer) const;
  ondemand_value_wrapper at_path(std::string_view json_path) const;
  simdjson::ondemand::json_type type() const;
  bool is_scalar() const;
  bool is_string() const;
  bool is_negative() const;
  bool is_integer() const;
  simdjson::ondemand::number_type get_number_type() const;
  ondemand_number_wrapper get_number() const;
  std::string raw_json_token() const;
  std::string raw_json() const;
  size_t current_depth() const;
  size_t current_location() const;
  std::string to_json() const;
  std::string repr() const;
};

struct ondemand_array_wrapper {
  ondemand_owner owner;
  mutable simdjson::ondemand::array array{};

  ondemand_array_wrapper() = default;
  ondemand_array_wrapper(ondemand_owner owner_, simdjson::ondemand::array array_)
      : owner(std::move(owner_)), array(array_) {}

  size_t count_elements() const;
  bool is_empty() const;
  bool reset() const;
  ondemand_value_wrapper at(size_t index) const;
  ondemand_value_wrapper at_pointer(std::string_view json_pointer) const;
  ondemand_value_wrapper at_path(std::string_view json_path) const;
  std::string raw_json() const;
  std::string to_json() const;
  std::string repr() const;
};

struct ondemand_object_wrapper {
  ondemand_owner owner;
  mutable simdjson::ondemand::object object{};

  ondemand_object_wrapper() = default;
  ondemand_object_wrapper(ondemand_owner owner_,
                          simdjson::ondemand::object object_)
      : owner(std::move(owner_)), object(object_) {}

  ondemand_value_wrapper find_field(std::string_view key) const;
  ondemand_value_wrapper find_field_unordered(std::string_view key) const;
  ondemand_value_wrapper at_pointer(std::string_view json_pointer) const;
  ondemand_value_wrapper at_path(std::string_view json_path) const;
  bool reset() const;
  bool is_empty() const;
  size_t count_fields() const;
  std::string raw_json() const;
  bool contains(std::string_view key) const;
  std::vector<std::string> keys() const;
  std::string to_json() const;
  std::string repr() const;
};

struct ondemand_field_wrapper {
  ondemand_owner owner;
  mutable simdjson::ondemand::field field{};

  ondemand_field_wrapper() = default;
  ondemand_field_wrapper(ondemand_owner owner_, simdjson::ondemand::field field_)
      : owner(std::move(owner_)), field(std::move(field_)) {}

  std::string unescaped_key(bool allow_replacement) const;
  ondemand_raw_json_string_wrapper key() const;
  std::string key_raw_json_token() const;
  std::string escaped_key() const;
  ondemand_value_wrapper value() const;
  std::string repr() const;
};

struct ondemand_document_wrapper {
  std::shared_ptr<ondemand_document_state> state;

  ondemand_document_wrapper() = default;
  explicit ondemand_document_wrapper(
      std::shared_ptr<ondemand_document_state> state_)
      : state(std::move(state_)) {}

  ondemand_array_wrapper get_array() const;
  ondemand_object_wrapper get_object() const;
  uint64_t get_uint64() const;
  uint64_t get_uint64_in_string() const;
  int64_t get_int64() const;
  int64_t get_int64_in_string() const;
  uint32_t get_uint32() const;
  int32_t get_int32() const;
  double get_double() const;
  double get_double_in_string() const;
  std::string get_string(bool allow_replacement) const;
  std::string get_wobbly_string() const;
  ondemand_raw_json_string_wrapper get_raw_json_string() const;
  bool get_bool() const;
  ondemand_value_wrapper get_value() const;
  bool is_null() const;
  size_t count_elements() const;
  size_t count_fields() const;
  ondemand_value_wrapper at(size_t index) const;
  ondemand_value_wrapper find_field(std::string_view key) const;
  ondemand_value_wrapper find_field_unordered(std::string_view key) const;
  ondemand_value_wrapper at_pointer(std::string_view json_pointer) const;
  ondemand_value_wrapper at_path(std::string_view json_path) const;
  simdjson::ondemand::json_type type() const;
  bool is_scalar() const;
  bool is_string() const;
  bool is_negative() const;
  bool is_integer() const;
  simdjson::ondemand::number_type get_number_type() const;
  ondemand_number_wrapper get_number() const;
  std::string raw_json_token() const;
  std::string raw_json() const;
  void rewind() const;
  bool is_alive() const;
  bool at_end() const;
  size_t current_depth() const;
  size_t current_location() const;
  std::string to_json() const;
  std::string repr() const;
};

struct ondemand_document_reference_wrapper {
  std::shared_ptr<ondemand_document_reference_state> state;

  ondemand_document_reference_wrapper() = default;
  explicit ondemand_document_reference_wrapper(
      std::shared_ptr<ondemand_document_reference_state> state_)
      : state(std::move(state_)) {}

  ondemand_array_wrapper get_array() const;
  ondemand_object_wrapper get_object() const;
  uint64_t get_uint64() const;
  uint64_t get_uint64_in_string() const;
  int64_t get_int64() const;
  int64_t get_int64_in_string() const;
  uint32_t get_uint32() const;
  int32_t get_int32() const;
  double get_double() const;
  double get_double_in_string() const;
  std::string get_string(bool allow_replacement) const;
  std::string get_wobbly_string() const;
  ondemand_raw_json_string_wrapper get_raw_json_string() const;
  bool get_bool() const;
  ondemand_value_wrapper get_value() const;
  bool is_null() const;
  size_t count_elements() const;
  size_t count_fields() const;
  ondemand_value_wrapper at(size_t index) const;
  ondemand_value_wrapper find_field(std::string_view key) const;
  ondemand_value_wrapper find_field_unordered(std::string_view key) const;
  ondemand_value_wrapper at_pointer(std::string_view json_pointer) const;
  ondemand_value_wrapper at_path(std::string_view json_path) const;
  simdjson::ondemand::json_type type() const;
  bool is_scalar() const;
  bool is_string() const;
  bool is_negative() const;
  bool is_integer() const;
  simdjson::ondemand::number_type get_number_type() const;
  ondemand_number_wrapper get_number() const;
  std::string raw_json_token() const;
  std::string raw_json() const;
  void rewind() const;
  size_t current_depth() const;
  size_t current_location() const;
  std::string to_json() const;
  std::string repr() const;
};

struct ondemand_parser_wrapper {
  mutable nb::ft_mutex mutex{};
  size_t max_capacity_{simdjson::SIMDJSON_MAXSIZE_BYTES};
  size_t allocated_capacity_{0};
  size_t max_depth_{simdjson::DEFAULT_MAX_DEPTH};
  bool threaded_{simdjson::ondemand::parser{}.threaded};

  explicit ondemand_parser_wrapper(
      size_t max_capacity = simdjson::SIMDJSON_MAXSIZE_BYTES)
      : max_capacity_(max_capacity) {}

  ondemand_document_wrapper iterate(nb::handle handle);
  size_t capacity() const noexcept;
  size_t max_capacity() const noexcept;
  void set_max_capacity(size_t max_capacity) noexcept;
  size_t max_depth() const noexcept;
  void allocate(size_t capacity, size_t max_depth);
  bool threaded() const noexcept;
  void set_threaded(bool threaded) noexcept;
};

struct ondemand_document_stream_wrapper {
  std::shared_ptr<ondemand_stream_state> state;

  ondemand_document_stream_wrapper() = default;
  explicit ondemand_document_stream_wrapper(
      std::shared_ptr<ondemand_stream_state> state_)
      : state(std::move(state_)) {}

  size_t size_in_bytes() const noexcept;
  size_t truncated_bytes() const noexcept;
};

}  // namespace simdjsonpy
