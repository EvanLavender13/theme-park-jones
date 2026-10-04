#include "render/band_overlay.h"

#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"

#include <algorithm>
#include <math.h>
#include <numbers>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// A sample of a band's line: where it stands, the line's direction there, and its color.
struct BandRow {
  GroundPoint At;
  ParkPoint Direction;
  Rgba Color;
};

void addVertex(ParkMesh &mesh, double x, float y, double z, Rgba color) {
  ParkVertex vertex;
  vertex.Position[0] = static_cast<float>(x);
  vertex.Position[1] = y;
  vertex.Position[2] = static_cast<float>(z);
  vertex.Normal[1] = 1.0f;
  vertex.Color = color;
  mesh.Vertices.push_back(vertex);
}

// The unit direction from one line point to the next. A carrier's points are distinct.
ParkPoint unitStep(const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double length = sqrt(dx * dx + dz * dz);
  return {dx / length, dz / length};
}

// Fills distances with those the band samples along a carrier: its points', its stops', and each
// positive multiple of OVERLAY_SPACING below its length, ascending, each once.
void sampleDistances(const Carrier &carrier, std::vector<double> &distances) {
  const double length = carrier.Points.back().Distance;
  distances.clear();
  for (const CarrierPoint &point : carrier.Points) {
    distances.push_back(point.Distance);
  }
  for (const CarrierStop &stop : carrier.Stops) {
    distances.push_back(stop.Distance);
  }
  for (uint64_t k = 1; static_cast<double>(k) * OVERLAY_SPACING < length; ++k) {
    distances.push_back(static_cast<double>(k) * OVERLAY_SPACING);
  }
  std::ranges::sort(distances);
  const auto [first, last] = std::ranges::unique(distances);
  distances.erase(first, last);
}

// The line's direction at a distance along it: its first segment's at the first point, its last
// segment's at the last, the two segments' averaged at a point between, as a ribbon's tangent is,
// and elsewhere the direction of the segment holding it.
ParkPoint directionAt(const std::vector<CarrierPoint> &points, double distance) {
  const auto after = std::ranges::lower_bound(points, distance, {}, &CarrierPoint::Distance);
  const auto i = static_cast<size_t>(after - points.begin());
  if (i == 0) {
    return unitStep(points[0], points[1]);
  }
  const ParkPoint before = unitStep(points[i - 1], points[i]);
  if (points[i].Distance != distance || i + 1 == points.size()) {
    return before;
  }
  const ParkPoint next = unitStep(points[i], points[i + 1]);
  const ParkPoint sum{before.X + next.X, before.Z + next.Z};
  const double length = sqrt(sum.X * sum.X + sum.Z * sum.Z);
  if (length > 0.0) {
    return {sum.X / length, sum.Z / length};
  }
  return before;
}

// Adds a row of three vertices for each sample, left edge, center, and right edge, and joins each
// row to the next with four triangles facing up.
void appendBand(ParkMesh &mesh, std::span<const BandRow> rows) {
  const auto first = static_cast<uint32_t>(mesh.Vertices.size());
  for (const BandRow &row : rows) {
    const ParkPoint right{-row.Direction.Z, row.Direction.X};
    addVertex(mesh, row.At.X - right.X * OVERLAY_BAND, OVERLAY_EDGE_LIFT,
              row.At.Z - right.Z * OVERLAY_BAND, row.Color);
    addVertex(mesh, row.At.X, OVERLAY_TOP_LIFT, row.At.Z, row.Color);
    addVertex(mesh, row.At.X + right.X * OVERLAY_BAND, OVERLAY_EDGE_LIFT,
              row.At.Z + right.Z * OVERLAY_BAND, row.Color);
  }
  for (uint32_t i = 0; i + 1 < rows.size(); ++i) {
    const uint32_t left0 = first + 3 * i;
    const uint32_t center0 = left0 + 1;
    const uint32_t right0 = left0 + 2;
    const uint32_t left1 = left0 + 3;
    const uint32_t center1 = left0 + 4;
    const uint32_t right1 = left0 + 5;
    mesh.Indices.insert(mesh.Indices.end(), {left0, center0, left1, left1, center0, center1,
                                             center0, right0, center1, center1, right0, right1});
  }
}

