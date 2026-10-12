#ifndef TPJ_APP_SESSION_PARK_SESSION_H
#define TPJ_APP_SESSION_PARK_SESSION_H

#include "app/session/park_file.h"
#include "app/session/park_file_requests.h"
#include "sim/command_queue.h"
#include "sim/park/edits.h"
#include "sim/world.h"
#include "tools/history.h"

#include <optional>
#include <stdint.h>
#include <string>

namespace tpj {

// What using a park file request needs from the platform: asking before replacing a file, and
// showing why an open or a save failed.
class ParkFileEdge {
public:
  ParkFileEdge() = default;
  ParkFileEdge(const ParkFileEdge &) = delete;
  ParkFileEdge &operator=(const ParkFileEdge &) = delete;
  ParkFileEdge(ParkFileEdge &&) = delete;
  ParkFileEdge &operator=(ParkFileEdge &&) = delete;
  virtual ~ParkFileEdge() = default;

  // True when the player chooses to replace the file at the path.
  virtual bool confirmReplace(const std::string &path) = 0;
  // Shows a failed open's or save's message.
  virtual void reportError(const std::string &message) = 0;
};

// makeNewPark(1), resolved.
World resolvedNewPark();

// The park the app starts from: resolvedNewPark(), or the park file at parkPath when it is not
// null, stepped ticks cycles with no commands. No world, and openParkFile's message, when the file
// cannot be read or loaded.
OpenedPark startingPark(const char *parkPath, uint64_t ticks);

// The world the app holds, the commands waiting for its next cycle, and the history of its edits.
// It is the one place the world is replaced, and its generation changes exactly when the world is.
class ParkSession {
public:
  explicit ParkSession(World world);

  [[nodiscard]] const World &world() const { return Current; }
  [[nodiscard]] CommandQueue &commands() { return Commands; }
  // Changes exactly when the world is replaced, to a value it has not had.
  [[nodiscard]] uint64_t generation() const { return Generation; }

  // One cycle with the waiting undo or redo alone, when one waits, or else with the queued
  // commands, which it empties. Either way, the history then notes what the cycle changed.
  void step();
  // While paused: applies the waiting undo or redo alone, when one waits, or else the queued
  // commands, which it empties, with commitCommands and no cycle. The history then notes what
  // changed. Does nothing when nothing waits or is queued.
  void applyWithoutTick();
  // Asks for an undo, or a redo: the history's command waits to apply alone in the next cycle.
  // Nothing happens while a command is queued or waits, or when there is nothing to undo or redo.
  void undo();
  void redo();
  // Whether the history holds an edit to undo, or to redo. An edit an undo or redo has taken
  // counts on neither list until its cycle runs.
  [[nodiscard]] bool canUndo() const;
  [[nodiscard]] bool canRedo() const;
  // Acts on a request. Save writes the world as it is to withParkExtension of the path, first
  // asking the edge when that added the extension and a file exists there, and saving nothing
  // unless it confirms. New replaces the world with resolvedNewPark(), and Open with openParkFile's
  // world. A failed open or save leaves the world as it was and reports its message to the edge.
  // Replacing the world empties the queue and the history.
  void useFileRequest(const FileRequest &request, ParkFileEdge &edge);

private:
  void replace(World world);

  World Current;
  CommandQueue Commands;
  // The lists the world's systems reuse from tick to tick. Replacing the world keeps them.
  TickBuffers Buffers;
  EditHistory History;
  // The undo's or redo's command waiting to apply alone in the next cycle.
  std::optional<RestoreIntent> Restore;
  uint64_t Generation = 0;
};

} // namespace tpj

#endif
