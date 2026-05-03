#include "ondemand.h"

namespace simdjsonpy {

namespace {

struct ondemand_parser_config {
  size_t max_capacity;
  size_t allocated_capacity;
  size_t max_depth;
  bool threaded;
};

ondemand_parser_config read_ondemand_parser_config(
    const ondemand_parser_wrapper &wrapper) {
  nb::ft_lock_guard guard(wrapper.mutex);
  return ondemand_parser_config{wrapper.max_capacity_, wrapper.allocated_capacity_,
                                wrapper.max_depth_, wrapper.threaded_};
}

void configure_ondemand_parser(simdjson::ondemand::parser &parser,
                               const ondemand_parser_config &config,
                               std::string_view context) {
  parser.threaded = config.threaded;
  if (config.allocated_capacity != 0) {
    ensure_success(parser.allocate(config.allocated_capacity, config.max_depth),
                   context);
  }
}

const char *input_start(const ondemand_owner &owner) {
  if (const auto *document =
          std::get_if<std::shared_ptr<ondemand_document_state>>(&owner)) {
    return (*document)->input->value.data();
  }
  return std::get<std::shared_ptr<ondemand_document_reference_state>>(owner)
      ->stream->input->value.data();
}

simdjson::ondemand::parser &parser_from_owner(const ondemand_owner &owner) {
  if (const auto *document =
          std::get_if<std::shared_ptr<ondemand_document_state>>(&owner)) {
    return (*document)->parser;
  }
  return std::get<std::shared_ptr<ondemand_document_reference_state>>(owner)
      ->stream->parser;
}

size_t raw_json_string_length(simdjson::ondemand::raw_json_string value) {
  const char *cursor = value.raw();
  bool escaped = false;
  size_t count = 0;
  while (cursor[count] != '\0') {
    const char ch = cursor[count];
    if (!escaped && ch == '"') {
      return count;
    }
    escaped = !escaped && ch == '\\';
    ++count;
  }
  return count;
}

ondemand_value_wrapper wrap_value(ondemand_owner owner,
                                  simdjson::ondemand::value value) {
  return ondemand_value_wrapper(std::move(owner), value);
}

ondemand_array_wrapper wrap_array(ondemand_owner owner,
                                  simdjson::ondemand::array array) {
  return ondemand_array_wrapper(std::move(owner), array);
}

ondemand_object_wrapper wrap_object(ondemand_owner owner,
                                    simdjson::ondemand::object object) {
  return ondemand_object_wrapper(std::move(owner), object);
}

ondemand_field_wrapper wrap_field(ondemand_owner owner,
                                  simdjson::ondemand::field field) {
  return ondemand_field_wrapper(std::move(owner), std::move(field));
}

ondemand_raw_json_string_wrapper wrap_raw_json_string(
    ondemand_owner owner, simdjson::ondemand::raw_json_string value) {
  return ondemand_raw_json_string_wrapper(std::move(owner), value);
}

size_t current_offset_from_pointer(const ondemand_owner &owner,
                                   const char *pointer) {
  return static_cast<size_t>(pointer - input_start(owner));
}

struct ondemand_array_iterator_wrapper {
  ondemand_array_wrapper array;
  simdjson::ondemand::array_iterator current{};
  simdjson::ondemand::array_iterator end{};

  explicit ondemand_array_iterator_wrapper(ondemand_array_wrapper array_)
      : array(std::move(array_)) {
    with_ondemand_owner(array.owner, [&] {
      auto reset = array.array.reset();
      ensure_success(reset.error(), "On-Demand array reset");
      auto begin = array.array.begin();
      auto finish = array.array.end();
      ensure_success(begin.error(), "On-Demand array begin");
      ensure_success(finish.error(), "On-Demand array end");
      current = begin.value_unsafe();
      end = finish.value_unsafe();
      return 0;
    });
  }

  ondemand_value_wrapper next() {
    return with_ondemand_owner(array.owner, [&] {
      if (!(current != end)) {
        throw nb::stop_iteration();
      }
      auto result = *current;
      ensure_success(result.error(), "On-Demand array iteration");
      auto value = result.value_unsafe();
      ++current;
      return wrap_value(array.owner, value);
    });
  }
};

struct ondemand_object_key_iterator_wrapper {
  ondemand_object_wrapper object;
  simdjson::ondemand::object_iterator current{};
  simdjson::ondemand::object_iterator end{};

  explicit ondemand_object_key_iterator_wrapper(ondemand_object_wrapper object_)
      : object(std::move(object_)) {
    with_ondemand_owner(object.owner, [&] {
      auto reset = object.object.reset();
      ensure_success(reset.error(), "On-Demand object reset");
      auto begin = object.object.begin();
      auto finish = object.object.end();
      ensure_success(begin.error(), "On-Demand object begin");
      ensure_success(finish.error(), "On-Demand object end");
      current = begin.value_unsafe();
      end = finish.value_unsafe();
      return 0;
    });
  }

  std::string next() {
    return with_ondemand_owner(object.owner, [&] {
      if (!(current != end)) {
        throw nb::stop_iteration();
      }
      auto result = *current;
      ensure_success(result.error(), "On-Demand object iteration");
      auto field = result.value_unsafe();
      auto key_result = field.unescaped_key();
      ensure_success(key_result.error(), "On-Demand object key");
      ++current;
      return std::string(key_result.value_unsafe());
    });
  }
};

struct ondemand_object_field_iterator_wrapper {
  ondemand_object_wrapper object;
  simdjson::ondemand::object_iterator current{};
  simdjson::ondemand::object_iterator end{};

  explicit ondemand_object_field_iterator_wrapper(
      ondemand_object_wrapper object_)
      : object(std::move(object_)) {
    with_ondemand_owner(object.owner, [&] {
      auto reset = object.object.reset();
      ensure_success(reset.error(), "On-Demand object reset");
      auto begin = object.object.begin();
      auto finish = object.object.end();
      ensure_success(begin.error(), "On-Demand object begin");
      ensure_success(finish.error(), "On-Demand object end");
      current = begin.value_unsafe();
      end = finish.value_unsafe();
      return 0;
    });
  }

  ondemand_field_wrapper next() {
    return with_ondemand_owner(object.owner, [&] {
      if (!(current != end)) {
        throw nb::stop_iteration();
      }
      auto result = *current;
      ensure_success(result.error(), "On-Demand object iteration");
      auto field = result.value_unsafe();
      ++current;
      return wrap_field(object.owner, std::move(field));
    });
  }
};

struct ondemand_document_stream_iterator_wrapper {
  std::shared_ptr<ondemand_stream_state> state;
  simdjson::ondemand::document_stream::iterator current{};
  simdjson::ondemand::document_stream::iterator end{};
  bool started{false};

  explicit ondemand_document_stream_iterator_wrapper(
      std::shared_ptr<ondemand_stream_state> state_)
      : state(std::move(state_)) {}

