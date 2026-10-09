#ifndef TPJ_SIM_COMMAND_QUEUE_H
#define TPJ_SIM_COMMAND_QUEUE_H

#include "sim/type_id.h"

#include <any>
#include <type_traits>
#include <utility>
#include <vector>

namespace tpj {

// A command waiting for the next cycle, or for commitCommands, with its type's id.
struct QueuedCommand {
  TypeId Id = nullptr;
  std::any Value;
};

// Commands submitted since the last cycle, in submission order. It lives outside the world, so
// copies, hashes, and saves never hold pending commands.
class CommandQueue {
public:
  template <typename T> void push(T command) {
    static_assert(std::is_copy_constructible_v<T>, "a command must be copyable");
    Commands.push_back(QueuedCommand{typeIdOf<T>(), std::move(command)});
  }

  [[nodiscard]] const std::vector<QueuedCommand> &commands() const { return Commands; }
  [[nodiscard]] bool empty() const { return Commands.empty(); }
  void clear() { Commands.clear(); }

private:
  std::vector<QueuedCommand> Commands;
};

} // namespace tpj

#endif
