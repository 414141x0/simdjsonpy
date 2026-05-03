#include "dom.h"

#include <sstream>

namespace simdjsonpy {

namespace {

dom_element_wrapper wrap_element(dom_owner owner, simdjson::dom::element element) {
  return dom_element_wrapper(std::move(owner), element);
}

dom_array_wrapper wrap_array(dom_owner owner, simdjson::dom::array array) {
  return dom_array_wrapper(std::move(owner), array);
}

dom_object_wrapper wrap_object(dom_owner owner, simdjson::dom::object object) {
  return dom_object_wrapper(std::move(owner), object);
}

dom_field_wrapper wrap_field(dom_owner owner,
                             simdjson::dom::key_value_pair pair) {
  return dom_field_wrapper(std::move(owner), std::string(pair.key),
                           pair.value);
}

simdjson::dom::parser make_dom_parser_from_template(
    const dom_parser_wrapper &wrapper) {
  nb::ft_lock_guard guard(wrapper.mutex);
  simdjson::dom::parser parser(wrapper.parser.max_capacity());
  const size_t capacity = wrapper.parser.capacity();
  if (capacity != 0) {
    ensure_success(parser.allocate(capacity, wrapper.parser.max_depth()),
                   "DOM parser clone allocate");
  }
  return parser;
}

struct dom_array_iterator_wrapper {
  dom_array_wrapper array;
  simdjson::dom::array::iterator current{};
  simdjson::dom::array::iterator end{};

  explicit dom_array_iterator_wrapper(dom_array_wrapper array_)
      : array(std::move(array_)) {
    with_dom_owner(array.owner, [&] {
      current = array.array.begin();
      end = array.array.end();
      return 0;
    });
  }

  dom_element_wrapper next() {
    return with_dom_owner(array.owner, [&] {
      if (!(current != end)) {
        throw nb::stop_iteration();
      }
      auto value = *current;
      ++current;
      return wrap_element(array.owner, value);
    });
  }
};

struct dom_object_key_iterator_wrapper {
  dom_object_wrapper object;
  simdjson::dom::object::iterator current{};
  simdjson::dom::object::iterator end{};

  explicit dom_object_key_iterator_wrapper(dom_object_wrapper object_)
      : object(std::move(object_)) {
    with_dom_owner(object.owner, [&] {
      current = object.object.begin();
      end = object.object.end();
      return 0;
    });
  }

  std::string next() {
    return with_dom_owner(object.owner, [&] {
      if (!(current != end)) {
        throw nb::stop_iteration();
      }
      const std::string key(current.key());
      ++current;
      return key;
    });
  }
};

struct dom_object_field_iterator_wrapper {
  dom_object_wrapper object;
  simdjson::dom::object::iterator current{};
  simdjson::dom::object::iterator end{};

  explicit dom_object_field_iterator_wrapper(dom_object_wrapper object_)
      : object(std::move(object_)) {
    with_dom_owner(object.owner, [&] {
      current = object.object.begin();
      end = object.object.end();
      return 0;
    });
  }

  dom_field_wrapper next() {
    return with_dom_owner(object.owner, [&] {
      if (!(current != end)) {
        throw nb::stop_iteration();
      }
      auto pair = *current;
      ++current;
      return wrap_field(object.owner, pair);
    });
  }
};

struct dom_document_stream_iterator_wrapper {
  std::shared_ptr<dom_stream_state> state;
  simdjson::dom::document_stream::iterator current{};
  simdjson::dom::document_stream::iterator end{};
  bool started{false};

  explicit dom_document_stream_iterator_wrapper(
      std::shared_ptr<dom_stream_state> state_)
      : state(std::move(state_)) {}