  ondemand_document_reference_wrapper next() {
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
    ensure_success(result.error(), "On-Demand document stream item");
    ++state->item_revision;
    auto reference_state = std::make_shared<ondemand_document_reference_state>();
    reference_state->stream = state;
    reference_state->generation = state->generation;
    reference_state->item_revision = state->item_revision;
    reference_state->document = result.value_unsafe();
    return ondemand_document_reference_wrapper(reference_state);
  }
};

}  // namespace

simdjson::ondemand::number_type ondemand_number_wrapper::type() const noexcept {
  return number.get_number_type();
}

bool ondemand_number_wrapper::is_uint64() const noexcept {
  return number.is_uint64();
}

uint64_t ondemand_number_wrapper::get_uint64() const noexcept {
  return number.get_uint64();
}

bool ondemand_number_wrapper::is_int64() const noexcept {
  return number.is_int64();
}

int64_t ondemand_number_wrapper::get_int64() const noexcept {
  return number.get_int64();
}

bool ondemand_number_wrapper::is_double() const noexcept {
  return number.is_double();
}

double ondemand_number_wrapper::get_double() const noexcept {
  return number.get_double();
}

double ondemand_number_wrapper::as_double() const noexcept {
  return number.as_double();
}

std::string ondemand_number_wrapper::repr() const {
  if (is_uint64()) {
    return "OnDemandNumber(" + std::to_string(get_uint64()) + ")";
  }
  if (is_int64()) {
    return "OnDemandNumber(" + std::to_string(get_int64()) + ")";
  }
  return "OnDemandNumber(" + std::to_string(get_double()) + ")";
}

std::string ondemand_raw_json_string_wrapper::raw() const {
  return with_ondemand_owner(owner, [&] {
    const size_t length = raw_json_string_length(value);
    return std::string(value.raw(), length);
  });
}

std::string ondemand_raw_json_string_wrapper::unescape(
    bool allow_replacement) const {
  return with_ondemand_owner(owner, [&] {
    std::vector<uint8_t> buffer(raw_json_string_length(value) +
                                simdjson::SIMDJSON_PADDING + 1);
    uint8_t *destination = buffer.data();
    auto result =
        parser_from_owner(owner).unescape(value, destination, allow_replacement);
    ensure_success(result.error(), "On-Demand string unescape");
    return std::string(result.value_unsafe());
  });
}

std::string ondemand_raw_json_string_wrapper::unescape_wobbly() const {
  return with_ondemand_owner(owner, [&] {
    std::vector<uint8_t> buffer(raw_json_string_length(value) +
                                simdjson::SIMDJSON_PADDING + 1);
    uint8_t *destination = buffer.data();
    auto result = parser_from_owner(owner).unescape_wobbly(value, destination);
    ensure_success(result.error(), "On-Demand string wobbly unescape");
    return std::string(result.value_unsafe());
  });
}

bool ondemand_raw_json_string_wrapper::is_equal(std::string_view other) const {
  return with_ondemand_owner(owner, [&] { return value.is_equal(other); });
}

std::string ondemand_raw_json_string_wrapper::repr() const {
  return "OnDemandRawJSONString(" + raw() + ")";
}

ondemand_array_wrapper ondemand_value_wrapper::get_array() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_array();
    ensure_success(result.error(), "On-Demand array");
    return wrap_array(owner, result.value_unsafe());
  });
}

ondemand_object_wrapper ondemand_value_wrapper::get_object() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_object();
    ensure_success(result.error(), "On-Demand object");
    return wrap_object(owner, result.value_unsafe());
  });
}

uint64_t ondemand_value_wrapper::get_uint64() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_uint64();
    ensure_success(result.error(), "On-Demand uint64");
    return result.value_unsafe();
  });
}

uint64_t ondemand_value_wrapper::get_uint64_in_string() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_uint64_in_string();
    ensure_success(result.error(), "On-Demand uint64 in string");
    return result.value_unsafe();
  });
}

int64_t ondemand_value_wrapper::get_int64() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_int64();
    ensure_success(result.error(), "On-Demand int64");
    return result.value_unsafe();
  });
}

int64_t ondemand_value_wrapper::get_int64_in_string() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_int64_in_string();
    ensure_success(result.error(), "On-Demand int64 in string");
    return result.value_unsafe();
  });
}

uint32_t ondemand_value_wrapper::get_uint32() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_uint32();
    ensure_success(result.error(), "On-Demand uint32");
    return result.value_unsafe();
  });
}

int32_t ondemand_value_wrapper::get_int32() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_int32();
    ensure_success(result.error(), "On-Demand int32");
    return result.value_unsafe();
  });
}

double ondemand_value_wrapper::get_double() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_double();
    ensure_success(result.error(), "On-Demand double");
    return result.value_unsafe();
  });
}

double ondemand_value_wrapper::get_double_in_string() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_double_in_string();
    ensure_success(result.error(), "On-Demand double in string");
    return result.value_unsafe();
  });
}

std::string ondemand_value_wrapper::get_string(bool allow_replacement) const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_string(allow_replacement);
    ensure_success(result.error(), "On-Demand string");
    return std::string(result.value_unsafe());
  });
}

std::string ondemand_value_wrapper::get_wobbly_string() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_wobbly_string();
    ensure_success(result.error(), "On-Demand wobbly string");
    return std::string(result.value_unsafe());
  });
}

ondemand_raw_json_string_wrapper ondemand_value_wrapper::get_raw_json_string()
    const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_raw_json_string();
    ensure_success(result.error(), "On-Demand raw JSON string");
    return wrap_raw_json_string(owner, result.value_unsafe());
  });
}

bool ondemand_value_wrapper::get_bool() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_bool();
    ensure_success(result.error(), "On-Demand bool");
    return result.value_unsafe();
  });
}

bool ondemand_value_wrapper::is_null() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.is_null();
    ensure_success(result.error(), "On-Demand null check");
    return result.value_unsafe();
  });
}

size_t ondemand_value_wrapper::count_elements() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.count_elements();
    ensure_success(result.error(), "On-Demand count elements");
    return result.value_unsafe();
  });
}

size_t ondemand_value_wrapper::count_fields() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.count_fields();
    ensure_success(result.error(), "On-Demand count fields");
    return result.value_unsafe();
  });
}

ondemand_value_wrapper ondemand_value_wrapper::at(size_t index) const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.at(index);
    ensure_success(result.error(), "On-Demand array lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_value_wrapper::find_field(
    std::string_view key) const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.find_field(key);
    ensure_success(result.error(), "On-Demand field lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_value_wrapper::find_field_unordered(
    std::string_view key) const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.find_field_unordered(key);
    ensure_success(result.error(), "On-Demand unordered field lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_value_wrapper::at_pointer(
    std::string_view json_pointer) const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.at_pointer(json_pointer);
    ensure_success(result.error(), "On-Demand JSON pointer");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_value_wrapper::at_path(
    std::string_view json_path) const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.at_path(json_path);
    ensure_success(result.error(), "On-Demand JSONPath");
    return wrap_value(owner, result.value_unsafe());
  });
}

simdjson::ondemand::json_type ondemand_value_wrapper::type() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.type();
    ensure_success(result.error(), "On-Demand type");
    return result.value_unsafe();
  });
}

bool ondemand_value_wrapper::is_scalar() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.is_scalar();
    ensure_success(result.error(), "On-Demand scalar check");
    return result.value_unsafe();
  });
}

