#include "tools/history.h"

#include "sim/park/intent.h"

#include <algorithm>
#include <stddef.h>
#include <utility>

namespace tpj {
namespace {

// The world's path points, segments, and boxes, each in key order, and its opening hours.
IntentRecords recordsOf(const World &world) {
  IntentRecords records;
  records.Points = parkPathPoints(world);
  for (const ParkSegment &segment : parkSegments(world)) {
    records.Segments.push_back(SegmentRecord{segment.Key, segment.Kind, segment.From, segment.To,
                                             segment.Width, segment.FromEdge, segment.ToEdge});
  }
  records.Boxes = parkBoxes(world);
  records.Hours = parkHours(world);
  return records;
}

// Sorts the records of two lists in key order into removed, added, and changed, before and after.
template <typename Record>
void compare(const std::vector<Record> &was, const std::vector<Record> &is,
             std::vector<Record> &removed, std::vector<Record> &added, std::vector<Record> &before,
             std::vector<Record> &after) {
  size_t i = 0;
  size_t j = 0;
  while (i < was.size() || j < is.size()) {
    if (j == is.size() || (i < was.size() && was[i].Key < is[j].Key)) {
      removed.push_back(was[i++]);
    } else if (i == was.size() || is[j].Key < was[i].Key) {
      added.push_back(is[j++]);
    } else {
      if (!(was[i] == is[j])) {
        before.push_back(was[i]);
        after.push_back(is[j]);
      }
      ++i;
      ++j;
    }
  }
}

IntentChange changeBetween(const IntentRecords &was, const IntentRecords &is) {
  IntentChange change;
  compare(was.Points, is.Points, change.Removed.Points, change.Added.Points, change.Before.Points,
          change.After.Points);
  compare(was.Segments, is.Segments, change.Removed.Segments, change.Added.Segments,
          change.Before.Segments, change.After.Segments);
  compare(was.Boxes, is.Boxes, change.Removed.Boxes, change.Added.Boxes, change.Before.Boxes,
          change.After.Boxes);
  if (was.Hours != is.Hours) {
    change.Before.Hours = was.Hours;
    change.After.Hours = is.Hours;
  }
  return change;
}

bool isEmpty(const IntentRecords &records) {
  return records.Points.empty() && records.Segments.empty() && records.Boxes.empty() &&
         !records.Hours;
}

bool isEmpty(const IntentChange &change) {
  return isEmpty(change.Removed) && isEmpty(change.Added) && isEmpty(change.Before) &&
         isEmpty(change.After);
}

// The key the record the key named holds now, following each rename.
EntityKey current(const EditHistory &history, EntityKey key) {
  for (auto found = history.Renamed.find(key); found != history.Renamed.end();
       found = history.Renamed.find(key)) {
    key = found->second;
  }
  return key;
}

// The command that turns the intent after the change back into the intent before it. What it adds
// back keeps its old keys as names, and its segments name those points by them.
RestoreIntent reversal(const EditHistory &history, const IntentChange &change) {
  const auto isAddedBack = [&change](EntityKey point) {
    return std::ranges::binary_search(change.Removed.Points, point, {}, &ParkPathPoint::Key);
  };
  const auto pointNow = [&](EntityKey point) {
    return isAddedBack(point) ? point : current(history, point);
  };
  const auto segmentNow = [&](SegmentRecord segment) {
    segment.From = pointNow(segment.From);
    segment.To = pointNow(segment.To);
    return segment;
  };
  RestoreIntent command;
  for (const ParkPathPoint &point : change.Added.Points) {
    command.Remove.push_back(current(history, point.Key));
  }
  for (const SegmentRecord &segment : change.Added.Segments) {
    command.Remove.push_back(current(history, segment.Key));
  }
  for (const ParkBox &box : change.Added.Boxes) {
    command.Remove.push_back(current(history, box.Key));
  }
  for (ParkPathPoint point : change.Before.Points) {
    point.Key = current(history, point.Key);
    command.Set.Points.push_back(point);
  }
  for (SegmentRecord segment : change.Before.Segments) {
    segment.Key = current(history, segment.Key);
    command.Set.Segments.push_back(segmentNow(segment));
  }
  for (ParkBox box : change.Before.Boxes) {
    box.Key = current(history, box.Key);
    command.Set.Boxes.push_back(box);
  }
  command.Add.Points = change.Removed.Points;
  for (const SegmentRecord &segment : change.Removed.Segments) {
    command.Add.Segments.push_back(segmentNow(segment));
  }
  command.Add.Boxes = change.Removed.Boxes;
  command.Set.Hours = change.Before.Hours;
  return command;
}

// Pairs each record of a list the command added, in the order it lists them, with a record of what
// the cycle added, in key order, since the counter gives keys in the order records are added.
template <typename Record>
void renameEach(std::map<EntityKey, EntityKey> &renamed, const std::vector<Record> &sent,
                const std::vector<Record> &seen) {
  for (size_t i = 0; i < sent.size() && i < seen.size(); ++i) {
    renamed[sent[i].Key] = seen[i].Key;
  }
}

void rename(EditHistory &history, const IntentChange &seen) {
  renameEach(history.Renamed, history.Sent.Add.Points, seen.Added.Points);
  renameEach(history.Renamed, history.Sent.Add.Segments, seen.Added.Segments);
  renameEach(history.Renamed, history.Sent.Add.Boxes, seen.Added.Boxes);
}

// Takes the last edit off the list and gives the command that reverses it, waiting to note it.
std::optional<RestoreIntent> take(EditHistory &history, std::vector<IntentChange> &from,
                                  HistoryStep step) {
  if (history.Waiting != HistoryStep::None || from.empty()) {
    return std::nullopt;
  }
  history.Taken = std::move(from.back());
  from.pop_back();
  history.Sent = reversal(history, history.Taken);
  history.Waiting = step;
  return history.Sent;
}

} // namespace

void clearHistory(EditHistory &history, const World &world) {
  history = EditHistory{};
  history.Seen = recordsOf(world);
}

void noteEdits(EditHistory &history, const World &world) {
  IntentRecords now = recordsOf(world);
  IntentChange change = changeBetween(history.Seen, now);
  history.Seen = std::move(now);
  const HistoryStep waiting = history.Waiting;
  history.Waiting = HistoryStep::None;
  if (waiting == HistoryStep::None) {
    if (!isEmpty(change)) {
      history.Undos.push_back(std::move(change));
      history.Redos.clear();
    }
    return;
  }
  // A refused undo or redo changed nothing, so its edit goes back where it was.
  if (isEmpty(change)) {
    (waiting == HistoryStep::Undo ? history.Undos : history.Redos)
        .push_back(std::move(history.Taken));
    return;
  }
  rename(history, change);
  (waiting == HistoryStep::Undo ? history.Redos : history.Undos).push_back(std::move(change));
}

std::optional<RestoreIntent> undoEdit(EditHistory &history) {
  return take(history, history.Undos, HistoryStep::Undo);
}

std::optional<RestoreIntent> redoEdit(EditHistory &history) {
  return take(history, history.Redos, HistoryStep::Redo);
}

bool canUndo(const EditHistory &history) {
  return history.Waiting == HistoryStep::None && !history.Undos.empty();
}

bool canRedo(const EditHistory &history) {
  return history.Waiting == HistoryStep::None && !history.Redos.empty();
}

} // namespace tpj
