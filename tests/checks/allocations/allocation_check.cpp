// The allocation check (decision 0041): the work the app repeats in steady play, repeated on
// full.park after a warm-up, must free no heap block it allocated while it repeats. Each tick is
// followed by the scene's sync, as the app runs it: once for the frame with the tick, and once for
// a frame without one.

#include "watch.h"

#include "app/scene/scene_sync.h"
#include "app/session/park_session.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>

namespace {

// Ticks stepped before watching, so that the kept lists have grown to what the park needs.
constexpr uint64_t WARM_TICKS = 300;
// Ticks watched: long enough for shops to serve, offers to change, and packets to arrive many times
// over. Guests and seat services live longer than this, so it can't see their storage reused.
constexpr uint64_t WATCHED_TICKS = 600;

std::string readText(const char *path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error(std::string("cannot read ") + path);
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// The scene's sync after a frame, as the app runs it.
void syncScene(tpj::SceneSync &scene, const tpj::ParkSession &session) {
  scene.syncWorld(session.world(), session.generation(), session.edits());
}

// Steps the park through the app's own session, syncing the scene after each tick, and reports
// the blocks the watched ticks freed.
int checkTicks(tpj::ParkSession &session) {
  tpj::SceneSync scene;
  for (uint64_t tick = 0; tick < WARM_TICKS; ++tick) {
    session.step();
    syncScene(scene, session);
  }
  tpj::check::startWatching(session.world().Tick);
  for (uint64_t tick = 0; tick < WATCHED_TICKS; ++tick) {
    tpj::check::watchTick(session.world().Tick);
    tpj::check::watchWork("the tick");
    session.step();
    tpj::check::watchWork("the scene's sync");
    syncScene(scene, session);
    syncScene(scene, session);
  }
  tpj::check::stopWatching();
  if (tpj::check::tableOverflowed()) {
    std::cout << "allocation check: the watched ticks noted more blocks than the table holds\n";
    return 1;
  }
  if (tpj::check::freedCount() > 0) {
    std::cout << "allocation check: " << tpj::check::freedCount()
              << " blocks were allocated and freed during the watched ticks\n";
    for (const tpj::check::FreedBlock &block : tpj::check::freedBlocks()) {
      std::cout << "allocation check: " << block.FreedBy << " freed a " << block.Size
                << "-byte block at tick " << block.Freed << " that " << block.AllocatedBy
                << " allocated at tick " << block.Allocated << '\n';
    }
    return 1;
  }
  std::cout << "allocation check: " << WATCHED_TICKS
            << " ticks of full.park and the scene's sync freed no block they allocated\n";
  return 0;
}

} // namespace

int main() {
  try {
    const std::string text = readText(TPJ_PARKS_DIR "/stress/full.park");
    tpj::World world = tpj::loadWorld(tpj::makeParkSchema(), text);
    tpj::resolveWorld(world);
    tpj::ParkSession session(std::move(world));
    return checkTicks(session);
  } catch (const std::exception &error) {
    std::cout << "allocation check: " << error.what() << '\n';
    return 2;
  }
}
