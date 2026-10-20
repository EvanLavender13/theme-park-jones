#ifndef TPJ_SIM_PARK_UNIT_READS_H
#define TPJ_SIM_PARK_UNIT_READS_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
#include "sim/schema.h"
#include "sim/tick_buffers.h"
#include "sim/units.h"
#include "sim/world.h"

#include <optional>
#include <span>
#include <vector>

namespace tpj {

// Unit kinds park keeps, which another module's unit kinds can be kept over with UnitsOf: one unit
// for each box, each entrance, each path point, and each path segment. Their values are read only
// through the functions below.
struct BoxUnits;
struct EntranceUnits;
struct PointPavingUnits;
struct SegmentPavingUnits;

// Registers park-boxes and park-entrances, their cell indexes box-cells and entrance-cells, and
// segment-furnishings. makeParkSchema calls it after addPavedSurface.
void addParkUnits(WorldSchema &schema);

// A point's edge lines before hiding, and the positions they run through.
struct EdgeRun {
  std::span<const EdgeLine> Edges;
  std::span<const ParkPoint> Points;
};

// Each of these reads park's units for another module's unit derivation, through its reads, so that
// the derivation runs again when what it read changes.

// The box the key holds, as parkBox gives it, or null when it holds none.
const ParkBox *boxOf(UnitReads &reads, EntityKey key);
// The entrance the key holds, as parkEntrances gives it, or null when it holds none.
const ParkEntrance *entranceOf(UnitReads &reads, EntityKey key);
// The segment the key holds, as parkSegments gives it without its furnishing, or null.
const ParkSegment *groundSegmentOf(UnitReads &reads, EntityKey key);
// The keys of the segments that end at the point, ascending.
std::span<const EntityKey> segmentsAt(UnitReads &reads, EntityKey point);
// Puts in pieces, clearing it first, every piece of the paved surface whose corners' bounding
// rectangle meets the rectangle from least to most. It can also put in other pieces of the paved
// surface, and a piece more than once.
void pavingNear(UnitReads &reads, ParkPoint least, ParkPoint most,
                std::vector<const SurfacePiece *> &pieces);
// The point's edge lines before hiding, or empty spans when it has none.
EdgeRun pointEdgeLines(UnitReads &reads, EntityKey point);
// Appends to the out lists the parts of the edge lines, whose First counts from the start of
// points, that no piece joined to the owner hides: no piece of the paved surface, and none of the
// extra pieces. A piece is joined to the owner when both lie in one group of points joined by
// segments with ground lines. It gives what hiding the paved surface gives for these edge lines in
// a surface that also holds the extra pieces. Nothing when the owner is in no group.
void hideJoined(UnitReads &reads, EntityKey owner, std::span<const EdgeLine> edges,
                std::span<const ParkPoint> points, std::span<const SurfacePiece> extra,
                std::vector<EdgeLine> &outEdges, std::vector<ParkPoint> &outPoints);
// Appends the keys of the cells of a fixed grid that the rectangle from least to most covers, so
// two rectangles that meet share a key.
void pavingCells(ParkPoint least, ParkPoint most, std::vector<EntityKey> &cells);

// Each of these reads the same units for a resolver, outside a unit derivation, keeping the lists
// it works with in the buffers. None allocates once its lists have grown.

// Fills the segment the key holds, as parkSegments gives it, into a segment the caller keeps. False
// when the key holds no segment.
bool parkSegment(const World &world, EntityKey key, ParkSegment &segment);
// The path point's position, or none when the key holds no path point.
std::optional<ParkPoint> pathPointAt(const World &world, EntityKey point);
// Puts in pieces, clearing it first, every piece of the paved surface whose bounds meet the
// rectangle from least to most, as the UnitReads form gives them.
void pavingNear(const World &world, ParkPoint least, ParkPoint most,
                std::vector<const SurfacePiece *> &pieces, TickBuffers &buffers);
// Puts in boxes, clearing it first, each box whose footprint's bounds meet the rectangle, ascending
// by key, once each. It can also put in other boxes.
void boxesNear(const World &world, ParkPoint least, ParkPoint most,
               std::vector<const ParkBox *> &boxes, TickBuffers &buffers);
// The same for entrances.
void entrancesNear(const World &world, ParkPoint least, ParkPoint most,
                   std::vector<const ParkEntrance *> &entrances, TickBuffers &buffers);
// Adds to the segments, keeping the list ascending without repeats, each segment whose furnishing,
// paving, or ground line the running resolution changed.
void changedSegments(const World &world, std::vector<EntityKey> &segments);
// Adds to the cells, keeping the list ascending without repeats, the cells of pavingCells' grid
// where the running resolution added, removed, or changed a piece of the paved surface, a box, or
// an entrance.
void changedParkCells(const World &world, std::vector<EntityKey> &cells, TickBuffers &buffers);

} // namespace tpj

#endif
