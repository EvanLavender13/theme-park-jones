#include "app/session/park_session.h"

#include "sim/park_schema.h"

#include <utility>

namespace tpj {

World resolvedNewPark() {
  World world = makeNewPark(1);
  resolveWorld(world);
  return world;
}

OpenedPark startingPark(const char *parkPath, uint64_t ticks) {
  OpenedPark opened;
  if (parkPath == nullptr) {
    opened.Park = resolvedNewPark();
  } else {
    opened = openParkFile(parkPath);
  }
  if (opened.Park) {
    TickBuffers buffers;
    for (uint64_t tick = 0; tick < ticks; ++tick) {
      stepWorld(*opened.Park, buffers);
    }
  }
  return opened;
}

ParkSession::ParkSession(World world) : Current(std::move(world)) {
  clearHistory(History, Current);
}

void ParkSession::step() {
  // An undo or redo applies alone, so the history can tell which keys its restored things took.
  if (Restore) {
    CommandQueue alone;
    queueEdit(alone, *Restore);
    Restore.reset();
    stepWorld(Current, alone, Buffers);
    noteEdits(History, Current);
    return;
  }
  const bool edited = !Commands.empty();
  stepWorld(Current, Commands, Buffers);
  if (edited) {
    noteEdits(History, Current);
  }
}

void ParkSession::applyWithoutTick() {
  if (Restore) {
    CommandQueue alone;
    queueEdit(alone, *Restore);
    Restore.reset();
    commitCommands(Current, alone);
    noteEdits(History, Current);
    return;
  }
  if (Commands.empty()) {
    return;
  }
  commitCommands(Current, Commands);
  noteEdits(History, Current);
}

void ParkSession::undo() {
  if (Commands.empty() && !Restore) {
    Restore = undoEdit(History);
  }
}

void ParkSession::redo() {
  if (Commands.empty() && !Restore) {
    Restore = redoEdit(History);
  }
}

// The lists alone decide, so the buttons don't flicker off while an edit waits for its tick.
bool ParkSession::canUndo() const { return !History.Undos.empty(); }

bool ParkSession::canRedo() const { return !History.Redos.empty(); }

void ParkSession::useFileRequest(const FileRequest &request, ParkFileEdge &edge) {
  if (request.Action == ParkAction::Save) {
    // The dialog asked only about the name as typed, so a save whose extension was added asks
    // before replacing a file.
    const std::string path = withParkExtension(request.Path);
    if (path != request.Path && fileExists(path.c_str()) && !edge.confirmReplace(path)) {
      return;
    }
    const std::string error = saveParkFile(Current, path.c_str());
    if (!error.empty()) {
      edge.reportError(error);
    }
  } else if (request.Action == ParkAction::New) {
    replace(resolvedNewPark());
  } else if (request.Action == ParkAction::Open) {
    OpenedPark opened = openParkFile(request.Path.c_str());
    if (!opened.Park) {
      edge.reportError(opened.Error);
      return;
    }
    replace(std::move(*opened.Park));
  }
}

void ParkSession::replace(World world) {
  Current = std::move(world);
  Commands.clear();
  Restore.reset();
  clearHistory(History, Current);
  ++Generation;
}

} // namespace tpj
