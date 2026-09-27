#ifndef TPJ_SIM_COMMAND_QUEUE_H
#define TPJ_SIM_COMMAND_QUEUE_H

#include <entt/core/type_info.hpp>

#include <any>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tpj {

// A command waiting for the next cycle, with its type's id and name.
struct QueuedCommand {
  entt::id_type TypeId = 0;
  std::string_view TypeName;
  std::any Value;
};

// Commands submitted since the last cycle, in submission order. It lives outside the world, so
// copies, hashes, and saves never hold pending commands.
class CommandQueue {
public:
  template <typename T> void push(T command) {
    static_assert(std::is_copy_constructible_v<T>, "a command must be copyable");
    Commands.push_back(
        QueuedCommand{entt::type_id<T>().hash(), entt::type_id<T>().name(), std::move(command)});
  }

  [[nodiscard]] const std::vector<QueuedCommand> &commands() const { return Commands; }
  [[nodiscard]] bool empty() const { return Commands.empty(); }
  void clear() { Commands.clear(); }

private:
  std::vector<QueuedCommand> Commands;
};

} // namespace tpj

#endif