bool ondemand_value_wrapper::is_string() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.is_string();
    ensure_success(result.error(), "On-Demand string check");
    return result.value_unsafe();
  });
}

bool ondemand_value_wrapper::is_negative() const {
  return with_ondemand_owner(owner, [&] { return value.is_negative(); });
}

bool ondemand_value_wrapper::is_integer() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.is_integer();
    ensure_success(result.error(), "On-Demand integer check");
    return result.value_unsafe();
  });
}

simdjson::ondemand::number_type ondemand_value_wrapper::get_number_type() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_number_type();
    ensure_success(result.error(), "On-Demand number type");
    return result.value_unsafe();
  });
}

ondemand_number_wrapper ondemand_value_wrapper::get_number() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.get_number();
    ensure_success(result.error(), "On-Demand number");
    return ondemand_number_wrapper(result.value_unsafe());
  });
}

std::string ondemand_value_wrapper::raw_json_token() const {
  return with_ondemand_owner(owner, [&] {
    return std::string(value.raw_json_token());
  });
}

std::string ondemand_value_wrapper::raw_json() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.raw_json();
    ensure_success(result.error(), "On-Demand raw JSON");
    return std::string(result.value_unsafe());
  });
}

size_t ondemand_value_wrapper::current_depth() const {
  return with_ondemand_owner(owner, [&] {
    return static_cast<size_t>(value.current_depth());
  });
}

size_t ondemand_value_wrapper::current_location() const {
  return with_ondemand_owner(owner, [&] {
    auto result = value.current_location();
    ensure_success(result.error(), "On-Demand current location");
    return current_offset_from_pointer(owner, result.value_unsafe());
  });
}

std::string ondemand_value_wrapper::to_json() const { return raw_json(); }

std::string ondemand_value_wrapper::repr() const {
  return "OnDemandValue(" + to_json() + ")";
}

size_t ondemand_array_wrapper::count_elements() const {
  return with_ondemand_owner(owner, [&] {
    auto result = array.count_elements();
    ensure_success(result.error(), "On-Demand count array elements");
    return result.value_unsafe();
  });
}

bool ondemand_array_wrapper::is_empty() const {
  return with_ondemand_owner(owner, [&] {
    auto result = array.is_empty();
    ensure_success(result.error(), "On-Demand array empty check");
    return result.value_unsafe();
  });
}

bool ondemand_array_wrapper::reset() const {
  return with_ondemand_owner(owner, [&] {
    auto result = array.reset();
    ensure_success(result.error(), "On-Demand array reset");
    return result.value_unsafe();
  });
}

ondemand_value_wrapper ondemand_array_wrapper::at(size_t index) const {
  return with_ondemand_owner(owner, [&] {
    auto result = array.at(index);
    ensure_success(result.error(), "On-Demand array at");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_array_wrapper::at_pointer(
    std::string_view json_pointer) const {
  return with_ondemand_owner(owner, [&] {
    auto result = array.at_pointer(json_pointer);
    ensure_success(result.error(), "On-Demand array JSON pointer");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_array_wrapper::at_path(
    std::string_view json_path) const {
  return with_ondemand_owner(owner, [&] {
    auto result = array.at_path(json_path);
    ensure_success(result.error(), "On-Demand array JSONPath");
    return wrap_value(owner, result.value_unsafe());
  });
}

std::string ondemand_array_wrapper::raw_json() const {
  return with_ondemand_owner(owner, [&] {
    auto result = array.raw_json();
    ensure_success(result.error(), "On-Demand array raw JSON");
    return std::string(result.value_unsafe());
  });
}

std::string ondemand_array_wrapper::to_json() const { return raw_json(); }

std::string ondemand_array_wrapper::repr() const {
  return "OnDemandArray(" + to_json() + ")";
}

ondemand_value_wrapper ondemand_object_wrapper::find_field(
    std::string_view key) const {
  return with_ondemand_owner(owner, [&] {
    auto result = object.find_field(key);
    ensure_success(result.error(), "On-Demand object field lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_object_wrapper::find_field_unordered(
    std::string_view key) const {
  return with_ondemand_owner(owner, [&] {
    auto result = object.find_field_unordered(key);
    ensure_success(result.error(), "On-Demand object unordered field lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_object_wrapper::at_pointer(
    std::string_view json_pointer) const {
  return with_ondemand_owner(owner, [&] {
    auto result = object.at_pointer(json_pointer);
    ensure_success(result.error(), "On-Demand object JSON pointer");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_object_wrapper::at_path(
    std::string_view json_path) const {
  return with_ondemand_owner(owner, [&] {
    auto result = object.at_path(json_path);
    ensure_success(result.error(), "On-Demand object JSONPath");
    return wrap_value(owner, result.value_unsafe());
  });
}

bool ondemand_object_wrapper::reset() const {
  return with_ondemand_owner(owner, [&] {
    auto result = object.reset();
    ensure_success(result.error(), "On-Demand object reset");
    return result.value_unsafe();
  });
}

bool ondemand_object_wrapper::is_empty() const {
  return with_ondemand_owner(owner, [&] {
    auto result = object.is_empty();
    ensure_success(result.error(), "On-Demand object empty check");
    return result.value_unsafe();
  });
}

size_t ondemand_object_wrapper::count_fields() const {
  return with_ondemand_owner(owner, [&] {
    auto result = object.count_fields();
    ensure_success(result.error(), "On-Demand object count fields");
    return result.value_unsafe();
  });
}

std::string ondemand_object_wrapper::raw_json() const {
  return with_ondemand_owner(owner, [&] {
    auto result = object.raw_json();
    ensure_success(result.error(), "On-Demand object raw JSON");
    return std::string(result.value_unsafe());
  });
}

bool ondemand_object_wrapper::contains(std::string_view key) const {
  return with_ondemand_owner(owner, [&] {
    auto reset_result = object.reset();
    ensure_success(reset_result.error(), "On-Demand object contains reset");
    auto result = object.find_field_unordered(key);
    if (result.error() == simdjson::NO_SUCH_FIELD) {
      return false;
    }
    ensure_success(result.error(), "On-Demand object contains");
    return true;
  });
}

std::vector<std::string> ondemand_object_wrapper::keys() const {
  return with_ondemand_owner(owner, [&] {
    std::vector<std::string> keys;
    auto reset_result = object.reset();
    ensure_success(reset_result.error(), "On-Demand object keys reset");
    auto begin = object.begin();
    auto finish = object.end();
    ensure_success(begin.error(), "On-Demand object keys begin");
    ensure_success(finish.error(), "On-Demand object keys end");
    auto current = begin.value_unsafe();
    auto end = finish.value_unsafe();
    while (current != end) {
      auto field_result = *current;
      ensure_success(field_result.error(), "On-Demand object keys");
      auto key_result = field_result.value_unsafe().unescaped_key();
      ensure_success(key_result.error(), "On-Demand object key");
      keys.emplace_back(key_result.value_unsafe());
      ++current;
    }
    return keys;
  });
}

std::string ondemand_object_wrapper::to_json() const { return raw_json(); }

std::string ondemand_object_wrapper::repr() const {
  return "OnDemandObject(" + to_json() + ")";
}

std::string ondemand_field_wrapper::unescaped_key(bool allow_replacement) const {
  return with_ondemand_owner(owner, [&] {
    auto result = field.unescaped_key(allow_replacement);
    ensure_success(result.error(), "On-Demand field key");
    return std::string(result.value_unsafe());
  });
}

ondemand_raw_json_string_wrapper ondemand_field_wrapper::key() const {
  return with_ondemand_owner(owner, [&] {
    return wrap_raw_json_string(owner, field.key());
  });
}

std::string ondemand_field_wrapper::key_raw_json_token() const {
  return with_ondemand_owner(owner, [&] {
    return std::string(field.key_raw_json_token());
  });
}

std::string ondemand_field_wrapper::escaped_key() const {
  return with_ondemand_owner(owner, [&] {
    return std::string(field.escaped_key());
  });
}

ondemand_value_wrapper ondemand_field_wrapper::value() const {
  return with_ondemand_owner(owner, [&] {
    return wrap_value(owner, field.value());
  });
}

std::string ondemand_field_wrapper::repr() const {
  return "OnDemandField(key=" + escaped_key() + ")";
}

ondemand_array_wrapper ondemand_document_wrapper::get_array() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_array();
    ensure_success(result.error(), "On-Demand document array");
    return wrap_array(owner, result.value_unsafe());
  });
}

ondemand_object_wrapper ondemand_document_wrapper::get_object() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_object();
    ensure_success(result.error(), "On-Demand document object");
    return wrap_object(owner, result.value_unsafe());
  });
}

ondemand_array_wrapper ondemand_document_reference_wrapper::get_array() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_array();
    ensure_success(result.error(), "On-Demand document reference array");
    return wrap_array(owner, result.value_unsafe());
  });
}

ondemand_object_wrapper ondemand_document_reference_wrapper::get_object() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_object();
    ensure_success(result.error(), "On-Demand document reference object");
    return wrap_object(owner, result.value_unsafe());
  });
}

#define SIMDJSONPY_BIND_DOCUMENT_SCALAR(method_name, result_type, context) \
  result_type ondemand_document_wrapper::method_name() const {             \
    ondemand_owner owner = state;                                          \
    return with_ondemand_owner(owner, [&] {                                \
      auto result = state->document.method_name();                         \
      ensure_success(result.error(), context);                             \
      return result.value_unsafe();                                        \
    });                                                                    \
  }                                                                        \
  result_type ondemand_document_reference_wrapper::method_name() const {   \
    ondemand_owner owner = state;                                          \
    return with_ondemand_owner(owner, [&] {                                \
      auto result = state->document.method_name();                         \
      ensure_success(result.error(), context);                             \
      return result.value_unsafe();                                        \
    });                                                                    \
  }

SIMDJSONPY_BIND_DOCUMENT_SCALAR(get_uint64, uint64_t, "On-Demand document uint64")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(get_uint64_in_string, uint64_t,
                                "On-Demand document uint64 in string")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(get_int64, int64_t, "On-Demand document int64")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(get_int64_in_string, int64_t,
                                "On-Demand document int64 in string")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(get_uint32, uint32_t, "On-Demand document uint32")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(get_int32, int32_t, "On-Demand document int32")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(get_double, double, "On-Demand document double")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(get_double_in_string, double,
                                "On-Demand document double in string")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(get_bool, bool, "On-Demand document bool")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(is_null, bool, "On-Demand document null check")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(count_elements, size_t,
                                "On-Demand document count elements")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(count_fields, size_t,
                                "On-Demand document count fields")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(type, simdjson::ondemand::json_type,
                                "On-Demand document type")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(is_scalar, bool,
                                "On-Demand document scalar check")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(is_string, bool,
                                "On-Demand document string check")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(is_integer, bool,
                                "On-Demand document integer check")
SIMDJSONPY_BIND_DOCUMENT_SCALAR(
    get_number_type, simdjson::ondemand::number_type,
    "On-Demand document number type")

#undef SIMDJSONPY_BIND_DOCUMENT_SCALAR

bool ondemand_document_wrapper::is_negative() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] { return state->document.is_negative(); });
}

