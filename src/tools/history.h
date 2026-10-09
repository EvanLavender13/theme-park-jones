#ifndef TPJ_TOOLS_HISTORY_H
#define TPJ_TOOLS_HISTORY_H

#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/world.h"

#include <map>
#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {

// One edit as the history holds it: the path points, segments, and boxes it removed and added, and
// those it changed, as they were before it and after it, each list in ascending key order.
struct IntentChange {
  IntentRecords Removed;
  IntentRecords Added;
  IntentRecords Before;
  IntentRecords After;
};

// Which command the history waits to note: none, an undo's, or a redo's.
enum class HistoryStep : uint8_t { None, Undo, Redo };

// The edits to undo and to redo, each list oldest first. Change it only through clearHistory,
// noteEdits, undoEdit, and redoEdit.
struct EditHistory {
  // The world's path points, segments, and boxes when the history last looked.
  IntentRecords Seen;
  std::vector<IntentChange> Undos;
  std::vector<IntentChange> Redos;
  // While an undo's or redo's command waits to be noted: which it is, the edit it took off its
  // list, and the command.
  HistoryStep Waiting = HistoryStep::None;
  IntentChange Taken;
  RestoreIntent Sent;
  // Each key whose record an undo or redo added back, and the key it took.
  std::map<EntityKey, EntityKey> Renamed;
};

// Empties both lists and takes the world's intent as the intent the history saw last.
void clearHistory(EditHistory &history, const World &world);
// Records what changed in the world's intent since the history last looked, as tools/SPEC.md's
// History states. Call it after commands were applied, by a cycle or by commitCommands.
void noteEdits(EditHistory &history, const World &world);
// The command that reverses the last edit on the undo list, or on the redo list. None when that
// list is empty or a command waits to be noted. Apply it alone, in its cycle or by
// commitCommands.
std::optional<RestoreIntent> undoEdit(EditHistory &history);
std::optional<RestoreIntent> redoEdit(EditHistory &history);
// Whether undoEdit or redoEdit would give a command.
bool canUndo(const EditHistory &history);
bool canRedo(const EditHistory &history);

} // namespace tpj

#endif
