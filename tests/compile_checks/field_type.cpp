// Registers a component whose one field has the type TPJ_FIELD_CASE selects. A visitFields may list
// only supported field types, so case 0 (double) must compile and every other case must fail to
// compile. The tests in tests/CMakeLists.txt build this file once per case.
#include "sim/schema.h"

#include <string>
#include <vector>

#ifndef TPJ_FIELD_CASE
#define TPJ_FIELD_CASE 0
#endif

namespace tpj {
namespace {

#if TPJ_FIELD_CASE == 0
using FieldType = double;
#elif TPJ_FIELD_CASE == 1
using FieldType = float;
#elif TPJ_FIELD_CASE == 2
using FieldType = std::vector<bool>;
#elif TPJ_FIELD_CASE == 3
using FieldType = std::string;
#elif TPJ_FIELD_CASE == 4
using FieldType = std::vector<float>;
#endif

struct Holder {
  FieldType Value{};
};

template <typename Visitor> void visitFields(Visitor &visitor, Holder &holder) {
  visitor.field("value", holder.Value);
}

} // namespace
} // namespace tpj

int main() {
  try {
    tpj::WorldSchema schema;
    schema.addComponent<tpj::Holder>("holder", tpj::DataKind::State);
    return schema.components().size() == 1 ? 0 : 1;
  } catch (...) {
    return 1;
  }
}
