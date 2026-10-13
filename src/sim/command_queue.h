#ifndef TPJ_SIM_COMMAND_QUEUE_H
#define TPJ_SIM_COMMAND_QUEUE_H

#include "sim/type_id.h"

#include <memory>
#include <stddef.h>
#include <type_traits>
#include <vector>

namespace tpj {

// A command waiting for the next cycle, or for commitCommands, with its type's id. Value points at
// the value the queue keeps, valid until the queue is emptied or destroyed.
struct QueuedCommand {
  TypeId Id = nullptr;
  const void *Value = nullptr;
};

// Commands submitted since the last cycle, in submission order. It lives outside the world, so
// copies, hashes, and saves never hold pending commands. It keeps the values of the commands it has
// held, for each type, and reuses their memory for later commands of that type.
class CommandQueue {
public:
  CommandQueue() = default;
  CommandQueue(const CommandQueue &) = delete;
  CommandQueue &operator=(const CommandQueue &) = delete;
  CommandQueue(CommandQueue &&) noexcept = default;
  CommandQueue &operator=(CommandQueue &&) noexcept = default;
  ~CommandQueue() = default;

  // Copies the command into the next value kept for commands of its type, which reuses that value's
  // memory, or into a new value when every kept one is in use.
  template <typename T> void push(const T &command);
  [[nodiscard]] const std::vector<QueuedCommand> &commands() const { return Commands; }
  [[nodiscard]] bool empty() const { return Commands.empty(); }
  // Empties the queue, keeping every value for later commands of its type.
  void clear();

private:
  // The values kept for one command type: Values points at a std::vector<std::unique_ptr<T>>, of
  // which the first Used are queued.
  struct Kept {
    TypeId Id = nullptr;
    std::unique_ptr<void, void (*)(void *)> Values{nullptr, nullptr};
    size_t Used = 0;
  };
  std::vector<Kept> Types;
  std::vector<QueuedCommand> Commands;
};

template <typename T> void CommandQueue::push(const T &command) {
  static_assert(std::is_copy_constructible_v<T>, "a command must be copyable");
  static_assert(std::is_copy_assignable_v<T>, "a command must be copy assignable");
  using Values = std::vector<std::unique_ptr<T>>;
  const TypeId id = typeIdOf<T>();
  Kept *kept = nullptr;
  for (Kept &each : Types) {
    if (each.Id == id) {
      kept = &each;
      break;
    }
  }
  if (kept == nullptr) {
    kept = &Types.emplace_back(
        Kept{id, {new Values(), [](void *values) { delete static_cast<Values *>(values); }}, 0});
  }
  Values &values = *static_cast<Values *>(kept->Values.get());
  if (kept->Used < values.size()) {
    *values[kept->Used] = command;
  } else {
    values.push_back(std::make_unique<T>(command));
  }
  Commands.push_back(QueuedCommand{id, values[kept->Used].get()});
  ++kept->Used;
}

inline void CommandQueue::clear() {
  for (Kept &kept : Types) {
    kept.Used = 0;
  }
  Commands.clear();
}

} // namespace tpj

#endif
