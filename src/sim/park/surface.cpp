#include "sim/park/surface.h"

#include "sim/park/internal/ground_units.h"
#include "sim/park/internal/layout.h"
#include "sim/park/internal/paving.h"
#include "sim/park/internal/surface_units.h"
#include "sim/schema.h"
#include "sim/tick_buffers.h"

#include <cmath>
#include <span>
#include <stdint.h>
#include <vector>

namespace tpj {

void clearPaving(OwnerPaving &paving) {
  paving.Pieces.clear();
  paving.Edges.clear();
  paving.EdgePoints.clear();
  paving.Sides.clear();
  paving.SidePoints.clear();
}

void beginRefill(PavedSurface &surface) {
  for (auto &[owner, paving] : surface.Owners) {
    clearPaving(paving);
  }
}

void endRefill(PavedSurface &surface) {
  surface.Owners.eraseIf([](const OwnerPaving &paving) {
    return paving.Pieces.empty() && paving.Edges.empty() && paving.Sides.empty();
  });
}

OwnerPaving &pavingFor(PavedSurface &surface, EntityKey owner) {
  auto [paving, added] = surface.Owners.getOrAdd(owner);
  if (added) {
    clearPaving(paving);
  }
  return paving;
}

const OwnerPaving *pavingOf(const PavedSurface &surface, EntityKey owner) {
  return surface.Owners.find(owner);
}

PavedSurface parkSurface(const World &world) {
  TickBuffers buffers;
  return parkSurface(world, buffers);
}

PavedSurface parkSurface(const World &world, TickBuffers &buffers) {
  GroundLists &lists = buffers.get<GroundLists>();
  worldGround(world, lists);
  PavedSurface surface;
  surfaceOf(lists.Layout, lists.Ground, surface, buffers);
  return surface;
}

const PavedSurface &resolvedSurface(const World &world) {
  static const PavedSurface EMPTY;
  const PavedSurface *surface = world.tryGet<PavedSurface>(pavedSurfaceKey());
  return surface != nullptr ? *surface : EMPTY;
}

void addPavedSurface(WorldSchema &schema) {
  schema.addComponent<PavedSurface>("paved-surface", DataKind::Derived);
  addSurfaceUnits(schema);
}

PavedSurface parkSurface(const World &world, std::span<const Walkway> walkways) {
  TickBuffers buffers;
  return parkSurface(world, walkways, buffers);
}

PavedSurface parkSurface(const World &world, std::span<const Walkway> walkways,
                         TickBuffers &buffers) {
  GroundLists &lists = buffers.get<GroundLists>();
  worldGround(world, lists);
  PavedSurface surface;
  walkedSurfaceOf(lists.Layout, lists.Ground, walkways, surface, buffers);
  return surface;
}

std::vector<ParkTaper> parkTapers(const World &world) {
  EdgeLists lists;
  taperCandidates(world, lists.CandidateEnds, lists.Candidates);
  std::vector<ParkTaper> tapers;
  if (lists.Candidates.empty()) {
    return tapers;
  }
  GroundLists ground;
  worldGround(world, ground);
  fitTapers(ground.Layout, ground.Ground, lists.Candidates, lists, tapers);
  return tapers;
}

void parkTapers(const World &world, std::vector<ParkTaper> &tapers, TickBuffers &buffers) {
  EdgeLists &lists = buffers.get<EdgeLists>();
  taperCandidates(world, lists.CandidateEnds, lists.Candidates);
  tapers.clear();
  if (lists.Candidates.empty()) {
    return;
  }
  GroundLists &ground = buffers.get<GroundLists>();
  worldGround(world, ground);
  fitTapers(ground.Layout, ground.Ground, lists.Candidates, lists, tapers);
}

std::vector<ParkEdge> parkEdges(const World &world) {
  GroundLists ground;
  worldGround(world, ground);
  return edgesOf(ground.Layout, ground.Ground);
}

void parkEdges(const World &world, std::vector<ParkEdge> &edges, TickBuffers &buffers) {
  GroundLists &ground = buffers.get<GroundLists>();
  worldGround(world, ground);
  fitEdges(ground.Layout, ground.Ground, buffers.get<EdgeLists>(), edges);
}

double edgeRadiusFor(const ParkEdge &edge, double distance) {
  // The middle lies 1 / sin(g / 2) - 1 farther from where the sides meet for each meter of radius,
  // where g is the turn from one segment to the other.
  const double halfSine = std::sqrt((1.0 - dot(edge.Along[0], edge.Along[1])) / 2.0);
  const double perRadius = halfSine > 0.0 ? 1.0 / halfSine - 1.0 : 0.0;
  if (!(distance > 0.0) || !(perRadius > 0.0)) {
    return 0.0;
  }
  return distance / perRadius;
}

} // namespace tpj