  dom_element_wrapper next() {
    nb::ft_lock_guard guard(state->mutex);
    if (!started) {
      current = state->stream.begin();
      end = state->stream.end();
      started = true;
    } else {
      ++current;
    }
    if (!(current != end)) {
      throw nb::stop_iteration();
    }
    auto result = *current;
    ensure_success(result.error(), "DOM document stream item");
    ++state->item_revision;
    auto item_state = std::make_shared<dom_stream_item_state>();
    item_state->stream = state;
    item_state->generation = state->generation;
    item_state->item_revision = state->item_revision;
    auto element = result.value_unsafe();
    return wrap_element(item_state, element);
  }
};

}  // namespace

simdjson::dom::element_type dom_element_wrapper::type() const {
  return with_dom_owner(owner, [&] { return dom_element_type(element); });
}

bool dom_element_wrapper::is_array() const {
  return with_dom_owner(owner, [&] { return element.is_array(); });
}

bool dom_element_wrapper::is_object() const {
  return with_dom_owner(owner, [&] { return element.is_object(); });
}

bool dom_element_wrapper::is_string() const {
  return with_dom_owner(owner, [&] { return element.is_string(); });
}

bool dom_element_wrapper::is_int64() const {
  return with_dom_owner(owner, [&] { return element.is_int64(); });
}

bool dom_element_wrapper::is_uint64() const {
  return with_dom_owner(owner, [&] { return element.is_uint64(); });
}

bool dom_element_wrapper::is_double() const {
  return with_dom_owner(owner, [&] { return element.is_double(); });
}

bool dom_element_wrapper::is_number() const {
  return with_dom_owner(owner, [&] { return element.is_number(); });
}

bool dom_element_wrapper::is_bool() const {
  return with_dom_owner(owner, [&] { return element.is_bool(); });
}

bool dom_element_wrapper::is_null() const {
  return with_dom_owner(owner, [&] { return element.is_null(); });
}

bool dom_element_wrapper::is_bigint() const {
  return with_dom_owner(owner, [&] { return element.is_bigint(); });
}

int64_t dom_element_wrapper::get_int64() const {
  return with_dom_owner(owner, [&] {
    auto result = element.get_int64();
    ensure_success(result.error(), "DOM int64");
    return result.value_unsafe();
  });
}

uint64_t dom_element_wrapper::get_uint64() const {
  return with_dom_owner(owner, [&] {
    auto result = element.get_uint64();
    ensure_success(result.error(), "DOM uint64");
    return result.value_unsafe();
  });
}

double dom_element_wrapper::get_double() const {
  return with_dom_owner(owner, [&] {
    auto result = element.get_double();
    ensure_success(result.error(), "DOM double");
    return result.value_unsafe();
  });
}

bool dom_element_wrapper::get_bool() const {
  return with_dom_owner(owner, [&] {
    auto result = element.get_bool();
    ensure_success(result.error(), "DOM bool");
    return result.value_unsafe();
  });
}

std::string dom_element_wrapper::get_string() const {
  return with_dom_owner(owner, [&] {
    auto result = element.get_string();
    ensure_success(result.error(), "DOM string");
    return std::string(result.value_unsafe());
  });
}

size_t dom_element_wrapper::get_string_length() const {
  return with_dom_owner(owner, [&] {
    auto result = element.get_string_length();
    ensure_success(result.error(), "DOM string length");
    return result.value_unsafe();
  });
}

std::string dom_element_wrapper::get_bigint() const {
  return with_dom_owner(owner, [&] {
    auto result = element.get_bigint();
    ensure_success(result.error(), "DOM bigint");
    return std::string(result.value_unsafe());
  });
}

dom_array_wrapper dom_element_wrapper::get_array() const {
  return with_dom_owner(owner, [&] {
    auto result = element.get_array();
    ensure_success(result.error(), "DOM array");
    return wrap_array(owner, result.value_unsafe());
  });
}

dom_object_wrapper dom_element_wrapper::get_object() const {
  return with_dom_owner(owner, [&] {
    auto result = element.get_object();
    ensure_success(result.error(), "DOM object");
    return wrap_object(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_element_wrapper::get_item(std::string_view key) const {
  return with_dom_owner(owner, [&] {
    auto result = element[key];
    ensure_success(result.error(), "DOM object lookup");
    return wrap_element(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_element_wrapper::get_index(size_t index) const {
  return with_dom_owner(owner, [&] {
    auto result = element.at(index);
    ensure_success(result.error(), "DOM array lookup");
    return wrap_element(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_element_wrapper::at_pointer(
    std::string_view json_pointer) const {
  return with_dom_owner(owner, [&] {
    auto result = element.at_pointer(json_pointer);
    ensure_success(result.error(), "DOM JSON pointer");
    return wrap_element(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_element_wrapper::at_path(
    std::string_view json_path) const {
  return with_dom_owner(owner, [&] {
    auto result = element.at_path(json_path);
    ensure_success(result.error(), "DOM JSONPath");
    return wrap_element(owner, result.value_unsafe());
  });
}

std::vector<dom_element_wrapper> dom_element_wrapper::at_path_with_wildcard(
    std::string_view json_path) const {
  return with_dom_owner(owner, [&] {
    auto result = element.at_path_with_wildcard(json_path);
    ensure_success(result.error(), "DOM JSONPath wildcard");
    std::vector<dom_element_wrapper> wrapped;
    wrapped.reserve(result.value_unsafe().size());
    for (auto value : result.value_unsafe()) {
      wrapped.emplace_back(owner, value);
    }
    return wrapped;
  });
}

dom_element_wrapper dom_element_wrapper::at_key(std::string_view key) const {
  return with_dom_owner(owner, [&] {
    auto result = element.at_key(key);
    ensure_success(result.error(), "DOM key lookup");
    return wrap_element(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_element_wrapper::at_key_case_insensitive(
    std::string_view key) const {
  return with_dom_owner(owner, [&] {
    auto result = element.at_key_case_insensitive(key);
    ensure_success(result.error(), "DOM case-insensitive key lookup");
    return wrap_element(owner, result.value_unsafe());
  });
}

size_t dom_element_wrapper::size_hint() const {
  if (is_array()) {
    return get_array().size();
  }
  if (is_object()) {
    return get_object().size();
  }
  throw nb::type_error("DOM element is not an array or object");
}

std::string dom_element_wrapper::to_json() const {
  return with_dom_owner(owner, [&] { return simdjson::minify(element); });
}

std::string dom_element_wrapper::prettify() const {
  return with_dom_owner(owner, [&] { return simdjson::prettify(element); });
}

std::string dom_element_wrapper::repr() const {
  return "DOMElement(" + to_json() + ")";
}

bool dom_element_wrapper::equals(const dom_element_wrapper &other) const {
  return to_json() == other.to_json();
}

dom_document_wrapper::dom_document_wrapper()
    : state(std::make_shared<dom_document_state>()) {}

dom_element_wrapper dom_document_wrapper::root() const {
  nb::ft_lock_guard guard(state->mutex);
  return wrap_element(state, state->document.root());
}

size_t dom_document_wrapper::capacity() const noexcept {
  nb::ft_lock_guard guard(state->mutex);
  return state->document.capacity();
}

std::string dom_document_wrapper::dump_raw_tape() const {
  nb::ft_lock_guard guard(state->mutex);
  std::ostringstream stream;
  if (!state->document.dump_raw_tape(stream)) {
    throw simdjson_runtime_error(simdjson::TAPE_ERROR,
                                 "unable to dump DOM raw tape");
  }
  return stream.str();
}

std::string dom_document_wrapper::to_json() const { return root().to_json(); }

std::string dom_document_wrapper::prettify() const { return root().prettify(); }

std::string dom_document_wrapper::repr() const {
  return "DOMDocument(" + to_json() + ")";
}

size_t dom_array_wrapper::size() const {
  return with_dom_owner(owner, [&] { return array.size(); });
}

size_t dom_array_wrapper::number_of_slots() const {
  return with_dom_owner(owner, [&] { return array.number_of_slots(); });
}

dom_element_wrapper dom_array_wrapper::at(size_t index) const {
  return with_dom_owner(owner, [&] {
    auto result = array.at(index);
    ensure_success(result.error(), "DOM array at");
    return wrap_element(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_array_wrapper::at_pointer(
    std::string_view json_pointer) const {
  return with_dom_owner(owner, [&] {
    auto result = array.at_pointer(json_pointer);
    ensure_success(result.error(), "DOM array JSON pointer");
    return wrap_element(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_array_wrapper::at_path(
    std::string_view json_path) const {
  return with_dom_owner(owner, [&] {
    auto result = array.at_path(json_path);
    ensure_success(result.error(), "DOM array JSONPath");
    return wrap_element(owner, result.value_unsafe());
  });
}

std::vector<dom_element_wrapper> dom_array_wrapper::at_path_with_wildcard(
    std::string_view json_path) const {
  return with_dom_owner(owner, [&] {
    auto result = array.at_path_with_wildcard(json_path);
    ensure_success(result.error(), "DOM array JSONPath wildcard");
    std::vector<dom_element_wrapper> wrapped;
    wrapped.reserve(result.value_unsafe().size());
    for (auto value : result.value_unsafe()) {
      wrapped.emplace_back(owner, value);
    }
    return wrapped;
  });
}

std::string dom_array_wrapper::to_json() const {
  return with_dom_owner(owner, [&] { return simdjson::minify(array); });
}

std::string dom_array_wrapper::prettify() const {
  return with_dom_owner(owner, [&] { return simdjson::prettify(array); });
}

std::string dom_array_wrapper::repr() const {
  return "DOMArray(" + to_json() + ")";
}

size_t dom_object_wrapper::size() const {
  return with_dom_owner(owner, [&] { return object.size(); });
}

dom_element_wrapper dom_object_wrapper::get_item(std::string_view key) const {
  return with_dom_owner(owner, [&] {
    auto result = object[key];
    ensure_success(result.error(), "DOM object lookup");
    return wrap_element(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_object_wrapper::at_key(std::string_view key) const {
  return with_dom_owner(owner, [&] {
    auto result = object.at_key(key);
    ensure_success(result.error(), "DOM object key lookup");
    return wrap_element(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_object_wrapper::at_key_case_insensitive(
    std::string_view key) const {
  return with_dom_owner(owner, [&] {
    auto result = object.at_key_case_insensitive(key);
    ensure_success(result.error(), "DOM object case-insensitive key lookup");
    return wrap_element(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_object_wrapper::at_pointer(
    std::string_view json_pointer) const {
  return with_dom_owner(owner, [&] {
    auto result = object.at_pointer(json_pointer);
    ensure_success(result.error(), "DOM object JSON pointer");
    return wrap_element(owner, result.value_unsafe());
  });
}

dom_element_wrapper dom_object_wrapper::at_path(
    std::string_view json_path) const {
  return with_dom_owner(owner, [&] {
    auto result = object.at_path(json_path);
    ensure_success(result.error(), "DOM object JSONPath");
    return wrap_element(owner, result.value_unsafe());
  });
}

std::vector<dom_element_wrapper> dom_object_wrapper::at_path_with_wildcard(
    std::string_view json_path) const {
  return with_dom_owner(owner, [&] {
    auto result = object.at_path_with_wildcard(json_path);
    ensure_success(result.error(), "DOM object JSONPath wildcard");
    std::vector<dom_element_wrapper> wrapped;
    wrapped.reserve(result.value_unsafe().size());
    for (auto value : result.value_unsafe()) {
      wrapped.emplace_back(owner, value);
    }
    return wrapped;
  });
}

bool dom_object_wrapper::contains(std::string_view key) const {
  return with_dom_owner(owner, [&] {
    auto result = object[key];
    if (result.error() == simdjson::NO_SUCH_FIELD) {
      return false;
    }
    ensure_success(result.error(), "DOM object contains");
    return true;
  });
}

std::vector<std::string> dom_object_wrapper::keys() const {
  return with_dom_owner(owner, [&] {
    std::vector<std::string> result;
    result.reserve(object.size());
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
      result.emplace_back(iterator.key());
    }
    return result;
  });
}

std::string dom_object_wrapper::to_json() const {
  return with_dom_owner(owner, [&] { return simdjson::minify(object); });
}

std::string dom_object_wrapper::prettify() const {
  return with_dom_owner(owner, [&] { return simdjson::prettify(object); });
}

std::string dom_object_wrapper::repr() const {
  return "DOMObject(" + to_json() + ")";
}

dom_element_wrapper dom_field_wrapper::element() const {
  return wrap_element(owner, value);
}

std::string dom_field_wrapper::repr() const {
  return "DOMField(key=" + key + ", value=" + element().to_json() + ")";
}

dom_document_wrapper dom_parser_wrapper::parse(nb::handle handle) {
  auto document = dom_document_wrapper();
  return parse_into(document, handle);
}

dom_document_wrapper dom_parser_wrapper::parse_into(dom_document_wrapper &document,
                                                    nb::handle handle) {
  nb::ft_lock_guard guard(mutex);
  nb::ft_lock_guard document_guard(document.state->mutex);
  if (nb::isinstance<padded_string_wrapper>(handle)) {
    const auto &padded = nb::cast<const padded_string_wrapper &>(handle);
    auto result = with_gil_released([&] {
      return parser.parse_into_document(document.state->document, padded.value());
    });
    ensure_success(result.error(), "DOM parse");
    return document;
  }

  borrowed_bytes bytes(handle);
  auto result = with_gil_released([&] {
    return parser.parse_into_document(
        document.state->document,
        reinterpret_cast<const uint8_t *>(bytes.view().data()), bytes.view().size(),
        true);
  });
  ensure_success(result.error(), "DOM parse");
  return document;
}

dom_document_wrapper dom_parser_wrapper::load(std::string_view path) {
  auto document = dom_document_wrapper();
  return load_into(document, path);
}

dom_document_wrapper dom_parser_wrapper::load_into(dom_document_wrapper &document,
                                                   std::string_view path) {
  nb::ft_lock_guard guard(mutex);
  nb::ft_lock_guard document_guard(document.state->mutex);
  auto result = with_gil_released(
      [&] { return parser.load_into_document(document.state->document, path); });
  ensure_success(result.error(), "DOM load");
  return document;
}

size_t dom_parser_wrapper::capacity() const noexcept {
  nb::ft_lock_guard guard(mutex);
  return parser.capacity();
}

size_t dom_parser_wrapper::max_capacity() const noexcept {
  nb::ft_lock_guard guard(mutex);
  return parser.max_capacity();
}

size_t dom_parser_wrapper::max_depth() const noexcept {
  nb::ft_lock_guard guard(mutex);
  return parser.max_depth();
}

void dom_parser_wrapper::set_max_capacity(size_t max_capacity) noexcept {
  nb::ft_lock_guard guard(mutex);
  parser.set_max_capacity(max_capacity);
}

void dom_parser_wrapper::allocate(size_t capacity, size_t max_depth) {
  nb::ft_lock_guard guard(mutex);
  ensure_success(parser.allocate(capacity, max_depth), "DOM parser allocate");
}

dom_element_wrapper dom_stream_element_wrapper::element_view() const {
  return wrap_element(state, element);
}

size_t dom_document_stream_wrapper::size_in_bytes() const noexcept {
  nb::ft_lock_guard guard(state->mutex);
  return state->stream.size_in_bytes();
}

size_t dom_document_stream_wrapper::truncated_bytes() const noexcept {
  nb::ft_lock_guard guard(state->mutex);
  return state->stream.truncated_bytes();
}

void bind_dom(nb::module_ &m) {
  nb::enum_<simdjson::dom::element_type>(m, "DOMElementType")
      .value("ARRAY", simdjson::dom::element_type::ARRAY)
      .value("OBJECT", simdjson::dom::element_type::OBJECT)
      .value("INT64", simdjson::dom::element_type::INT64)
      .value("UINT64", simdjson::dom::element_type::UINT64)
      .value("DOUBLE", simdjson::dom::element_type::DOUBLE)
      .value("STRING", simdjson::dom::element_type::STRING)
      .value("BOOL", simdjson::dom::element_type::BOOL)
      .value("NULL_VALUE", simdjson::dom::element_type::NULL_VALUE)
      .value("BIGINT", simdjson::dom::element_type::BIGINT);

  nb::class_<dom_array_iterator_wrapper>(m, "_DOMArrayIterator")
      .def("__iter__", [](dom_array_iterator_wrapper &self) -> dom_array_iterator_wrapper & {
        return self;
      })
      .def("__next__", &dom_array_iterator_wrapper::next);

  nb::class_<dom_object_key_iterator_wrapper>(m, "_DOMObjectKeyIterator")
      .def("__iter__",
           [](dom_object_key_iterator_wrapper &self)
               -> dom_object_key_iterator_wrapper & { return self; })
      .def("__next__", &dom_object_key_iterator_wrapper::next);

  nb::class_<dom_object_field_iterator_wrapper>(m, "_DOMObjectFieldIterator")
      .def("__iter__",
           [](dom_object_field_iterator_wrapper &self)
               -> dom_object_field_iterator_wrapper & { return self; })
      .def("__next__", &dom_object_field_iterator_wrapper::next);

  nb::class_<dom_document_stream_iterator_wrapper>(m, "_DOMDocumentStreamIterator")
      .def("__iter__",
           [](dom_document_stream_iterator_wrapper &self)
               -> dom_document_stream_iterator_wrapper & { return self; })
      .def("__next__", &dom_document_stream_iterator_wrapper::next);

  nb::class_<dom_field_wrapper>(m, "DOMField")
      .def_prop_ro("key", [](const dom_field_wrapper &self) { return self.key; })
      .def_prop_ro("value", &dom_field_wrapper::element)
      .def("__iter__", [](const dom_field_wrapper &self) {
        return nb::make_tuple(self.key, self.element()).attr("__iter__")();
      })
      .def("__repr__", &dom_field_wrapper::repr);

  nb::class_<dom_array_wrapper>(
      m, "DOMArray", nb::sig("class DOMArray(collections.abc.Sequence[DOMElement])"))
      .def("__len__", &dom_array_wrapper::size)
      .def("size", &dom_array_wrapper::size)
      .def_prop_ro("number_of_slots", &dom_array_wrapper::number_of_slots)
      .def("__getitem__", &dom_array_wrapper::at, "index"_a)
      .def("at", &dom_array_wrapper::at, "index"_a)
      .def("at_pointer", &dom_array_wrapper::at_pointer, "json_pointer"_a)
      .def("at_path", &dom_array_wrapper::at_path, "json_path"_a)
      .def("at_path_with_wildcard", &dom_array_wrapper::at_path_with_wildcard,
           "json_path"_a)
      .def("__iter__",
           [](const dom_array_wrapper &self) { return dom_array_iterator_wrapper(self); })
      .def("to_json", &dom_array_wrapper::to_json)
      .def("prettify", &dom_array_wrapper::prettify)
      .def("__str__", &dom_array_wrapper::to_json)
      .def("__repr__", &dom_array_wrapper::repr);

  nb::class_<dom_object_wrapper>(
      m, "DOMObject",
      nb::sig("class DOMObject(collections.abc.Mapping[str, DOMElement])"))
      .def("__len__", &dom_object_wrapper::size)
      .def("size", &dom_object_wrapper::size)
      .def("__getitem__", &dom_object_wrapper::get_item, "key"_a)
      .def("get", [](const dom_object_wrapper &self, std::string_view key,
                     nb::object default_value) -> nb::object {
        if (!self.contains(key)) {
          return default_value;
        }
        return nb::cast(self.get_item(key));
      }, "key"_a, "default"_a = nb::none())
      .def("__contains__", &dom_object_wrapper::contains, "key"_a)
      .def("at_key", &dom_object_wrapper::at_key, "key"_a)
      .def("at_key_case_insensitive",
           &dom_object_wrapper::at_key_case_insensitive, "key"_a)
      .def("at_pointer", &dom_object_wrapper::at_pointer, "json_pointer"_a)
      .def("at_path", &dom_object_wrapper::at_path, "json_path"_a)
      .def("at_path_with_wildcard", &dom_object_wrapper::at_path_with_wildcard,
           "json_path"_a)
      .def("keys", &dom_object_wrapper::keys)
      .def("__iter__",
           [](const dom_object_wrapper &self) {
             return dom_object_key_iterator_wrapper(self);
           })
      .def("fields",
           [](const dom_object_wrapper &self) {
             return dom_object_field_iterator_wrapper(self);
           })
      .def("to_json", &dom_object_wrapper::to_json)
      .def("prettify", &dom_object_wrapper::prettify)
      .def("__str__", &dom_object_wrapper::to_json)
      .def("__repr__", &dom_object_wrapper::repr);

  nb::class_<dom_element_wrapper>(m, "DOMElement")
      .def_prop_ro("type", &dom_element_wrapper::type)
      .def("is_array", &dom_element_wrapper::is_array)
      .def("is_object", &dom_element_wrapper::is_object)
      .def("is_string", &dom_element_wrapper::is_string)
      .def("is_int64", &dom_element_wrapper::is_int64)
      .def("is_uint64", &dom_element_wrapper::is_uint64)
      .def("is_double", &dom_element_wrapper::is_double)
      .def("is_number", &dom_element_wrapper::is_number)
      .def("is_bool", &dom_element_wrapper::is_bool)
      .def("is_null", &dom_element_wrapper::is_null)
      .def("is_bigint", &dom_element_wrapper::is_bigint)
      .def("get_array", &dom_element_wrapper::get_array)
      .def("get_object", &dom_element_wrapper::get_object)
      .def("get_string", &dom_element_wrapper::get_string)
      .def("get_string_length", &dom_element_wrapper::get_string_length)
      .def("get_int64", &dom_element_wrapper::get_int64)
      .def("get_uint64", &dom_element_wrapper::get_uint64)
      .def("get_double", &dom_element_wrapper::get_double)
      .def("get_bool", &dom_element_wrapper::get_bool)
      .def("get_bigint", &dom_element_wrapper::get_bigint)
      .def("__len__", &dom_element_wrapper::size_hint)
      .def(
          "__getitem__",
          [](const dom_element_wrapper &self, nb::handle key) {
            if (PyLong_Check(key.ptr())) {
              return self.get_index(nb::cast<size_t>(key));
            }
            return self.get_item(nb::cast<std::string_view>(key));
          },
          "key"_a)
      .def("at_pointer", &dom_element_wrapper::at_pointer, "json_pointer"_a)
      .def("at_path", &dom_element_wrapper::at_path, "json_path"_a)
      .def("at_path_with_wildcard", &dom_element_wrapper::at_path_with_wildcard,
           "json_path"_a)
      .def("at_key", &dom_element_wrapper::at_key, "key"_a)
      .def("at_key_case_insensitive",
           &dom_element_wrapper::at_key_case_insensitive, "key"_a)
      .def("__iter__",
           [](const dom_element_wrapper &self) -> nb::object {
             if (self.is_array()) {
               return nb::cast(dom_array_iterator_wrapper(self.get_array()));
             }
             if (self.is_object()) {
               return nb::cast(dom_object_key_iterator_wrapper(self.get_object()));
             }
             throw nb::type_error("DOM element is not iterable");
           })
      .def("to_json", &dom_element_wrapper::to_json)
      .def("prettify", &dom_element_wrapper::prettify)
      .def("__str__", &dom_element_wrapper::to_json)
      .def("__repr__", &dom_element_wrapper::repr)
      .def("__eq__", &dom_element_wrapper::equals,
           nb::sig("def __eq__(self, other: object, /) -> bool"));

  nb::class_<dom_document_wrapper>(m, "DOMDocument")
      .def(nb::init<>())
      .def_prop_ro("root", &dom_document_wrapper::root)
      .def_prop_ro("capacity", &dom_document_wrapper::capacity)
      .def("dump_raw_tape", &dom_document_wrapper::dump_raw_tape)
      .def("__len__", [](const dom_document_wrapper &self) { return self.root().size_hint(); })
      .def("__getitem__",
           [](const dom_document_wrapper &self, nb::handle key) {
             if (PyLong_Check(key.ptr())) {
               return self.root().get_index(nb::cast<size_t>(key));
             }
             return self.root().get_item(nb::cast<std::string_view>(key));
           },
           "key"_a)
      .def("__iter__", [](const dom_document_wrapper &self) -> nb::object {
        return nb::cast(self.root()).attr("__iter__")();
      })
      .def("type", [](const dom_document_wrapper &self) { return self.root().type(); })
      .def("get_array", [](const dom_document_wrapper &self) { return self.root().get_array(); })
      .def("get_object", [](const dom_document_wrapper &self) { return self.root().get_object(); })
      .def("get_string", [](const dom_document_wrapper &self) { return self.root().get_string(); })
      .def("get_string_length",
           [](const dom_document_wrapper &self) { return self.root().get_string_length(); })
      .def("get_int64", [](const dom_document_wrapper &self) { return self.root().get_int64(); })
      .def("get_uint64", [](const dom_document_wrapper &self) { return self.root().get_uint64(); })
      .def("get_double", [](const dom_document_wrapper &self) { return self.root().get_double(); })
      .def("get_bool", [](const dom_document_wrapper &self) { return self.root().get_bool(); })
      .def("get_bigint", [](const dom_document_wrapper &self) { return self.root().get_bigint(); })
      .def("at_pointer",
           [](const dom_document_wrapper &self, std::string_view json_pointer) {
             return self.root().at_pointer(json_pointer);
           },
           "json_pointer"_a)
      .def("at_path",
           [](const dom_document_wrapper &self, std::string_view json_path) {
             return self.root().at_path(json_path);
           },
           "json_path"_a)
      .def("at_path_with_wildcard",
           [](const dom_document_wrapper &self, std::string_view json_path) {
             return self.root().at_path_with_wildcard(json_path);
           },
           "json_path"_a)
      .def("at_key",
           [](const dom_document_wrapper &self, std::string_view key) {
             return self.root().at_key(key);
           },
           "key"_a)
      .def("at_key_case_insensitive",
           [](const dom_document_wrapper &self, std::string_view key) {
             return self.root().at_key_case_insensitive(key);
           },
           "key"_a)
      .def("to_json", &dom_document_wrapper::to_json)
      .def("prettify", &dom_document_wrapper::prettify)
      .def("__str__", &dom_document_wrapper::to_json)
      .def("__repr__", &dom_document_wrapper::repr);

  nb::class_<dom_document_stream_wrapper>(m, "DOMDocumentStream")
      .def_prop_ro("size_in_bytes", &dom_document_stream_wrapper::size_in_bytes)
      .def_prop_ro("truncated_bytes",
                   &dom_document_stream_wrapper::truncated_bytes)
      .def("__iter__",
           [](const dom_document_stream_wrapper &self) {
             return dom_document_stream_iterator_wrapper(self.state);
           });

  nb::class_<dom_parser_wrapper>(m, "DOMParser")
      .def(nb::init<size_t>(),
           "max_capacity"_a = simdjson::SIMDJSON_MAXSIZE_BYTES)
      .def("parse", &dom_parser_wrapper::parse, "data"_a)
      .def("parse_into", &dom_parser_wrapper::parse_into, "document"_a, "data"_a)
      .def("load", &dom_parser_wrapper::load, "path"_a)
      .def("load_into", &dom_parser_wrapper::load_into, "document"_a, "path"_a)
      .def(
          "parse_many",
          [](const dom_parser_wrapper &self, nb::handle handle,
             size_t batch_size) {
            auto state = std::make_shared<dom_stream_state>();
            state->parser = make_dom_parser_from_template(self);
            state->input = padded_storage_from_python(handle);
            auto result = with_gil_released([&] {
              return state->parser.parse_many(state->input->value, batch_size);
            });
            simdjson::dom::document_stream stream;
            ensure_success(std::move(result).get(stream), "DOM parse_many");
            state->stream = std::move(stream);
            state->generation = 1;
            return dom_document_stream_wrapper(state);
          },
          "data"_a, "batch_size"_a = simdjson::dom::DEFAULT_BATCH_SIZE)
      .def(
          "load_many",
          [](const dom_parser_wrapper &self, std::string_view path,
             size_t batch_size) {
            auto state = std::make_shared<dom_stream_state>();
            state->parser = make_dom_parser_from_template(self);
            state->input = padded_string_wrapper::load(path).storage();
            auto result = with_gil_released([&] {
              return state->parser.parse_many(state->input->value, batch_size);
            });
            simdjson::dom::document_stream stream;
            ensure_success(std::move(result).get(stream), "DOM load_many");
            state->stream = std::move(stream);
            state->generation = 1;
            return dom_document_stream_wrapper(state);
          },
          "path"_a, "batch_size"_a = simdjson::dom::DEFAULT_BATCH_SIZE)
      .def_prop_ro("capacity", &dom_parser_wrapper::capacity)
      .def_prop_ro("max_capacity", &dom_parser_wrapper::max_capacity)
      .def_prop_ro("max_depth", &dom_parser_wrapper::max_depth)
      .def("set_max_capacity", &dom_parser_wrapper::set_max_capacity,
           "max_capacity"_a)
      .def("allocate", &dom_parser_wrapper::allocate, "capacity"_a,
           "max_depth"_a = simdjson::DEFAULT_MAX_DEPTH);
}

}  // namespace simdjsonpy
