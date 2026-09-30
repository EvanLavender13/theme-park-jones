#ifndef TPJ_LEGIBLE_PREVIEW_H
#define TPJ_LEGIBLE_PREVIEW_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>

namespace tpj {

// What a shop ghost would find: where its guest connector meets the guest path in the candidate,
// the committed world's hungry footfall there, and its nearest depot and supply route length in
// the candidate.
struct ShopContext {
  EntityKey Shop = NULL_KEY;
  std::optional<Place> Connection;
  double Footfall = 0.0;
  std::optional<DepotRoute> Supply;

  bool operator==(const ShopContext &) const = default;
};

// A tentative edit, the candidate world it gives when accepted, and a shop ghost's context.
struct Preview {
  std::optional<ParkEdit> Edit;
  std::optional<World> Candidate;
  std::optional<ShopContext> Shop;
};

// The preview of the edit on the world: the candidate, makeCandidate with the edit queued, when
// the edit is accepted, and its shop context when it adds or moves a shop. Changes nothing.
Preview previewEdit(const World &world, const std::optional<ParkEdit> &edit);

// The preview's candidate when it holds one, and the world otherwise.
const World &previewedWorld(const World &world, const Preview &preview);

// A preview kept between frames, and the tick of the world it was made from, or none before the
// first.
struct KeptPreview {
  std::optional<uint64_t> Tick;
  Preview Made;
};

// Makes the kept preview again, as previewEdit gives it, when nothing is kept or the world's tick
// or the edit differs from the kept one's, and returns whether it did. A world changes only as it
// ticks, so the kept preview is previewEdit's for as long as it is kept for one world: empty it
// when the world is replaced.
bool keepPreview(KeptPreview &kept, const World &world, const std::optional<ParkEdit> &edit);

// A shop ghost's context in the candidate of the edit on the world, or none when the edit neither
// adds nor moves a shop. Changes nothing.
std::optional<ShopContext> shopContext(const World &world, const World &candidate,
                                       const ParkEdit &edit);

} // namespace tpj

#endif
