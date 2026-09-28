#ifndef TPJ_TESTS_SIM_SUPPORT_PARK_WORLDS_H
#define TPJ_TESTS_SIM_SUPPORT_PARK_WORLDS_H

#include "sim/entity_key.h"
#include "sim/field_text.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj::test {

// Park intent as the public queries give it. A world holding exactly this intent is written as a
// save and loaded, so tests can state any arrangement, including ones no command would accept.
struct ParkIntent {
  std::vector<ParkEntrance> Entrances;
  std::vector<ParkPath> Paths;
  std::vector<ParkBox> Boxes;
};

inline ParkIntent intentOf(const World &world) {
  return {parkEntrances(world), parkPaths(world), parkBoxes(world)};
}

inline void writePose(std::string &out, const Pose &pose) {
  out += " x=";
  writeNumber(out, pose.X);
  out += " z=";
  writeNumber(out, pose.Z);
  out += " facing-x=";
  writeNumber(out, pose.FacingX);
  out += " facing-z=";
  writeNumber(out, pose.FacingZ);
}

inline void writeKey(std::string &out, EntityKey key) {
  writeNumber(out, static_cast<uint64_t>(key));
}

// A world with makeParkSchema's schema holding exactly the intent, each entity under its given key,
// with the next key above them all.
inline World worldOf(ParkIntent intent) {
  uint64_t highest = 0;
  const auto note = [&highest](EntityKey key) {
    highest = std::max(highest, static_cast<uint64_t>(key));
  };
  const auto byKey = [](const auto &left, const auto &right) { return left.Key < right.Key; };
  std::ranges::sort(intent.Entrances, byKey);
  std::ranges::sort(intent.Paths, byKey);
  std::ranges::sort(intent.Boxes, byKey);

  std::string sections;
  if (!intent.Entrances.empty()) {
    sections += "\n[entrance]\n";
    for (const ParkEntrance &entrance : intent.Entrances) {
      note(entrance.Key);
      writeKey(sections, entrance.Key);
      writePose(sections, entrance.At);
      sections += "\n";
    }
  }
  if (!intent.Paths.empty()) {
    sections += "\n[path]\n";
    for (const ParkPath &path : intent.Paths) {
      note(path.Key);
      writeKey(sections, path.Key);
      sections += " kind=";
      sections += enumNames(path.Kind).at(static_cast<std::size_t>(path.Kind));
      sections += " points=[";
      for (std::size_t index = 0; index < path.Points.size(); ++index) {
        sections += index == 0 ? "{x=" : " {x=";
        writeNumber(sections, path.Points[index].X);
        sections += " z=";
        writeNumber(sections, path.Points[index].Z);
        sections += "}";
      }
      sections += "]\n";
    }
  }
  if (!intent.Boxes.empty()) {
    sections += "\n[box]\n";
    for (const ParkBox &box : intent.Boxes) {
      note(box.Key);
      writeKey(sections, box.Key);
      sections += " kind=";
      sections += enumNames(box.Kind).at(static_cast<std::size_t>(box.Kind));
      writePose(sections, box.At);
      sections += "\n";
    }
  }

  std::string text = "tpj-park 1\nseed 1\ntick 0\nnext-key ";
  writeNumber(text, highest + 1);
  text += "\n";
  text += sections;
  return loadWorld(makeParkSchema(), text);
}

inline bool sameBits(double left, double right) {
  return std::bit_cast<uint64_t>(left) == std::bit_cast<uint64_t>(right);
}

inline bool samePose(const Pose &left, const Pose &right) {
  return sameBits(left.X, right.X) && sameBits(left.Z, right.Z) &&
         sameBits(left.FacingX, right.FacingX) && sameBits(left.FacingZ, right.FacingZ);
}

inline bool samePoints(const std::vector<ParkPoint> &left, const std::vector<ParkPoint> &right) {
  return std::ranges::equal(left, right, [](const ParkPoint &a, const ParkPoint &b) {
    return sameBits(a.X, b.X) && sameBits(a.Z, b.Z);
  });
}

} // namespace tpj::test

#endif