bool ondemand_document_reference_wrapper::is_negative() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] { return state->document.is_negative(); });
}

std::string ondemand_document_wrapper::get_string(bool allow_replacement) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_string(allow_replacement);
    ensure_success(result.error(), "On-Demand document string");
    return std::string(result.value_unsafe());
  });
}

std::string ondemand_document_reference_wrapper::get_string(
    bool allow_replacement) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_string(allow_replacement);
    ensure_success(result.error(), "On-Demand document reference string");
    return std::string(result.value_unsafe());
  });
}

std::string ondemand_document_wrapper::get_wobbly_string() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_wobbly_string();
    ensure_success(result.error(), "On-Demand document wobbly string");
    return std::string(result.value_unsafe());
  });
}

std::string ondemand_document_reference_wrapper::get_wobbly_string() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_wobbly_string();
    ensure_success(result.error(), "On-Demand document reference wobbly string");
    return std::string(result.value_unsafe());
  });
}

ondemand_raw_json_string_wrapper ondemand_document_wrapper::get_raw_json_string()
    const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_raw_json_string();
    ensure_success(result.error(), "On-Demand document raw JSON string");
    return wrap_raw_json_string(owner, result.value_unsafe());
  });
}

ondemand_raw_json_string_wrapper
ondemand_document_reference_wrapper::get_raw_json_string() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_raw_json_string();
    ensure_success(result.error(),
                   "On-Demand document reference raw JSON string");
    return wrap_raw_json_string(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_wrapper::get_value() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_value();
    ensure_success(result.error(), "On-Demand document value");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_reference_wrapper::get_value() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_value();
    ensure_success(result.error(), "On-Demand document reference value");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_wrapper::at(size_t index) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.at(index);
    ensure_success(result.error(), "On-Demand document array lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_reference_wrapper::at(size_t index) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.at(index);
    ensure_success(result.error(), "On-Demand document reference array lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_wrapper::find_field(
    std::string_view key) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.find_field(key);
    ensure_success(result.error(), "On-Demand document field lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_reference_wrapper::find_field(
    std::string_view key) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.find_field(key);
    ensure_success(result.error(), "On-Demand document reference field lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_wrapper::find_field_unordered(
    std::string_view key) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.find_field_unordered(key);
    ensure_success(result.error(), "On-Demand document unordered field lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_reference_wrapper::find_field_unordered(
    std::string_view key) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.find_field_unordered(key);
    ensure_success(result.error(),
                   "On-Demand document reference unordered field lookup");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_wrapper::at_pointer(
    std::string_view json_pointer) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.at_pointer(json_pointer);
    ensure_success(result.error(), "On-Demand document JSON pointer");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_reference_wrapper::at_pointer(
    std::string_view json_pointer) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.at_pointer(json_pointer);
    ensure_success(result.error(), "On-Demand document reference JSON pointer");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_wrapper::at_path(
    std::string_view json_path) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.at_path(json_path);
    ensure_success(result.error(), "On-Demand document JSONPath");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_value_wrapper ondemand_document_reference_wrapper::at_path(
    std::string_view json_path) const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.at_path(json_path);
    ensure_success(result.error(), "On-Demand document reference JSONPath");
    return wrap_value(owner, result.value_unsafe());
  });
}

