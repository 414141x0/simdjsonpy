#pragma once

#include "common.h"

#include <sstream>

namespace simdjsonpy {

struct dom_document_state {
  nb::ft_mutex mutex{};
  simdjson::dom::document document{};
};

struct dom_stream_state {
  nb::ft_mutex mutex{};
  simdjson::dom::parser parser{};
  padded_string_ptr input{};
  simdjson::dom::document_stream stream{};
  bool started{false};
  size_t generation{0};
  size_t item_revision{0};
};

struct dom_stream_item_state {
  std::shared_ptr<dom_stream_state> stream{};
  size_t generation{0};
  size_t item_revision{0};
};

using dom_owner =
    std::variant<std::shared_ptr<dom_document_state>,
                 std::shared_ptr<dom_stream_item_state>>;

struct dom_element_wrapper;
struct dom_array_wrapper;
struct dom_object_wrapper;
struct dom_field_wrapper;

template <typename Fn>
decltype(auto) with_dom_owner(const dom_owner &owner, Fn &&fn) {
  if (const auto *item = std::get_if<std::shared_ptr<dom_stream_item_state>>(
          &owner)) {
    nb::ft_lock_guard guard((*item)->stream->mutex);
    if ((*item)->generation != (*item)->stream->generation ||
        (*item)->item_revision != (*item)->stream->item_revision) {
      raise_stale_reference("DOM stream item");
    }
    return std::forward<Fn>(fn)();
  }
  const auto &document = std::get<std::shared_ptr<dom_document_state>>(owner);
  nb::ft_lock_guard guard(document->mutex);
  return std::forward<Fn>(fn)();
}

struct dom_element_wrapper {
  dom_owner owner;
  simdjson::dom::element element{};

  dom_element_wrapper() = default;
  dom_element_wrapper(dom_owner owner_, simdjson::dom::element element_)
      : owner(std::move(owner_)), element(element_) {}

  simdjson::dom::element_type type() const;
  bool is_array() const;
  bool is_object() const;
  bool is_string() const;
  bool is_int64() const;
  bool is_uint64() const;
  bool is_double() const;
  bool is_number() const;
  bool is_bool() const;
  bool is_null() const;
  bool is_bigint() const;

  int64_t get_int64() const;
  uint64_t get_uint64() const;
  double get_double() const;
  bool get_bool() const;
  std::string get_string() const;
  size_t get_string_length() const;
  std::string get_bigint() const;
  dom_array_wrapper get_array() const;
  dom_object_wrapper get_object() const;

  dom_element_wrapper get_item(std::string_view key) const;
  dom_element_wrapper get_index(size_t index) const;
  dom_element_wrapper at_pointer(std::string_view json_pointer) const;
  dom_element_wrapper at_path(std::string_view json_path) const;
  std::vector<dom_element_wrapper> at_path_with_wildcard(
      std::string_view json_path) const;
  dom_element_wrapper at_key(std::string_view key) const;
  dom_element_wrapper at_key_case_insensitive(std::string_view key) const;

  size_t size_hint() const;
  std::string to_json() const;
  std::string prettify() const;
  std::string repr() const;
  bool equals(const dom_element_wrapper &other) const;
};

struct dom_document_wrapper {
  std::shared_ptr<dom_document_state> state;

  dom_document_wrapper();
  explicit dom_document_wrapper(std::shared_ptr<dom_document_state> state_)
      : state(std::move(state_)) {}

  dom_element_wrapper root() const;
  size_t capacity() const noexcept;
  std::string dump_raw_tape() const;
  std::string to_json() const;
  std::string prettify() const;
  std::string repr() const;
};

struct dom_array_wrapper {
  dom_owner owner;
  simdjson::dom::array array{};

  dom_array_wrapper() = default;
  dom_array_wrapper(dom_owner owner_, simdjson::dom::array array_)
      : owner(std::move(owner_)), array(array_) {}

  size_t size() const;
  size_t number_of_slots() const;
  dom_element_wrapper at(size_t index) const;
  dom_element_wrapper at_pointer(std::string_view json_pointer) const;
  dom_element_wrapper at_path(std::string_view json_path) const;
  std::vector<dom_element_wrapper> at_path_with_wildcard(
      std::string_view json_path) const;
  std::string to_json() const;
  std::string prettify() const;
  std::string repr() const;
};

struct dom_object_wrapper {
  dom_owner owner;
  simdjson::dom::object object{};

  dom_object_wrapper() = default;
  dom_object_wrapper(dom_owner owner_, simdjson::dom::object object_)
      : owner(std::move(owner_)), object(object_) {}

  size_t size() const;
  dom_element_wrapper get_item(std::string_view key) const;
  dom_element_wrapper at_key(std::string_view key) const;
  dom_element_wrapper at_key_case_insensitive(std::string_view key) const;
  dom_element_wrapper at_pointer(std::string_view json_pointer) const;
  dom_element_wrapper at_path(std::string_view json_path) const;
  std::vector<dom_element_wrapper> at_path_with_wildcard(
      std::string_view json_path) const;
  bool contains(std::string_view key) const;
  std::vector<std::string> keys() const;
  std::string to_json() const;
  std::string prettify() const;
  std::string repr() const;
};

struct dom_field_wrapper {
  dom_owner owner;
  std::string key;
  simdjson::dom::element value{};

  dom_field_wrapper() = default;
  dom_field_wrapper(dom_owner owner_, std::string key_,
                    simdjson::dom::element value_)
      : owner(std::move(owner_)),
        key(std::move(key_)),
        value(value_) {}

  dom_element_wrapper element() const;
  std::string repr() const;
};

struct dom_parser_wrapper {
  mutable nb::ft_mutex mutex{};
  simdjson::dom::parser parser{};

  explicit dom_parser_wrapper(
      size_t max_capacity = simdjson::SIMDJSON_MAXSIZE_BYTES)
      : parser(max_capacity) {}

  dom_document_wrapper parse(nb::handle handle);
  dom_document_wrapper parse_into(dom_document_wrapper &document,
                                  nb::handle handle);
  dom_document_wrapper load(std::string_view path);
  dom_document_wrapper load_into(dom_document_wrapper &document,
                                 std::string_view path);
  size_t capacity() const noexcept;
  size_t max_capacity() const noexcept;
  size_t max_depth() const noexcept;
  void set_max_capacity(size_t max_capacity) noexcept;
  void allocate(size_t capacity, size_t max_depth);
};

struct dom_stream_element_wrapper {
  std::shared_ptr<dom_stream_item_state> state{};
  simdjson::dom::element element{};

  dom_stream_element_wrapper() = default;
  dom_stream_element_wrapper(std::shared_ptr<dom_stream_item_state> state_,
                             simdjson::dom::element element_)
      : state(std::move(state_)), element(element_) {}

  dom_element_wrapper element_view() const;
};

struct dom_document_stream_wrapper {
  std::shared_ptr<dom_stream_state> state;

  dom_document_stream_wrapper() = default;
  explicit dom_document_stream_wrapper(std::shared_ptr<dom_stream_state> state_)
      : state(std::move(state_)) {}

  size_t size_in_bytes() const noexcept;
  size_t truncated_bytes() const noexcept;
};

}  // namespace simdjsonpy
