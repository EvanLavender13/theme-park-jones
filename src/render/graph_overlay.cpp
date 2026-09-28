#include "render/graph_overlay.h"

#include "render/math.h"
#include "sim/routes/networks.h"

#include <cstddef>

namespace tpj {

namespace {

constexpr Vec3 UP{0.0f, 1.0f, 0.0f};

// dot(p - Eye, f), with p the point at height 0 and f the unit direction from Eye to Target.
float viewDepth(const CameraView &view, GroundPoint point) {
  const Vec3 ground{static_cast<float>(point.X), 0.0f, static_cast<float>(point.Z)};
  return dot(ground - view.Eye, normalize(view.Target - view.Eye));
}

// The point of a segment at view depth nearZ, between its end behind that depth and its end at or
// past it.
GroundPoint pointAtDepth(GroundPoint behind, GroundPoint ahead, float behindDepth, float aheadDepth,
                         float nearZ) {
  const double t =
      static_cast<double>(nearZ - behindDepth) / static_cast<double>(aheadDepth - behindDepth);
  return {behind.X + (t * (ahead.X - behind.X)), behind.Z + (t * (ahead.Z - behind.Z))};
}

// Adds a line for each segment of the network's carriers with a part in front of the near plane.
void appendLines(GraphOverlay &overlay, const Network &network, PathKind kind,
                 const CameraView &view, float width, float height) {
  for (const Carrier &carrier : network.carriers()) {
    for (std::size_t index = 0; index + 1 < carrier.Points.size(); ++index) {
      GroundPoint a{carrier.Points[index].X, carrier.Points[index].Z};
      GroundPoint b{carrier.Points[index + 1].X, carrier.Points[index + 1].Z};
      const float depthA = viewDepth(view, a);
      const float depthB = viewDepth(view, b);
      if (depthA < view.NearZ && depthB < view.NearZ) {
        continue;
      }
      if (depthA < view.NearZ) {
        a = pointAtDepth(a, b, depthA, depthB, view.NearZ);
      } else if (depthB < view.NearZ) {
        b = pointAtDepth(b, a, depthB, depthA, view.NearZ);
      }
      const std::optional<WindowPoint> from = windowPoint(view, width, height, a);
      const std::optional<WindowPoint> to = windowPoint(view, width, height, b);
      if (from && to) {
        overlay.Lines.push_back(
            GraphLine{kind, carrier.Key, static_cast<uint32_t>(index), *from, *to});
      }
    }
  }
}

// Adds a node for each of the network's nodes whose ground position lies in front of the near
// plane.
void appendNodes(GraphOverlay &overlay, const Network &network, PathKind kind,
                 const CameraView &view, float width, float height) {
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    const std::optional<GroundPoint> ground = network.groundPoint(network.nodePlace(node));
    if (!ground || viewDepth(view, *ground) < view.NearZ) {
      continue;
    }
    if (const std::optional<WindowPoint> at = windowPoint(view, width, height, *ground)) {
      overlay.Nodes.push_back(GraphNode{kind, node, *at});
    }
  }
}

} // namespace

std::optional<WindowPoint> windowPoint(const CameraView &view, float width, float height,
                                       GroundPoint point) {
  if (!(width > 0.0f) || !(height > 0.0f) || !(viewDepth(view, point) > 0.0f)) {
    return std::nullopt;
  }
  const Mat4 viewProjection =
      multiply(perspective(view.FovY, width / height, view.NearZ, view.FarZ),
               lookAt(view.Eye, view.Target, UP));
  const auto &m = viewProjection.M;
  const auto x = static_cast<float>(point.X);
  const auto z = static_cast<float>(point.Z);
  // Column-major: element (row, col) is M[col * 4 + row], and the point is (x, 0, z, 1).
  const auto row = [&m, x, z](int r) { return (m[r] * x) + (m[8 + r] * z) + m[12 + r]; };
  const float w = row(3);
  return WindowPoint{((row(0) / w) + 1.0f) * 0.5f * width, (1.0f - (row(1) / w)) * 0.5f * height};
}

GraphOverlay buildGraphOverlay(const World &world, const CameraView &view, float width,
                               float height) {
  GraphOverlay overlay;
  if (!(width > 0.0f) || !(height > 0.0f)) {
    return overlay;
  }
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    appendLines(overlay, parkNetwork(world, kind), kind, view, width, height);
    appendNodes(overlay, parkNetwork(world, kind), kind, view, width, height);
  }
  return overlay;
}

} // namespace tpj