ondemand_number_wrapper ondemand_document_wrapper::get_number() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_number();
    ensure_success(result.error(), "On-Demand document number");
    return ondemand_number_wrapper(result.value_unsafe());
  });
}

ondemand_number_wrapper ondemand_document_reference_wrapper::get_number() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.get_number();
    ensure_success(result.error(), "On-Demand document reference number");
    return ondemand_number_wrapper(result.value_unsafe());
  });
}

std::string ondemand_document_wrapper::raw_json_token() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.raw_json_token();
    ensure_success(result.error(), "On-Demand document raw token");
    return std::string(result.value_unsafe());
  });
}

std::string ondemand_document_reference_wrapper::raw_json_token() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.raw_json_token();
    ensure_success(result.error(), "On-Demand document reference raw token");
    return std::string(result.value_unsafe());
  });
}

std::string ondemand_document_wrapper::raw_json() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.raw_json();
    ensure_success(result.error(), "On-Demand document raw JSON");
    return std::string(result.value_unsafe());
  });
}

std::string ondemand_document_reference_wrapper::raw_json() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.raw_json();
    ensure_success(result.error(), "On-Demand document reference raw JSON");
    return std::string(result.value_unsafe());
  });
}

void ondemand_document_wrapper::rewind() const {
  ondemand_owner owner = state;
  with_ondemand_owner(owner, [&] {
    state->document.rewind();
    return 0;
  });
}

void ondemand_document_reference_wrapper::rewind() const {
  ondemand_owner owner = state;
  with_ondemand_owner(owner, [&] {
    state->document.rewind();
    return 0;
  });
}

bool ondemand_document_wrapper::is_alive() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] { return state->document.is_alive(); });
}

bool ondemand_document_wrapper::at_end() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] { return state->document.at_end(); });
}

size_t ondemand_document_wrapper::current_depth() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner,
                             [&] { return static_cast<size_t>(state->document.current_depth()); });
}

size_t ondemand_document_reference_wrapper::current_depth() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    return static_cast<size_t>(state->document.current_depth());
  });
}

size_t ondemand_document_wrapper::current_location() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.current_location();
    ensure_success(result.error(), "On-Demand document current location");
    return current_offset_from_pointer(owner, result.value_unsafe());
  });
}

size_t ondemand_document_reference_wrapper::current_location() const {
  ondemand_owner owner = state;
  return with_ondemand_owner(owner, [&] {
    auto result = state->document.current_location();
    ensure_success(result.error(),
                   "On-Demand document reference current location");
    return current_offset_from_pointer(owner, result.value_unsafe());
  });
}

std::string ondemand_document_wrapper::to_json() const { return raw_json(); }

std::string ondemand_document_reference_wrapper::to_json() const {
  return raw_json();
}

std::string ondemand_document_wrapper::repr() const {
  return "OnDemandDocument(" + to_json() + ")";
}

std::string ondemand_document_reference_wrapper::repr() const {
  return "OnDemandDocumentReference(" + to_json() + ")";
}

void ondemand_parser_wrapper::allocate(size_t capacity, size_t max_depth) {
  nb::ft_lock_guard guard(mutex);
  simdjson::ondemand::parser parser(max_capacity_);
  parser.threaded = threaded_;
  ensure_success(parser.allocate(capacity, max_depth), "On-Demand allocate");
  allocated_capacity_ = capacity;
  max_depth_ = max_depth;
}

ondemand_document_wrapper ondemand_parser_wrapper::iterate(nb::handle handle) {
  auto input = padded_storage_from_python(handle);
  const auto config = read_ondemand_parser_config(*this);
  auto state = std::make_shared<ondemand_document_state>(config.max_capacity);
  configure_ondemand_parser(state->parser, config,
                            "On-Demand parser clone allocate");
  state->input = std::move(input);
  auto result =
      with_gil_released([&] { return state->parser.iterate(state->input->value); });
  ensure_success(std::move(result).get(state->document), "On-Demand iterate");
  return ondemand_document_wrapper(state);
}

size_t ondemand_parser_wrapper::capacity() const noexcept {
  nb::ft_lock_guard guard(mutex);
  return allocated_capacity_;
}

size_t ondemand_parser_wrapper::max_capacity() const noexcept {
  nb::ft_lock_guard guard(mutex);
  return max_capacity_;
}

void ondemand_parser_wrapper::set_max_capacity(size_t max_capacity) noexcept {
  nb::ft_lock_guard guard(mutex);
  max_capacity_ = max_capacity;
}

size_t ondemand_parser_wrapper::max_depth() const noexcept {
  nb::ft_lock_guard guard(mutex);
  return max_depth_;
}

bool ondemand_parser_wrapper::threaded() const noexcept {
  nb::ft_lock_guard guard(mutex);
  return threaded_;
}

void ondemand_parser_wrapper::set_threaded(bool threaded) noexcept {
  nb::ft_lock_guard guard(mutex);
  threaded_ = threaded;
}

size_t ondemand_document_stream_wrapper::size_in_bytes() const noexcept {
  nb::ft_lock_guard guard(state->mutex);
  return state->stream.size_in_bytes();
}

size_t ondemand_document_stream_wrapper::truncated_bytes() const noexcept {
  nb::ft_lock_guard guard(state->mutex);
  return state->stream.truncated_bytes();
}

