#pragma once

#include <Python.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <nanobind/make_iterator.h>
#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/variant.h>
#include <nanobind/stl/vector.h>

#include <simdjson.h>

namespace nb = nanobind;
using namespace nb::literals;

namespace simdjsonpy {

struct simdjson_runtime_error : std::runtime_error {
  simdjson::error_code code;

  simdjson_runtime_error(simdjson::error_code code_, std::string message)
      : std::runtime_error(std::move(message)), code(code_) {}
};

struct padded_string_storage {
  simdjson::padded_string value;

  padded_string_storage() noexcept = default;
  explicit padded_string_storage(simdjson::padded_string value_) noexcept
      : value(std::move(value_)) {}
};

using padded_string_ptr = std::shared_ptr<padded_string_storage>;

class borrowed_bytes {
public:
  explicit borrowed_bytes(nb::handle handle);
  ~borrowed_bytes();

  borrowed_bytes(const borrowed_bytes &) = delete;
  borrowed_bytes &operator=(const borrowed_bytes &) = delete;

  borrowed_bytes(borrowed_bytes &&other) noexcept;
  borrowed_bytes &operator=(borrowed_bytes &&other) noexcept;

  [[nodiscard]] std::string_view view() const noexcept {
    return {data_, size_};
  }

private:
  void reset() noexcept;

  const char *data_{nullptr};
  size_t size_{0};
  bool has_buffer_{false};
  Py_buffer buffer_{};
};

class padded_string_wrapper {
public:
  padded_string_wrapper();
  explicit padded_string_wrapper(simdjson::padded_string value);

  [[nodiscard]] size_t size() const noexcept;
  [[nodiscard]] size_t length() const noexcept;
  [[nodiscard]] std::string_view view() const noexcept;
  [[nodiscard]] const simdjson::padded_string &value() const noexcept;
  [[nodiscard]] simdjson::padded_string &value() noexcept;
  [[nodiscard]] padded_string_ptr storage() const noexcept;

  bool append(std::string_view data) noexcept;

  static padded_string_wrapper from_python(nb::handle handle);
  static padded_string_wrapper load(std::string_view path);

private:
  padded_string_ptr storage_;
};

class padded_string_builder_wrapper {
public:
  padded_string_builder_wrapper() noexcept = default;
  explicit padded_string_builder_wrapper(size_t capacity) noexcept
      : builder_(capacity) {}

  bool append(std::string_view data) noexcept;
  [[nodiscard]] size_t length() const noexcept;
  [[nodiscard]] padded_string_wrapper build() const noexcept;
  [[nodiscard]] padded_string_wrapper convert() noexcept;

private:
  simdjson::padded_string_builder builder_{};
};

std::string error_name(simdjson::error_code code);
std::string build_error_message(simdjson::error_code code,
                                std::string_view context = {});
[[noreturn]] void raise_simdjson_error(simdjson::error_code code,
                                       std::string_view context = {});
void ensure_success(simdjson::error_code code,
                    std::string_view context = {});
[[noreturn]] void raise_stale_reference(std::string_view what);

padded_string_wrapper padded_from_python(nb::handle handle);
padded_string_ptr padded_storage_from_python(nb::handle handle);
std::string python_text_from_handle(nb::handle handle);

std::string minify_text(std::string_view text);
bool validate_utf8(nb::handle handle);
bool is_valid_json(nb::handle handle);

simdjson::dom::element_type dom_element_type(simdjson::dom::element element);

void bind_common(nb::module_ &m);
void bind_dom(nb::module_ &m);
void bind_ondemand(nb::module_ &m);

template <typename T>
inline std::string repr_with_json(std::string_view type_name, const T &value) {
  return std::string(type_name) + "(" + simdjson::minify(value) + ")";
}

template <typename Fn>
decltype(auto) with_gil_released(Fn &&fn) {
  nb::gil_scoped_release release;
  return std::forward<Fn>(fn)();
}

}  // namespace simdjsonpy