// Adds the cone beyond a line's end, for the unit direction pointing away from the line: its
// center on the line at the top lift, and a half circle of the band's width at the edge lift.
void appendCone(ParkMesh &mesh, const BandRow &end, ParkPoint direction) {
  const ParkPoint right{-direction.Z, direction.X};
  const auto center = static_cast<uint32_t>(mesh.Vertices.size());
  addVertex(mesh, end.At.X, OVERLAY_TOP_LIFT, end.At.Z, end.Color);
  for (uint32_t k = 0; k <= JOINT_SEGMENTS; ++k) {
    const double angle = std::numbers::pi * static_cast<double>(k) / JOINT_SEGMENTS;
    const double across = OVERLAY_BAND * cos(angle);
    const double along = OVERLAY_BAND * sin(angle);
    addVertex(mesh, end.At.X + across * right.X + along * direction.X, OVERLAY_EDGE_LIFT,
              end.At.Z + across * right.Z + along * direction.Z, end.Color);
  }
  for (uint32_t k = 0; k < JOINT_SEGMENTS; ++k) {
    mesh.Indices.insert(mesh.Indices.end(), {center, center + 1 + k, center + 2 + k});
  }
}

// The ramp's color for a value, reaching its top at full, or OVERLAY_ZERO_COLOR for a value not
// above 0.
Rgba rampColor(double value, double full) {
  if (isnan(value) || value <= 0.0) {
    return OVERLAY_ZERO_COLOR;
  }
  const double scaled = std::min(value / full, 1.0) * static_cast<double>(OVERLAY_RAMP.size() - 1);
  const size_t index = std::min(static_cast<size_t>(scaled), OVERLAY_RAMP.size() - 2);
  const auto fraction = static_cast<float>(scaled - static_cast<double>(index));
  const Rgba &low = OVERLAY_RAMP[index];
  const Rgba &high = OVERLAY_RAMP[index + 1];
  return {low.R + (high.R - low.R) * fraction, low.G + (high.G - low.G) * fraction,
          low.B + (high.B - low.B) * fraction, 1.0f};
}

} // namespace

ParkMesh buildBandOverlay(const World &world, const OverlayValues &values, double full) {
  std::vector<EntityKey> guestPaths;
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind == PathKind::Guest) {
      guestPaths.push_back(path.Key);
    }
  }
  const Network &network = parkNetwork(world, PathKind::Guest);
  // Each band line's carrier and its rows' range in rows, colored once every row's value is known.
  struct BandLine {
    const Carrier *Line = nullptr;
    size_t First = 0;
    size_t Count = 0;
  };
  std::vector<BandLine> lines;
  std::vector<BandRow> rows;
  std::vector<Place> places;
  std::vector<double> distances;
  // Carriers come in key order, and a connector's key is no path's.
  for (const Carrier &carrier : network.carriers()) {
    if (!std::ranges::binary_search(guestPaths, carrier.Key)) {
      continue;
    }
    sampleDistances(carrier, distances);
    const size_t first = rows.size();
    for (const double distance : distances) {
      const Place place{carrier.Key, distance};
      const std::optional<GroundPoint> at = network.groundPoint(place);
      if (!at) {
        continue;
      }
      rows.push_back({*at, directionAt(carrier.Points, distance), OVERLAY_ZERO_COLOR});
      places.push_back(place);
    }
    if (rows.size() > first) {
      lines.push_back({&carrier, first, rows.size() - first});
    }
  }
  const std::vector<double> shades = values(places);
  for (size_t i = 0; i < rows.size(); ++i) {
    rows[i].Color = rampColor(shades.at(i), full);
  }
  ParkMesh mesh;
  // Three vertices and twelve indices a row, and two cones a line.
  mesh.Vertices.reserve(3 * rows.size() + 2 * lines.size() * (JOINT_SEGMENTS + 2));
  mesh.Indices.reserve(12 * rows.size() + 2 * lines.size() * 3 * JOINT_SEGMENTS);
  for (const BandLine &line : lines) {
    const std::span<const BandRow> lineRows(rows.data() + line.First, line.Count);
    const std::vector<CarrierPoint> &points = line.Line->Points;
    appendBand(mesh, lineRows);
    appendCone(mesh, lineRows.front(), unitStep(points[1], points[0]));
    appendCone(mesh, lineRows.back(), unitStep(points[points.size() - 2], points.back()));
  }
  return mesh;
}

} // namespace tpj
