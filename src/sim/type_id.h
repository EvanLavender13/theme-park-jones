#ifndef TPJ_SIM_TYPE_ID_H
#define TPJ_SIM_TYPE_ID_H

namespace tpj {

// Names a C++ type within one run of the program. It differs between runs, so never save or hash
// it.
using TypeId = const void *;

// One value for every call with T, and a different value for every other type. Each instantiation
// has its own static, and it isn't const, so the compiler can't merge two types' statics into one.
template <typename T> TypeId typeIdOf() {
  static char tag = 0;
  return &tag;
}

} // namespace tpj

#endif
