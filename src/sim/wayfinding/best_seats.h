#ifndef TPJ_SIM_WAYFINDING_BEST_SEATS_H
#define TPJ_SIM_WAYFINDING_BEST_SEATS_H

#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/routes/best_places.h"

#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {

class World;
class WorldSchema;

// The pieces each best seats field keeps at a place: those fewer than this many others beat.
inline constexpr uint32_t SEAT_CHOICES = 4;

// The best seats for rest: pieces of furniture by rest relief and the walk.
struct BestRestSeats {
  using Entry = BestPlace;
  static constexpr std::string_view Name = "best-rest-seats";
  static constexpr FieldKind Kind = FieldKind::Entry;

  static void sampleEdge(const EdgeSample<BestPlace> &sample, std::vector<BestPlace> &out);
};

// The best seats for a meal: pieces of furniture by meal relief and the walk.
struct BestMealSeats {
  using Entry = BestPlace;
  static constexpr std::string_view Name = "best-meal-seats";
  static constexpr FieldKind Kind = FieldKind::Entry;

  static void sampleEdge(const EdgeSample<BestPlace> &sample, std::vector<BestPlace> &out);
};

// Which best seats field a reader samples: best rest seats or best meal seats.
enum class SeatUse : uint8_t { Rest, Meal };

// Fills out with the use's field's entries at the place, exactly the values sampleField gives there
// and in its order, or none where the place doesn't resolve. Reuses out and the scratch's lists,
// so it allocates only while they grow.
void bestSeatsAt(const World &world, const Network &network, const Place &place, SeatUse use,
                 SampleScratch<BestPlace> &scratch, std::vector<BestPlace> &out);

// Registers the best rest seats and best meal seats fields, then the resolver best-seats.
// addPark calls it after addOperations.
void addBestSeats(WorldSchema &schema);

} // namespace tpj

#endif
