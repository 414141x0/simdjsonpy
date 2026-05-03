#include "common.h"

namespace simdjsonpy {

NB_MODULE(_core, m) {
  bind_common(m);
  bind_dom(m);
  bind_ondemand(m);
}

}  // namespace simdjsonpy
