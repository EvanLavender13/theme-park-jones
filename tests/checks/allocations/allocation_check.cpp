// The allocation check (decision 0041): the work the app repeats in steady play, repeated on
// full.park after a warm-up, must free no heap block it allocated while it repeats.

#include "watch.h"

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

// Steps the park through the app's own session and reports the blocks the watched ticks freed.
int checkTicks(tpj::ParkSession &session) {
  for (uint64_t tick = 0; tick < WARM_TICKS; ++tick) {
    session.step();
  }
  tpj::check::startWatching(session.world().Tick);
  for (uint64_t tick = 0; tick < WATCHED_TICKS; ++tick) {
    tpj::check::watchTick(session.world().Tick);
    session.step();
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
      std::cout << "allocation check: tick freed a " << block.Size << "-byte block at tick "
                << block.Freed << " that it allocated at tick " << block.Allocated << '\n';
    }
    return 1;
  }
  std::cout << "allocation check: " << WATCHED_TICKS
            << " ticks of full.park freed no block they allocated\n";
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
