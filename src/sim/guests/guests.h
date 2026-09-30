#ifndef TPJ_SIM_GUESTS_GUESTS_H
#define TPJ_SIM_GUESTS_GUESTS_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"

#include <array>
#include <optional>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {

class World;
class WorldSchema;

// What a guest is doing: wandering the paths, or heading home once its stay is over.
enum class GuestActivity : uint8_t { Wandering, HeadingHome };

constexpr std::array<std::string_view, 2> enumNames(GuestActivity /*value*/) {
  return {"wandering", "heading-home"};
}

// What a guest publishes about itself for display and tests (decision 0025): what it is doing,
// where it stands, where that is on the ground (none while its place does not resolve), how
// hungry it is, and the tick its stay ends. Nothing in the park reads it.
struct GuestRecord {
  GuestActivity Activity = GuestActivity::Wandering;
  Place At;
  std::optional<GroundPoint> Position;
  double Hunger = 0.0;
  uint64_t StayUntil = 0;

  bool operator==(const GuestRecord &) const = default;
};

// Ticks between arrivals at each entrance.
inline constexpr uint64_t ARRIVAL_INTERVAL = 60;
// A guest's stay, in ticks, is drawn from STAY_MIN up to STAY_MAX.
inline constexpr uint64_t STAY_MIN = 1800;
inline constexpr uint64_t STAY_MAX = 3600;
// A guest's hunger rises by a rate per tick drawn from these, so from 0 to 1 in 45 to 90 s.
inline constexpr double HUNGER_RATE_MIN = 1.0 / 2700.0;
inline constexpr double HUNGER_RATE_MAX = 1.0 / 1350.0;
// A guest arrives with a hunger drawn from 0 up to this.
inline constexpr double STARTING_HUNGER_MAX = 0.4;
// Meters per second a guest walks.
inline constexpr double WALK_SPEED = 1.3;

// The keys of every guest, ascending.
std::vector<EntityKey> parkGuests(const World &world);
// The guest's inspection record, or none when the key holds no guest.
std::optional<GuestRecord> guestRecord(const World &world, EntityKey guest);
// Registers the guest state and the system that steps guests and admits new ones. The routes and
// operations modules' registrations come first.
void addGuests(WorldSchema &schema);

} // namespace tpj

#endif
