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
    for (uint64_t tick = 0; tick < ticks; ++tick) {
      stepWorld(*opened.Park);
    }
  }
  return opened;
}

ParkSession::ParkSession(World world) : Current(std::move(world)) {}

void ParkSession::step() { stepWorld(Current, Commands); }

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
  ++Generation;
}

} // namespace tpj
