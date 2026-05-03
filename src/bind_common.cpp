#include "common.h"

#include <cstring>

namespace simdjsonpy {

namespace {

[[noreturn]] void raise_python_builtin(PyObject *type,
                                       std::string_view message) {
  PyErr_SetString(type, std::string(message).c_str());
  throw nb::python_error();
}

}  // namespace

borrowed_bytes::borrowed_bytes(nb::handle handle) {
  PyObject *object = handle.ptr();
  if (PyUnicode_Check(object)) {
    Py_ssize_t size = 0;
    data_ = PyUnicode_AsUTF8AndSize(object, &size);
    if (data_ == nullptr) {
      throw nb::python_error();
    }
    size_ = static_cast<size_t>(size);
    return;
  }

  if (PyBytes_Check(object)) {
    data_ = PyBytes_AsString(object);
    size_ = static_cast<size_t>(PyBytes_GET_SIZE(object));
    return;
  }

  if (PyObject_GetBuffer(object, &buffer_, PyBUF_CONTIG_RO) == 0) {
    has_buffer_ = true;
    data_ = static_cast<const char *>(buffer_.buf);
    size_ = static_cast<size_t>(buffer_.len);
    return;
  }

  PyErr_Clear();
  throw nb::type_error(
      "expected str, bytes, bytearray, memoryview, or any contiguous "
      "bytes-like object");
}

borrowed_bytes::~borrowed_bytes() { reset(); }

borrowed_bytes::borrowed_bytes(borrowed_bytes &&other) noexcept
    : data_(other.data_),
      size_(other.size_),
      has_buffer_(other.has_buffer_),
      buffer_(other.buffer_) {
  other.data_ = nullptr;
  other.size_ = 0;
  other.has_buffer_ = false;
  other.buffer_ = {};
}

borrowed_bytes &borrowed_bytes::operator=(borrowed_bytes &&other) noexcept {
  if (this != &other) {
    reset();
    data_ = other.data_;
    size_ = other.size_;
    has_buffer_ = other.has_buffer_;
    buffer_ = other.buffer_;
    other.data_ = nullptr;
    other.size_ = 0;
    other.has_buffer_ = false;
    other.buffer_ = {};
  }
  return *this;
}

void borrowed_bytes::reset() noexcept {
  if (has_buffer_) {
    PyBuffer_Release(&buffer_);
    has_buffer_ = false;
    buffer_ = {};
  }
  data_ = nullptr;
  size_ = 0;
}

padded_string_wrapper::padded_string_wrapper()
    : storage_(std::make_shared<padded_string_storage>()) {}

padded_string_wrapper::padded_string_wrapper(simdjson::padded_string value)
    : storage_(std::make_shared<padded_string_storage>(std::move(value))) {}

size_t padded_string_wrapper::size() const noexcept { return storage_->value.size(); }

size_t padded_string_wrapper::length() const noexcept {
  return storage_->value.length();
}

std::string_view padded_string_wrapper::view() const noexcept {
  return static_cast<std::string_view>(storage_->value);
}

const simdjson::padded_string &padded_string_wrapper::value() const noexcept {
  return storage_->value;
}

simdjson::padded_string &padded_string_wrapper::value() noexcept {
  return storage_->value;
}

padded_string_ptr padded_string_wrapper::storage() const noexcept {
  return storage_;
}

bool padded_string_wrapper::append(std::string_view data) noexcept {
  return storage_->value.append(data.data(), data.size());
}

padded_string_wrapper padded_string_wrapper::from_python(nb::handle handle) {
  if (nb::isinstance<padded_string_wrapper>(handle)) {
    return nb::cast<padded_string_wrapper>(handle);
  }

  borrowed_bytes bytes(handle);
  return padded_string_wrapper(
      simdjson::padded_string(bytes.view().data(), bytes.view().size()));
}

padded_string_wrapper padded_string_wrapper::load(std::string_view path) {
  auto result = simdjson::padded_string::load(path);
  simdjson::padded_string value;
  ensure_success(std::move(result).get(value), "padded string load");
  return padded_string_wrapper(std::move(value));
}

bool padded_string_builder_wrapper::append(std::string_view data) noexcept {
  return builder_.append(data);
}

size_t padded_string_builder_wrapper::length() const noexcept {
  return builder_.length();
}

padded_string_wrapper padded_string_builder_wrapper::build() const noexcept {
  return padded_string_wrapper(builder_.build());
}

padded_string_wrapper padded_string_builder_wrapper::convert() noexcept {
  return padded_string_wrapper(builder_.convert());
}

std::string error_name(simdjson::error_code code) {
  switch (code) {
    case simdjson::SUCCESS:
      return "SUCCESS";
    case simdjson::CAPACITY:
      return "CAPACITY";
    case simdjson::MEMALLOC:
      return "MEMALLOC";
    case simdjson::TAPE_ERROR:
      return "TAPE_ERROR";
    case simdjson::DEPTH_ERROR:
      return "DEPTH_ERROR";
    case simdjson::STRING_ERROR:
      return "STRING_ERROR";
    case simdjson::T_ATOM_ERROR:
      return "T_ATOM_ERROR";
    case simdjson::F_ATOM_ERROR:
      return "F_ATOM_ERROR";
    case simdjson::N_ATOM_ERROR:
      return "N_ATOM_ERROR";
    case simdjson::NUMBER_ERROR:
      return "NUMBER_ERROR";
    case simdjson::BIGINT_ERROR:
      return "BIGINT_ERROR";
    case simdjson::UTF8_ERROR:
      return "UTF8_ERROR";
    case simdjson::UNINITIALIZED:
      return "UNINITIALIZED";
    case simdjson::EMPTY:
      return "EMPTY";
    case simdjson::UNESCAPED_CHARS:
      return "UNESCAPED_CHARS";
    case simdjson::UNCLOSED_STRING:
      return "UNCLOSED_STRING";
    case simdjson::UNSUPPORTED_ARCHITECTURE:
      return "UNSUPPORTED_ARCHITECTURE";
    case simdjson::INCORRECT_TYPE:
      return "INCORRECT_TYPE";
    case simdjson::NUMBER_OUT_OF_RANGE:
      return "NUMBER_OUT_OF_RANGE";
    case simdjson::INDEX_OUT_OF_BOUNDS:
      return "INDEX_OUT_OF_BOUNDS";
    case simdjson::NO_SUCH_FIELD:
      return "NO_SUCH_FIELD";
    case simdjson::IO_ERROR:
      return "IO_ERROR";
    case simdjson::INVALID_JSON_POINTER:
      return "INVALID_JSON_POINTER";
    case simdjson::INVALID_URI_FRAGMENT:
      return "INVALID_URI_FRAGMENT";
    case simdjson::UNEXPECTED_ERROR:
      return "UNEXPECTED_ERROR";
    case simdjson::PARSER_IN_USE:
      return "PARSER_IN_USE";
    case simdjson::OUT_OF_ORDER_ITERATION:
      return "OUT_OF_ORDER_ITERATION";
    case simdjson::INSUFFICIENT_PADDING:
      return "INSUFFICIENT_PADDING";
    case simdjson::INCOMPLETE_ARRAY_OR_OBJECT:
      return "INCOMPLETE_ARRAY_OR_OBJECT";
    case simdjson::SCALAR_DOCUMENT_AS_VALUE:
      return "SCALAR_DOCUMENT_AS_VALUE";
    case simdjson::OUT_OF_BOUNDS:
      return "OUT_OF_BOUNDS";
    case simdjson::TRAILING_CONTENT:
      return "TRAILING_CONTENT";
    case simdjson::OUT_OF_CAPACITY:
      return "OUT_OF_CAPACITY";
    case simdjson::NUM_ERROR_CODES:
      return "NUM_ERROR_CODES";
  }
  return "UNKNOWN_ERROR";
}

std::string build_error_message(simdjson::error_code code,
                                std::string_view context) {
  std::string message = error_name(code);
  message.append(": ");
  message.append(simdjson::error_message(code));
  if (!context.empty()) {
    message.append(" (");
    message.append(context);
    message.push_back(')');
  }
  return message;
}

[[noreturn]] void raise_simdjson_error(simdjson::error_code code,
                                       std::string_view context) {
  const std::string message = build_error_message(code, context);
  switch (code) {
    case simdjson::INCORRECT_TYPE:
      throw nb::type_error(message.c_str());
    case simdjson::NO_SUCH_FIELD:
      throw nb::key_error(message.c_str());
    case simdjson::INDEX_OUT_OF_BOUNDS:
      throw nb::index_error(message.c_str());
    case simdjson::INVALID_JSON_POINTER:
    case simdjson::INVALID_URI_FRAGMENT:
      throw nb::value_error(message.c_str());
    case simdjson::NUMBER_OUT_OF_RANGE:
      raise_python_builtin(PyExc_OverflowError, message);
    case simdjson::UTF8_ERROR:
      raise_python_builtin(PyExc_UnicodeError, message);
    default:
      throw simdjson_runtime_error(code, message);
  }
}

void ensure_success(simdjson::error_code code, std::string_view context) {
  if (code != simdjson::SUCCESS) {
    raise_simdjson_error(code, context);
  }
}

[[noreturn]] void raise_stale_reference(std::string_view what) {
  raise_python_builtin(PyExc_ReferenceError,
                       std::string(what) +
                           " is no longer valid because the underlying parser "
                           "or stream has advanced");
}

padded_string_wrapper padded_from_python(nb::handle handle) {
  return padded_string_wrapper::from_python(handle);
}

padded_string_ptr padded_storage_from_python(nb::handle handle) {
  return padded_from_python(handle).storage();
}

std::string python_text_from_handle(nb::handle handle) {
  borrowed_bytes bytes(handle);
  return std::string(bytes.view());
}

std::string minify_text(std::string_view text) {
  std::string output(text.size(), '\0');
  size_t output_size = 0;
  ensure_success(with_gil_released([&] {
                   return simdjson::minify(text.data(), text.size(),
                                           output.data(), output_size);
                 }),
                 "minify");
  output.resize(output_size);
  return output;
}

bool validate_utf8(nb::handle handle) {
  borrowed_bytes bytes(handle);
  return with_gil_released([&] { return simdjson::validate_utf8(bytes.view()); });
}

bool is_valid_json(nb::handle handle) {
  borrowed_bytes bytes(handle);
  simdjson::dom::parser parser;
  return with_gil_released([&] {
    auto result =
        parser.parse(reinterpret_cast<const uint8_t *>(bytes.view().data()),
                     bytes.view().size(), true);
    return result.error() == simdjson::SUCCESS;
  });
}

simdjson::dom::element_type dom_element_type(simdjson::dom::element element) {
  return element.type();
}

namespace {

nb::object dom_to_python(simdjson::dom::element element) {
  switch (element.type()) {
    case simdjson::dom::element_type::ARRAY: {
      auto arr = element.get_array().value_unsafe();
      Py_ssize_t len = static_cast<Py_ssize_t>(arr.size());
      PyObject *raw = PyList_New(len);
      if (!raw) throw nb::python_error();
      nb::object list = nb::steal(raw);
      Py_ssize_t i = 0;
      for (auto child : arr) {
        PyList_SET_ITEM(raw, i++, dom_to_python(child).release().ptr());
      }
      return list;
    }
    case simdjson::dom::element_type::OBJECT: {
      auto obj = element.get_object().value_unsafe();
      PyObject *raw = PyDict_New();
      if (!raw) throw nb::python_error();
      nb::object dict = nb::steal(raw);
      for (auto field : obj) {
        PyObject *raw_key = PyUnicode_FromStringAndSize(
            field.key.data(), static_cast<Py_ssize_t>(field.key.size()));
        if (!raw_key) throw nb::python_error();
        nb::object key = nb::steal(raw_key);
        nb::object value = dom_to_python(field.value);
        if (PyDict_SetItem(raw, key.ptr(), value.ptr()) < 0)
          throw nb::python_error();
      }
      return dict;
    }
    case simdjson::dom::element_type::INT64: {
      PyObject *obj = PyLong_FromLongLong(element.get_int64().value_unsafe());
      if (!obj) throw nb::python_error();
      return nb::steal(obj);
    }
    case simdjson::dom::element_type::UINT64: {
      PyObject *obj =
          PyLong_FromUnsignedLongLong(element.get_uint64().value_unsafe());
      if (!obj) throw nb::python_error();
      return nb::steal(obj);
    }
    case simdjson::dom::element_type::DOUBLE: {
      PyObject *obj = PyFloat_FromDouble(element.get_double().value_unsafe());
      if (!obj) throw nb::python_error();
      return nb::steal(obj);
    }
    case simdjson::dom::element_type::STRING: {
      auto sv = element.get_string().value_unsafe();
      PyObject *obj = PyUnicode_FromStringAndSize(
          sv.data(), static_cast<Py_ssize_t>(sv.size()));
      if (!obj) throw nb::python_error();
      return nb::steal(obj);
    }
    case simdjson::dom::element_type::BOOL:
      return nb::borrow(element.get_bool().value_unsafe() ? Py_True : Py_False);
    case simdjson::dom::element_type::NULL_VALUE:
      return nb::none();
    case simdjson::dom::element_type::BIGINT: {
      auto sv = element.get_bigint().value_unsafe();
      std::string str(sv);
      PyObject *obj = PyLong_FromString(str.c_str(), nullptr, 10);
      if (!obj) throw nb::python_error();
      return nb::steal(obj);
    }
  }
  return nb::none();
}

}  // namespace

void bind_common(nb::module_ &m) {
  m.attr("__version__") = "0.1.0";
  m.attr("SIMDJSON_PADDING") = nb::int_(simdjson::SIMDJSON_PADDING);
  m.attr("SIMDJSON_MAXSIZE_BYTES") = nb::int_(simdjson::SIMDJSON_MAXSIZE_BYTES);
  m.attr("DEFAULT_MAX_DEPTH") = nb::int_(simdjson::DEFAULT_MAX_DEPTH);

  nb::exception<simdjson_runtime_error>(m, "SimdjsonError", PyExc_RuntimeError);

  nb::enum_<simdjson::error_code>(m, "ErrorCode")
      .value("SUCCESS", simdjson::SUCCESS)
      .value("CAPACITY", simdjson::CAPACITY)
      .value("MEMALLOC", simdjson::MEMALLOC)
      .value("TAPE_ERROR", simdjson::TAPE_ERROR)
      .value("DEPTH_ERROR", simdjson::DEPTH_ERROR)
      .value("STRING_ERROR", simdjson::STRING_ERROR)
      .value("T_ATOM_ERROR", simdjson::T_ATOM_ERROR)
      .value("F_ATOM_ERROR", simdjson::F_ATOM_ERROR)
      .value("N_ATOM_ERROR", simdjson::N_ATOM_ERROR)
      .value("NUMBER_ERROR", simdjson::NUMBER_ERROR)
      .value("BIGINT_ERROR", simdjson::BIGINT_ERROR)
      .value("UTF8_ERROR", simdjson::UTF8_ERROR)
      .value("UNINITIALIZED", simdjson::UNINITIALIZED)
      .value("EMPTY", simdjson::EMPTY)
      .value("UNESCAPED_CHARS", simdjson::UNESCAPED_CHARS)
      .value("UNCLOSED_STRING", simdjson::UNCLOSED_STRING)
      .value("UNSUPPORTED_ARCHITECTURE",
             simdjson::UNSUPPORTED_ARCHITECTURE)
      .value("INCORRECT_TYPE", simdjson::INCORRECT_TYPE)
      .value("NUMBER_OUT_OF_RANGE", simdjson::NUMBER_OUT_OF_RANGE)
      .value("INDEX_OUT_OF_BOUNDS", simdjson::INDEX_OUT_OF_BOUNDS)
      .value("NO_SUCH_FIELD", simdjson::NO_SUCH_FIELD)
      .value("IO_ERROR", simdjson::IO_ERROR)
      .value("INVALID_JSON_POINTER", simdjson::INVALID_JSON_POINTER)
      .value("INVALID_URI_FRAGMENT", simdjson::INVALID_URI_FRAGMENT)
      .value("UNEXPECTED_ERROR", simdjson::UNEXPECTED_ERROR)
      .value("PARSER_IN_USE", simdjson::PARSER_IN_USE)
      .value("OUT_OF_ORDER_ITERATION", simdjson::OUT_OF_ORDER_ITERATION)
      .value("INSUFFICIENT_PADDING", simdjson::INSUFFICIENT_PADDING)
      .value("INCOMPLETE_ARRAY_OR_OBJECT",
             simdjson::INCOMPLETE_ARRAY_OR_OBJECT)
      .value("SCALAR_DOCUMENT_AS_VALUE", simdjson::SCALAR_DOCUMENT_AS_VALUE)
      .value("OUT_OF_BOUNDS", simdjson::OUT_OF_BOUNDS)
      .value("TRAILING_CONTENT", simdjson::TRAILING_CONTENT)
      .value("OUT_OF_CAPACITY", simdjson::OUT_OF_CAPACITY)
      .value("NUM_ERROR_CODES", simdjson::NUM_ERROR_CODES)
      .export_values();

  nb::class_<padded_string_wrapper>(m, "PaddedString",
                                    "Owned UTF-8 or bytes buffer with simdjson "
                                    "padding for zero-copy parsing.")
      .def(nb::init<>())
      .def(
          "__init__",
          [](padded_string_wrapper *self, nb::handle handle) {
            new (self) padded_string_wrapper(
                padded_string_wrapper::from_python(handle));
          },
          "data"_a)
      .def_prop_ro("size", &padded_string_wrapper::size)
      .def_prop_ro("length", &padded_string_wrapper::length)
      .def("__len__", &padded_string_wrapper::size)
      .def("append",
           [](padded_string_wrapper &self, nb::handle handle) {
             borrowed_bytes bytes(handle);
             return self.append(bytes.view());
           },
           "data"_a)
      .def("to_bytes",
           [](const padded_string_wrapper &self) {
             return nb::bytes(self.view().data(), self.view().size());
           })
      .def("__bytes__",
           [](const padded_string_wrapper &self) {
             return nb::bytes(self.view().data(), self.view().size());
           })
      .def("__repr__",
           [](const padded_string_wrapper &self) {
             return "PaddedString(size=" + std::to_string(self.size()) + ")";
           })
      .def_static("load", &padded_string_wrapper::load, "path"_a);

  nb::class_<padded_string_builder_wrapper>(
      m, "PaddedStringBuilder",
      "Incrementally build a padded string without repeated reallocations.")
      .def(nb::init<>())
      .def(nb::init<size_t>(), "capacity"_a)
      .def("append",
           [](padded_string_builder_wrapper &self, nb::handle handle) {
             borrowed_bytes bytes(handle);
             return self.append(bytes.view());
           },
           "data"_a)
      .def_prop_ro("length", &padded_string_builder_wrapper::length)
      .def("build", &padded_string_builder_wrapper::build)
      .def("convert", &padded_string_builder_wrapper::convert);

  nb::class_<simdjson::implementation>(m, "Implementation")
      .def_prop_ro("name", &simdjson::implementation::name)
      .def_prop_ro("description", &simdjson::implementation::description)
      .def("supported_by_runtime_system",
           &simdjson::implementation::supported_by_runtime_system)
      .def("validate_utf8",
           [](const simdjson::implementation &self, nb::handle handle) {
             borrowed_bytes bytes(handle);
             return with_gil_released([&] {
               return self.validate_utf8(bytes.view().data(),
                                         bytes.view().size());
             });
           },
           "data"_a)
      .def("minify",
           [](const simdjson::implementation &self, nb::handle handle) {
             borrowed_bytes bytes(handle);
             std::string output(bytes.view().size(), '\0');
             size_t output_size = 0;
             ensure_success(with_gil_released([&] {
                             return self.minify(
                                 reinterpret_cast<const uint8_t *>(
                                     bytes.view().data()),
                                 bytes.view().size(),
                                 reinterpret_cast<uint8_t *>(output.data()),
                                 output_size);
                           }),
                           "implementation minify");
             output.resize(output_size);
             return output;
           },
           "data"_a)
      .def("__repr__", [](const simdjson::implementation &self) {
        return "Implementation(name=" + self.name() +
               ", description=" + self.description() + ")";
      });

  m.def("error_message",
        [](simdjson::error_code code) {
          return std::string(simdjson::error_message(code));
        },
        "code"_a);
  m.def("error_name", &error_name, "code"_a);
  m.def("is_fatal", &simdjson::is_fatal, "code"_a);
  m.def("validate_utf8", &validate_utf8, "data"_a);
  m.def("minify", [](nb::handle handle) {
    borrowed_bytes bytes(handle);
    return minify_text(bytes.view());
  });
  m.def("is_valid", &is_valid_json, "data"_a);
  m.def(
      "active_implementation",
      []() -> const simdjson::implementation & {
        return *simdjson::get_active_implementation();
      },
      nb::rv_policy::reference);
  m.def(
      "builtin_implementation",
      []() -> const simdjson::implementation & {
        return *simdjson::builtin_implementation();
      },
      nb::rv_policy::reference);
  m.def("available_implementations",
        []() {
          nb::list implementations;
          for (const simdjson::implementation *implementation :
               simdjson::get_available_implementations()) {
            implementations.append(
                nb::cast(*implementation, nb::rv_policy::reference));
          }
          return implementations;
        });
  m.def("set_active_implementation",
        [](const simdjson::implementation *implementation) {
          if (implementation == nullptr) {
            throw nb::value_error("implementation must not be None");
          }
          simdjson::get_active_implementation() = implementation;
        },
        "implementation"_a);
  m.def("set_active_implementation",
        [](std::string_view name) {
          const simdjson::implementation *implementation =
              simdjson::get_available_implementations()[name];
          if (implementation == nullptr) {
            throw nb::key_error(
                ("unknown implementation: " + std::string(name)).c_str());
          }
          simdjson::get_active_implementation() = implementation;
        },
        "name"_a);
  m.def(
      "loads",
      [](nb::handle handle) -> nb::object {
        static thread_local simdjson::dom::parser parser;
        simdjson::dom::element root;
        if (nb::isinstance<padded_string_wrapper>(handle)) {
          const auto &ps = nb::cast<const padded_string_wrapper &>(handle);
          auto result =
              with_gil_released([&] { return parser.parse(ps.value()); });
          ensure_success(result.error(), "loads");
          root = result.value_unsafe();
        } else {
          borrowed_bytes bytes(handle);
          auto result = with_gil_released([&] {
            return parser.parse(
                reinterpret_cast<const uint8_t *>(bytes.view().data()),
                bytes.view().size(), true);
          });
          ensure_success(result.error(), "loads");
          root = result.value_unsafe();
        }
        return dom_to_python(root);
      },
      "data"_a);
}

}  // namespace simdjsonpy
