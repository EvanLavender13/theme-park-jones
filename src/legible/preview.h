#ifndef TPJ_LEGIBLE_PREVIEW_H
#define TPJ_LEGIBLE_PREVIEW_H

#include "sim/entity_key.h"
#include "sim/medium/kept_field.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/tick_buffers.h"
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

// A tentative edit, its plan on the world, the candidate world it gives when accepted, and a shop
// ghost's context.
struct Preview {
  std::optional<ParkEdit> Edit;
  PlannedEdit Planned;
  std::optional<World> Candidate;
  std::optional<ShopContext> Shop;
};

// The preview of the edit on the world: the candidate, makeCandidate with the edit queued, when
// the edit is accepted, and its shop context when it adds or moves a shop. Changes nothing.
Preview previewEdit(const World &world, const std::optional<ParkEdit> &edit);

// The preview's candidate when it holds one, and the world otherwise.
const World &previewedWorld(const World &world, const Preview &preview);

// A preview kept between frames, the tick of the world it was made from, or none before the
// first, and the lists that reading the shop context's footfall again reuses. Spare holds the world
// the next candidate is refilled into, and Buffers the lists planning the edit, applying it, and
// resolving it reuse, so making the preview again allocates no new world.
struct KeptPreview {
  std::optional<uint64_t> Tick;
  Preview Made;
  KeptFieldSample Footfall;
  std::optional<World> Spare;
  TickBuffers Buffers;
};

// Makes the kept preview again, as previewEdit gives it, when nothing is kept, when the edit
// differs from the kept one's, or, when following ticks and the kept preview holds a candidate,
// when the world's tick differs, and returns whether it did. Otherwise it reads the shop context's
// footfall from the world again. Ticks change the world's state but nothing it derives from
// intent, so a preview kept across ticks still shows what the edit derives exactly; follow ticks
// to show the candidate's state. A preview without a candidate holds nothing a tick changes,
// since whether the world accepts an edit depends only on the park's intent. Keeping the preview
// allocates nothing once its lists have grown. The kept preview holds only while the world changes
// just by ticking. When the world is changed without a tick, as edits applied while paused change
// it, clear the kept preview's Tick, so the next call makes it again, keeping its lists. Empty it
// when the world is replaced.
bool keepPreview(KeptPreview &kept, const World &world, const std::optional<ParkEdit> &edit,
                 bool followTicks);

// A shop ghost's context in the candidate of the edit on the world, or none when the edit neither
// adds nor moves a shop. Changes nothing.
std::optional<ShopContext> shopContext(const World &world, const World &candidate,
                                       const ParkEdit &edit);
// As above, sampling the footfall with the sample and keeping the lists it works with in the
// buffers. Allocates nothing once they have grown.
std::optional<ShopContext> shopContext(const World &world, const World &candidate,
                                       const ParkEdit &edit, KeptFieldSample &footfall,
                                       TickBuffers &buffers);

} // namespace tpj

#endif
