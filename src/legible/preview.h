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

// Makes the kept preview again, as previewEdit gives it, when nothing is kept, when the edit
// differs from the kept one's, or, when following ticks, when the world's tick differs, and
// returns whether it did. Otherwise it reads the shop context's footfall from the world again.
// Ticks change the world's state but nothing it derives from intent, so a preview kept across
// ticks still shows what the edit derives exactly; follow ticks to show the candidate's state.
// The kept preview holds only while the world changes just by ticking: empty it when the world is
// replaced, or changed without a tick, as edits applied while paused change it.
bool keepPreview(KeptPreview &kept, const World &world, const std::optional<ParkEdit> &edit,
                 bool followTicks);

// A shop ghost's context in the candidate of the edit on the world, or none when the edit neither
// adds nor moves a shop. Changes nothing.
std::optional<ShopContext> shopContext(const World &world, const World &candidate,
                                       const ParkEdit &edit);

} // namespace tpj

#endif