void bind_ondemand(nb::module_ &m) {
  nb::enum_<simdjson::ondemand::json_type>(m, "OnDemandJSONType")
      .value("UNKNOWN", simdjson::ondemand::json_type::unknown)
      .value("ARRAY", simdjson::ondemand::json_type::array)
      .value("OBJECT", simdjson::ondemand::json_type::object)
      .value("NUMBER", simdjson::ondemand::json_type::number)
      .value("STRING", simdjson::ondemand::json_type::string)
      .value("BOOLEAN", simdjson::ondemand::json_type::boolean)
      .value("NULL", simdjson::ondemand::json_type::null);

  nb::enum_<simdjson::ondemand::number_type>(m, "OnDemandNumberType")
      .value("FLOATING_POINT", simdjson::ondemand::number_type::floating_point_number)
      .value("SIGNED_INTEGER", simdjson::ondemand::number_type::signed_integer)
      .value("UNSIGNED_INTEGER",
             simdjson::ondemand::number_type::unsigned_integer)
      .value("BIG_INTEGER", simdjson::ondemand::number_type::big_integer);

  nb::class_<ondemand_array_iterator_wrapper>(m, "_OnDemandArrayIterator")
      .def("__iter__", [](ondemand_array_iterator_wrapper &self)
               -> ondemand_array_iterator_wrapper & { return self; })
      .def("__next__", &ondemand_array_iterator_wrapper::next);

  nb::class_<ondemand_object_key_iterator_wrapper>(m, "_OnDemandObjectKeyIterator")
      .def("__iter__", [](ondemand_object_key_iterator_wrapper &self)
               -> ondemand_object_key_iterator_wrapper & { return self; })
      .def("__next__", &ondemand_object_key_iterator_wrapper::next);

  nb::class_<ondemand_object_field_iterator_wrapper>(
      m, "_OnDemandObjectFieldIterator")
      .def("__iter__", [](ondemand_object_field_iterator_wrapper &self)
               -> ondemand_object_field_iterator_wrapper & { return self; })
      .def("__next__", &ondemand_object_field_iterator_wrapper::next);

  nb::class_<ondemand_document_stream_iterator_wrapper>(
      m, "_OnDemandDocumentStreamIterator")
      .def("__iter__", [](ondemand_document_stream_iterator_wrapper &self)
               -> ondemand_document_stream_iterator_wrapper & { return self; })
      .def("__next__", &ondemand_document_stream_iterator_wrapper::next);

  nb::class_<ondemand_number_wrapper>(m, "OnDemandNumber")
      .def_prop_ro("type", &ondemand_number_wrapper::type)
      .def("is_uint64", &ondemand_number_wrapper::is_uint64)
      .def("get_uint64", &ondemand_number_wrapper::get_uint64)
      .def("is_int64", &ondemand_number_wrapper::is_int64)
      .def("get_int64", &ondemand_number_wrapper::get_int64)
      .def("is_double", &ondemand_number_wrapper::is_double)
      .def("get_double", &ondemand_number_wrapper::get_double)
      .def("as_double", &ondemand_number_wrapper::as_double)
      .def("__repr__", &ondemand_number_wrapper::repr);

  nb::class_<ondemand_raw_json_string_wrapper>(m, "OnDemandRawJSONString")
      .def("raw", &ondemand_raw_json_string_wrapper::raw)
      .def("unescape", &ondemand_raw_json_string_wrapper::unescape,
           "allow_replacement"_a = false)
      .def("unescape_wobbly", &ondemand_raw_json_string_wrapper::unescape_wobbly)
      .def("is_equal", &ondemand_raw_json_string_wrapper::is_equal, "other"_a)
      .def("__str__", &ondemand_raw_json_string_wrapper::raw)
      .def("__repr__", &ondemand_raw_json_string_wrapper::repr)
      .def("__eq__",
           [](const ondemand_raw_json_string_wrapper &self,
              std::string_view other) { return self.is_equal(other); },
           nb::sig("def __eq__(self, other: object, /) -> bool"));

  nb::class_<ondemand_field_wrapper>(m, "OnDemandField")
      .def("unescaped_key", &ondemand_field_wrapper::unescaped_key,
           "allow_replacement"_a = false)
      .def("key", &ondemand_field_wrapper::key)
      .def("key_raw_json_token", &ondemand_field_wrapper::key_raw_json_token)
      .def("escaped_key", &ondemand_field_wrapper::escaped_key)
      .def_prop_ro("value", &ondemand_field_wrapper::value)
      .def("__repr__", &ondemand_field_wrapper::repr);

  nb::class_<ondemand_array_wrapper>(
      m, "OnDemandArray",
      nb::sig("class OnDemandArray(collections.abc.Sequence[OnDemandValue])"))
      .def("__len__", [](const ondemand_array_wrapper &self) {
        self.reset();
        return self.count_elements();
      })
      .def("count_elements", &ondemand_array_wrapper::count_elements)
      .def("is_empty", &ondemand_array_wrapper::is_empty)
      .def("reset", &ondemand_array_wrapper::reset)
      .def("__getitem__", [](const ondemand_array_wrapper &self, size_t index) {
        self.reset();
        return self.at(index);
      }, "index"_a)
      .def("at", &ondemand_array_wrapper::at, "index"_a)
      .def("at_pointer", &ondemand_array_wrapper::at_pointer, "json_pointer"_a)
      .def("at_path", &ondemand_array_wrapper::at_path, "json_path"_a)
      .def("__iter__",
           [](const ondemand_array_wrapper &self) {
             return ondemand_array_iterator_wrapper(self);
           })
      .def("raw_json", &ondemand_array_wrapper::raw_json)
      .def("to_json", &ondemand_array_wrapper::to_json)
      .def("__str__", &ondemand_array_wrapper::to_json)
      .def("__repr__", &ondemand_array_wrapper::repr);

  nb::class_<ondemand_object_wrapper>(
      m, "OnDemandObject",
      nb::sig("class OnDemandObject(collections.abc.Mapping[str, OnDemandValue])"))
      .def("__len__", [](const ondemand_object_wrapper &self) {
        self.reset();
        return self.count_fields();
      })
      .def("count_fields", &ondemand_object_wrapper::count_fields)
      .def("is_empty", &ondemand_object_wrapper::is_empty)
      .def("reset", &ondemand_object_wrapper::reset)
      .def("__getitem__", [](const ondemand_object_wrapper &self,
                             std::string_view key) {
        self.reset();
        return self.find_field_unordered(key);
      }, "key"_a)
      .def("find_field", &ondemand_object_wrapper::find_field, "key"_a)
      .def("find_field_unordered", &ondemand_object_wrapper::find_field_unordered,
           "key"_a)
      .def("at_pointer", &ondemand_object_wrapper::at_pointer, "json_pointer"_a)
      .def("at_path", &ondemand_object_wrapper::at_path, "json_path"_a)
      .def("__contains__", &ondemand_object_wrapper::contains, "key"_a)
      .def("keys", &ondemand_object_wrapper::keys)
      .def("__iter__",
           [](const ondemand_object_wrapper &self) {
             return ondemand_object_key_iterator_wrapper(self);
           })
      .def("fields",
           [](const ondemand_object_wrapper &self) {
             return ondemand_object_field_iterator_wrapper(self);
           })
      .def("raw_json", &ondemand_object_wrapper::raw_json)
      .def("to_json", &ondemand_object_wrapper::to_json)
      .def("__str__", &ondemand_object_wrapper::to_json)
      .def("__repr__", &ondemand_object_wrapper::repr);

  nb::class_<ondemand_value_wrapper>(m, "OnDemandValue")
      .def("get_array", &ondemand_value_wrapper::get_array)
      .def("get_object", &ondemand_value_wrapper::get_object)
      .def("get_uint64", &ondemand_value_wrapper::get_uint64)
      .def("get_uint64_in_string", &ondemand_value_wrapper::get_uint64_in_string)
      .def("get_int64", &ondemand_value_wrapper::get_int64)
      .def("get_int64_in_string", &ondemand_value_wrapper::get_int64_in_string)
      .def("get_uint32", &ondemand_value_wrapper::get_uint32)
      .def("get_int32", &ondemand_value_wrapper::get_int32)
      .def("get_double", &ondemand_value_wrapper::get_double)
      .def("get_double_in_string", &ondemand_value_wrapper::get_double_in_string)
      .def("get_string", &ondemand_value_wrapper::get_string,
           "allow_replacement"_a = false)
      .def("get_wobbly_string", &ondemand_value_wrapper::get_wobbly_string)
      .def("get_raw_json_string", &ondemand_value_wrapper::get_raw_json_string)
      .def("get_bool", &ondemand_value_wrapper::get_bool)
      .def("is_null", &ondemand_value_wrapper::is_null)
      .def("count_elements", &ondemand_value_wrapper::count_elements)
      .def("count_fields", &ondemand_value_wrapper::count_fields)
      .def("at", &ondemand_value_wrapper::at, "index"_a)
      .def("find_field", &ondemand_value_wrapper::find_field, "key"_a)
      .def("find_field_unordered", &ondemand_value_wrapper::find_field_unordered,
           "key"_a)
      .def("at_pointer", &ondemand_value_wrapper::at_pointer, "json_pointer"_a)
      .def("at_path", &ondemand_value_wrapper::at_path, "json_path"_a)
      .def_prop_ro("type", &ondemand_value_wrapper::type)
      .def("is_scalar", &ondemand_value_wrapper::is_scalar)
      .def("is_string", &ondemand_value_wrapper::is_string)
      .def("is_negative", &ondemand_value_wrapper::is_negative)
      .def("is_integer", &ondemand_value_wrapper::is_integer)
      .def("get_number_type", &ondemand_value_wrapper::get_number_type)
      .def("get_number", &ondemand_value_wrapper::get_number)
      .def("raw_json_token", &ondemand_value_wrapper::raw_json_token)
      .def("raw_json", &ondemand_value_wrapper::raw_json)
      .def("current_depth", &ondemand_value_wrapper::current_depth)
      .def("current_location", &ondemand_value_wrapper::current_location)
      .def("__len__", [](const ondemand_value_wrapper &self) {
        if (self.type() == simdjson::ondemand::json_type::array) {
          auto array = self.get_array();
          array.reset();
          return array.count_elements();
        }
        if (self.type() == simdjson::ondemand::json_type::object) {
          auto object = self.get_object();
          object.reset();
          return object.count_fields();
        }
        throw nb::type_error("On-Demand value is not an array or object");
      })
      .def("__getitem__",
           [](const ondemand_value_wrapper &self, nb::handle key) {
             if (PyLong_Check(key.ptr())) {
               auto array = self.get_array();
               array.reset();
               return array.at(nb::cast<size_t>(key));
             }
             auto object = self.get_object();
             object.reset();
             return object.find_field_unordered(nb::cast<std::string_view>(key));
           },
           "key"_a)
      .def("__iter__",
           [](const ondemand_value_wrapper &self) -> nb::object {
             if (self.type() == simdjson::ondemand::json_type::array) {
               return nb::cast(ondemand_array_iterator_wrapper(self.get_array()));
             }
             if (self.type() == simdjson::ondemand::json_type::object) {
               return nb::cast(
                   ondemand_object_key_iterator_wrapper(self.get_object()));
             }
             throw nb::type_error("On-Demand value is not iterable");
           })
      .def("to_json", &ondemand_value_wrapper::to_json)
      .def("__str__", &ondemand_value_wrapper::to_json)
      .def("__repr__", &ondemand_value_wrapper::repr);

  nb::class_<ondemand_document_wrapper>(m, "OnDemandDocument")
      .def("get_array", &ondemand_document_wrapper::get_array)
      .def("get_object", &ondemand_document_wrapper::get_object)
      .def("get_uint64", &ondemand_document_wrapper::get_uint64)
      .def("get_uint64_in_string",
           &ondemand_document_wrapper::get_uint64_in_string)
      .def("get_int64", &ondemand_document_wrapper::get_int64)
      .def("get_int64_in_string",
           &ondemand_document_wrapper::get_int64_in_string)
      .def("get_uint32", &ondemand_document_wrapper::get_uint32)
      .def("get_int32", &ondemand_document_wrapper::get_int32)
      .def("get_double", &ondemand_document_wrapper::get_double)
      .def("get_double_in_string",
           &ondemand_document_wrapper::get_double_in_string)
      .def("get_string", &ondemand_document_wrapper::get_string,
           "allow_replacement"_a = false)
      .def("get_wobbly_string", &ondemand_document_wrapper::get_wobbly_string)
      .def("get_raw_json_string",
           &ondemand_document_wrapper::get_raw_json_string)
      .def("get_bool", &ondemand_document_wrapper::get_bool)
      .def("get_value", &ondemand_document_wrapper::get_value)
      .def("is_null", &ondemand_document_wrapper::is_null)
      .def("count_elements", &ondemand_document_wrapper::count_elements)
      .def("count_fields", &ondemand_document_wrapper::count_fields)
      .def("at", &ondemand_document_wrapper::at, "index"_a)
      .def("find_field", &ondemand_document_wrapper::find_field, "key"_a)
      .def("find_field_unordered",
           &ondemand_document_wrapper::find_field_unordered, "key"_a)
      .def("at_pointer", &ondemand_document_wrapper::at_pointer,
           "json_pointer"_a)
      .def("at_path", &ondemand_document_wrapper::at_path, "json_path"_a)
      .def_prop_ro("type", &ondemand_document_wrapper::type)
      .def("is_scalar", &ondemand_document_wrapper::is_scalar)
      .def("is_string", &ondemand_document_wrapper::is_string)
      .def("is_negative", &ondemand_document_wrapper::is_negative)
      .def("is_integer", &ondemand_document_wrapper::is_integer)
      .def("get_number_type", &ondemand_document_wrapper::get_number_type)
      .def("get_number", &ondemand_document_wrapper::get_number)
      .def("raw_json_token", &ondemand_document_wrapper::raw_json_token)
      .def("raw_json", &ondemand_document_wrapper::raw_json)
      .def("rewind", &ondemand_document_wrapper::rewind)
      .def("is_alive", &ondemand_document_wrapper::is_alive)
      .def("at_end", &ondemand_document_wrapper::at_end)
      .def("current_depth", &ondemand_document_wrapper::current_depth)
      .def("current_location", &ondemand_document_wrapper::current_location)
      .def("__len__", [](const ondemand_document_wrapper &self) {
        self.rewind();
        if (self.type() == simdjson::ondemand::json_type::array) {
          return self.count_elements();
        }
        if (self.type() == simdjson::ondemand::json_type::object) {
          return self.count_fields();
        }
        throw nb::type_error("On-Demand document is not an array or object");
      })
      .def("__getitem__",
           [](const ondemand_document_wrapper &self, nb::handle key) {
             self.rewind();
             if (PyLong_Check(key.ptr())) {
               return self.at(nb::cast<size_t>(key));
             }
             return self.find_field_unordered(nb::cast<std::string_view>(key));
           },
           "key"_a)
      .def("__iter__",
           [](const ondemand_document_wrapper &self) -> nb::object {
             self.rewind();
             if (self.type() == simdjson::ondemand::json_type::array) {
               return nb::cast(ondemand_array_iterator_wrapper(self.get_array()));
             }
             if (self.type() == simdjson::ondemand::json_type::object) {
               return nb::cast(
                   ondemand_object_key_iterator_wrapper(self.get_object()));
             }
             throw nb::type_error("On-Demand document is not iterable");
           })
      .def("to_json", &ondemand_document_wrapper::to_json)
      .def("__str__", &ondemand_document_wrapper::to_json)
      .def("__repr__", &ondemand_document_wrapper::repr);

  nb::class_<ondemand_document_reference_wrapper>(m, "OnDemandDocumentReference")
      .def("get_array", &ondemand_document_reference_wrapper::get_array)
      .def("get_object", &ondemand_document_reference_wrapper::get_object)
      .def("get_uint64", &ondemand_document_reference_wrapper::get_uint64)
      .def("get_uint64_in_string",
           &ondemand_document_reference_wrapper::get_uint64_in_string)
      .def("get_int64", &ondemand_document_reference_wrapper::get_int64)
      .def("get_int64_in_string",
           &ondemand_document_reference_wrapper::get_int64_in_string)
      .def("get_uint32", &ondemand_document_reference_wrapper::get_uint32)
      .def("get_int32", &ondemand_document_reference_wrapper::get_int32)
      .def("get_double", &ondemand_document_reference_wrapper::get_double)
      .def("get_double_in_string",
           &ondemand_document_reference_wrapper::get_double_in_string)
      .def("get_string", &ondemand_document_reference_wrapper::get_string,
           "allow_replacement"_a = false)
      .def("get_wobbly_string",
           &ondemand_document_reference_wrapper::get_wobbly_string)
      .def("get_raw_json_string",
           &ondemand_document_reference_wrapper::get_raw_json_string)
      .def("get_bool", &ondemand_document_reference_wrapper::get_bool)
      .def("get_value", &ondemand_document_reference_wrapper::get_value)
      .def("is_null", &ondemand_document_reference_wrapper::is_null)
      .def("count_elements",
           &ondemand_document_reference_wrapper::count_elements)
      .def("count_fields", &ondemand_document_reference_wrapper::count_fields)
      .def("at", &ondemand_document_reference_wrapper::at, "index"_a)
      .def("find_field", &ondemand_document_reference_wrapper::find_field,
           "key"_a)
      .def("find_field_unordered",
           &ondemand_document_reference_wrapper::find_field_unordered, "key"_a)
      .def("at_pointer", &ondemand_document_reference_wrapper::at_pointer,
           "json_pointer"_a)
      .def("at_path", &ondemand_document_reference_wrapper::at_path,
           "json_path"_a)
      .def_prop_ro("type", &ondemand_document_reference_wrapper::type)
      .def("is_scalar", &ondemand_document_reference_wrapper::is_scalar)
      .def("is_string", &ondemand_document_reference_wrapper::is_string)
      .def("is_negative", &ondemand_document_reference_wrapper::is_negative)
      .def("is_integer", &ondemand_document_reference_wrapper::is_integer)
      .def("get_number_type",
           &ondemand_document_reference_wrapper::get_number_type)
      .def("get_number", &ondemand_document_reference_wrapper::get_number)
      .def("raw_json_token", &ondemand_document_reference_wrapper::raw_json_token)
      .def("raw_json", &ondemand_document_reference_wrapper::raw_json)
      .def("rewind", &ondemand_document_reference_wrapper::rewind)
      .def("current_depth", &ondemand_document_reference_wrapper::current_depth)
      .def("current_location",
           &ondemand_document_reference_wrapper::current_location)
      .def("__len__", [](const ondemand_document_reference_wrapper &self) {
        self.rewind();
        if (self.type() == simdjson::ondemand::json_type::array) {
          return self.count_elements();
        }
        if (self.type() == simdjson::ondemand::json_type::object) {
          return self.count_fields();
        }
        throw nb::type_error(
            "On-Demand document reference is not an array or object");
      })
      .def("__getitem__",
           [](const ondemand_document_reference_wrapper &self, nb::handle key) {
             self.rewind();
             if (PyLong_Check(key.ptr())) {
               return self.at(nb::cast<size_t>(key));
             }
             return self.find_field_unordered(nb::cast<std::string_view>(key));
           },
           "key"_a)
      .def("__iter__",
           [](const ondemand_document_reference_wrapper &self) -> nb::object {
             self.rewind();
             if (self.type() == simdjson::ondemand::json_type::array) {
               return nb::cast(ondemand_array_iterator_wrapper(self.get_array()));
             }
             if (self.type() == simdjson::ondemand::json_type::object) {
               return nb::cast(
                   ondemand_object_key_iterator_wrapper(self.get_object()));
             }
             throw nb::type_error(
                 "On-Demand document reference is not iterable");
           })
      .def("to_json", &ondemand_document_reference_wrapper::to_json)
      .def("__str__", &ondemand_document_reference_wrapper::to_json)
      .def("__repr__", &ondemand_document_reference_wrapper::repr);

  nb::class_<ondemand_document_stream_wrapper>(m, "OnDemandDocumentStream")
      .def_prop_ro("size_in_bytes",
                   &ondemand_document_stream_wrapper::size_in_bytes)
      .def_prop_ro("truncated_bytes",
                   &ondemand_document_stream_wrapper::truncated_bytes)
      .def("__iter__",
           [](const ondemand_document_stream_wrapper &self) {
             return ondemand_document_stream_iterator_wrapper(self.state);
           });

  nb::class_<ondemand_parser_wrapper>(m, "OnDemandParser")
      .def(nb::init<size_t>(),
           "max_capacity"_a = simdjson::SIMDJSON_MAXSIZE_BYTES)
      .def("iterate", &ondemand_parser_wrapper::iterate, "data"_a)
      .def(
          "iterate_many",
          [](const ondemand_parser_wrapper &self, nb::handle handle,
             size_t batch_size, bool allow_comma_separated) {
            const auto config = read_ondemand_parser_config(self);
            auto state =
                std::make_shared<ondemand_stream_state>(config.max_capacity);
            configure_ondemand_parser(state->parser, config,
                                      "On-Demand parser clone allocate");
            state->input = padded_storage_from_python(handle);
            auto result = with_gil_released([&] {
              return state->parser.iterate_many(state->input->value, batch_size,
                                                allow_comma_separated);
            });
            simdjson::ondemand::document_stream stream;
            ensure_success(std::move(result).get(stream), "On-Demand iterate_many");
            state->stream = std::move(stream);
            state->generation = 1;
            return ondemand_document_stream_wrapper(state);
          },
          "data"_a,
          "batch_size"_a = simdjson::ondemand::DEFAULT_BATCH_SIZE,
          "allow_comma_separated"_a = false)
      .def_prop_ro("capacity", &ondemand_parser_wrapper::capacity)
      .def_prop_ro("max_capacity", &ondemand_parser_wrapper::max_capacity)
      .def_prop_ro("max_depth", &ondemand_parser_wrapper::max_depth)
      .def("set_max_capacity", &ondemand_parser_wrapper::set_max_capacity,
           "max_capacity"_a)
      .def("allocate", &ondemand_parser_wrapper::allocate, "capacity"_a,
           "max_depth"_a = simdjson::DEFAULT_MAX_DEPTH)
      .def_prop_rw("threaded", &ondemand_parser_wrapper::threaded,
                   &ondemand_parser_wrapper::set_threaded);
}

}  // namespace simdjsonpy
