#ifndef TPJ_SIM_OPERATIONS_FACILITIES_H
#define TPJ_SIM_OPERATIONS_FACILITIES_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/operations/operations.h"

#include <optional>
#include <stdint.h>
#include <string_view>

namespace tpj {

class World;
class WorldSchema;

// A guest's visit to a bench, addressed to the guest, which the bench returns to it.
struct RestVisits {
  static constexpr std::string_view Name = "rest-visits";
};

// A rest a bench sends the guest it seated, when its time is up.
struct Rests {
  static constexpr std::string_view Name = "rests";
};

// A guest's visit to a toilet block, addressed to the guest, which the toilet block returns to it.
struct ToiletVisits {
  static constexpr std::string_view Name = "toilet-visits";
};

// A toilet use a toilet block sends the guest it took in, when its time is up.
struct ToiletUses {
  static constexpr std::string_view Name = "toilet-uses";
};

// The rest offer field: each bench's offer, at its guest anchor.
struct RestOffer {
  using Entry = OfferEntry;
  static constexpr std::string_view Name = "rest-offer";
  static constexpr FieldKind Kind = FieldKind::Entry;
};

// The toilet offer field: each toilet block's offer, at its guest anchor.
struct ToiletOffer {
  using Entry = OfferEntry;
  static constexpr std::string_view Name = "toilet-offer";
  static constexpr FieldKind Kind = FieldKind::Entry;
};

// The guests a bench seats at once.
inline constexpr int64_t BENCH_SEATS = 3;
// Ticks a guest keeps its seat on a bench.
inline constexpr uint32_t SITTING_TIME = 300;
// The guests a toilet block takes in at once.
inline constexpr int64_t TOILET_STALLS = 4;
// Ticks a guest keeps its stall in a toilet block.
inline constexpr uint32_t TOILET_TIME = 90;
// The tiredness a rest relieves, on a scale from 0 to 1.
inline constexpr double REST_RELIEF = 0.5;
// The toilet need a toilet use relieves, on a scale from 0 to 1.
inline constexpr double TOILET_RELIEF = 1.0;

// What a bench or toilet block publishes about itself for display and tests (decision 0025): its
// places, how many are taken, and the guests waiting at its door. Nothing in the park reads it.
struct FacilityRecord {
  int64_t Places = 0;
  int64_t Taken = 0;
  int64_t Queue = 0;

  bool operator==(const FacilityRecord &) const = default;
};

// The bench or toilet block box's inspection record in the world as it stands, or none when the
// key is not one.
std::optional<FacilityRecord> facilityRecord(const World &world, EntityKey facility);
// Registers the flow kinds rest-visits, rests, toilet-visits, and toilet-uses, the facility-service
// state, the rest-offer and toilet-offer fields and their resolvers, then the system that steps
// benches and toilet blocks. addOperations calls it after the shops' registrations.
void addFacilities(WorldSchema &schema);

} // namespace tpj

#endif
