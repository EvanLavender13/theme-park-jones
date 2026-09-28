#include "render/graph_overlay.h"

#include "render/math.h"
#include "render/park_mesh.h"
#include "render/picking.h"
#include "render/renderer.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tpj {
namespace {

// The projection runs in floats, so a ground point comes back from the window to within a
// centimeter at the tens of meters these views span.
constexpr double GROUND_TOLERANCE = 1e-2;
// Window positions are compared to a twentieth of a window unit: points near the near plane
// project far off the window, where float rounding of the depth moves them by thousandths.
constexpr double WINDOW_TOLERANCE = 5e-2;
// Node positions are compared to the positions stated below, which they equal but for rounding.
constexpr double NODE_TOLERANCE = 1e-6;

constexpr float WIDTH = 1280.0f;
constexpr float HEIGHT = 720.0f;

bool near(WindowPoint a, WindowPoint b) {
  return std::abs(a.X - b.X) <= WINDOW_TOLERANCE && std::abs(a.Y - b.Y) <= WINDOW_TOLERANCE;
}

bool near(GroundPoint a, GroundPoint b) {
  return std::abs(a.X - b.X) <= NODE_TOLERANCE && std::abs(a.Z - b.Z) <= NODE_TOLERANCE;
}

WindowPoint requirePoint(const CameraView &view, float width, float height, GroundPoint point) {
  const std::optional<WindowPoint> at = windowPoint(view, width, height, point);
  REQUIRE(at.has_value());
  return at.value_or(WindowPoint{});
}

// A view looking down at 45 degrees from (0, 10, 10) toward the origin. Its forward direction is
// (0, -1, -1) / sqrt(2), so a ground point at z has view depth (20 - z) / sqrt(2), and with NearZ 2
// the near plane meets the ground at z = 20 - 2 sqrt(2), about 17.17.
constexpr CameraView DOWNWARD{Vec3{0.0f, 10.0f, 10.0f}, Vec3{0.0f, 0.0f, 0.0f}, 0.9f, 2.0f,
                              2000.0f};

// Straight paths, so their ground lines lie on the lines drawn and every meeting follows from the
// geometry. Seen from DOWNWARD, z 0 and 10 lie in front of the near plane, z 18 in front of the eye
// but behind the near plane, and z 25 and 40 behind the eye.
//
// Guest path 1 runs from in front to behind, so it has segments wholly in front, one clipped at its
// second end, and segments wholly behind. Guest path 2 lies wholly in front and crosses path 1 at
// (-10, 10), a node. Backstage path 3 runs from behind to in front, so one of its segments is
// clipped at its first end; it crosses guest path 2 at (5, 10), where the kinds share no node.
// Backstage path 4 lies between the eye and the near plane at one end and behind the eye at the
// other, so it has no line.
constexpr std::string_view NEAR_PLANE_PARK = "tpj-park 1\nseed 1\ntick 0\nnext-key 5\n"
                                             "\n[path]\n"
                                             "1 kind=guest points=[{x=-10 z=0} {x=-10 z=40}]\n"
                                             "2 kind=guest points=[{x=-20 z=10} {x=20 z=10}]\n"
                                             "3 kind=backstage points=[{x=5 z=40} {x=5 z=-10}]\n"
                                             "4 kind=backstage points=[{x=15 z=18} {x=15 z=25}]\n";

World nearPlanePark() {
  World world = loadWorld(makeParkSchema(), NEAR_PLANE_PARK);
  resolveWorld(world);
  return world;
}

constexpr std::array<PathKind, 2> KINDS = {PathKind::Guest, PathKind::Backstage};

// A ground point's view depth: its offset from the eye along the unit direction to the target.
double viewDepth(const CameraView &view, const CarrierPoint &point) {
  const double fx = view.Target.X - view.Eye.X;
  const double fy = view.Target.Y - view.Eye.Y;
  const double fz = view.Target.Z - view.Eye.Z;
  const double length = std::sqrt(fx * fx + fy * fy + fz * fz);
  return ((point.X - view.Eye.X) * fx + (0.0 - view.Eye.Y) * fy + (point.Z - view.Eye.Z) * fz) /
         length;
}

// A carrier point's depth is compared with NearZ in floats by the code under test and in doubles
// here, so the fixture keeps every point well clear of the near plane.
void requireClearOfNearPlane(const World &world) {
  for (const PathKind kind : KINDS) {
    for (const Carrier &carrier : parkNetwork(world, kind).carriers()) {
      for (const CarrierPoint &point : carrier.Points) {
        REQUIRE(std::abs(viewDepth(DOWNWARD, point) - DOWNWARD.NearZ) > 1e-3);
      }
    }
  }
}

using LineName = std::tuple<PathKind, EntityKey, uint32_t>;

LineName nameOf(const GraphLine &line) { return {line.Kind, line.Carrier, line.Segment}; }

const Carrier *findCarrier(const Network &network, EntityKey key) {
  const auto found = std::ranges::find_if(
      network.carriers(), [key](const Carrier &carrier) { return carrier.Key == key; });
  return found == network.carriers().end() ? nullptr : &*found;
}

TEST_CASE("groundAtCursor at a window point's normalized device coordinates gives its ground "
          "point back") {
  struct Case {
    std::string_view Name;
    CameraView View;
    float Width = 0.0f;
    float Height = 0.0f;
    GroundPoint Point;
  };
  const std::array<Case, 3> cases = {{
      {"an oblique view in a wide window, a point off both axes",
       CameraView{Vec3{30.0f, 40.0f, 60.0f}, Vec3{5.0f, 0.0f, -10.0f}}, WIDTH, HEIGHT,
       GroundPoint{-8.0, 4.0}},
      // A tall window, whose aspect ratio must scale x alone.
      {"an oblique view in a tall window",
       CameraView{Vec3{30.0f, 40.0f, 60.0f}, Vec3{5.0f, 0.0f, -10.0f}}, 600.0f, 900.0f,
       GroundPoint{12.0, -15.0}},
      // A target above the ground, so the ground point is not on the line of sight.
      {"a view of a target in the air",
       CameraView{Vec3{-20.0f, 15.0f, -35.0f}, Vec3{10.0f, 2.0f, 5.0f}}, 1024.0f, 768.0f,
       GroundPoint{5.0, -5.0}},
  }};
  for (const Case &test : cases) {
    INFO(test.Name);
    const WindowPoint at = requirePoint(test.View, test.Width, test.Height, test.Point);
    const float ndcX = 2.0f * at.X / test.Width - 1.0f;
    const float ndcY = 1.0f - 2.0f * at.Y / test.Height;
    const std::optional<ParkPoint> back =
        groundAtCursor(test.View, test.Width / test.Height, ndcX, ndcY);
    REQUIRE(back.has_value());
    CHECK(std::abs(back.value_or(ParkPoint{}).X - test.Point.X) <= GROUND_TOLERANCE);
    CHECK(std::abs(back.value_or(ParkPoint{}).Z - test.Point.Z) <= GROUND_TOLERANCE);
  }
}

TEST_CASE("windowPoint gives none exactly when the view depth is not positive or the window has "
          "no area") {
  // A level view along -z from z = 20, so a ground point at z has view depth 20 - z.
  const CameraView level{Vec3{0.0f, 10.0f, 20.0f}, Vec3{0.0f, 10.0f, 0.0f}};
  const GroundPoint ahead{3.0, 0.0};

  SECTION("a point behind the eye") {
    CHECK_FALSE(windowPoint(level, WIDTH, HEIGHT, GroundPoint{3.0, 30.0}).has_value());
  }
  SECTION("a point at view depth zero") {
    CHECK_FALSE(windowPoint(level, WIDTH, HEIGHT, GroundPoint{3.0, 20.0}).has_value());
  }
  // The near plane plays no part: only the sign of the depth does.
  SECTION("a point at a positive view depth below NearZ") {
    REQUIRE(level.NearZ > 0.05f);
    CHECK(windowPoint(level, WIDTH, HEIGHT, GroundPoint{3.0, 19.95}).has_value());
  }
  SECTION("a point ahead in a window of positive size") {
    CHECK(windowPoint(level, WIDTH, HEIGHT, ahead).has_value());
  }
  SECTION("a window with no width or no height") {
    CHECK_FALSE(windowPoint(level, 0.0f, HEIGHT, ahead).has_value());
    CHECK_FALSE(windowPoint(level, WIDTH, 0.0f, ahead).has_value());
    CHECK_FALSE(windowPoint(level, -WIDTH, HEIGHT, ahead).has_value());
    CHECK_FALSE(windowPoint(level, WIDTH, -HEIGHT, ahead).has_value());
  }
}

TEST_CASE("buildGraphOverlay has one line for each carrier segment with an end at or past the "
          "near plane, and no others") {
  const World world = nearPlanePark();
  requireClearOfNearPlane(world);

  std::vector<LineName> expected;
  size_t clipped = 0;
  size_t hidden = 0;
  for (const PathKind kind : KINDS) {
    const std::vector<Carrier> &carriers = parkNetwork(world, kind).carriers();
    // Each path of the fixture is a carrier of its kind.
    REQUIRE(carriers.size() == 2);
    for (const Carrier &carrier : carriers) {
      for (size_t i = 0; i + 1 < carrier.Points.size(); ++i) {
        const double first = viewDepth(DOWNWARD, carrier.Points[i]);
        const double second = viewDepth(DOWNWARD, carrier.Points[i + 1]);
        if (std::max(first, second) >= DOWNWARD.NearZ) {
          expected.emplace_back(kind, carrier.Key, static_cast<uint32_t>(i));
          clipped += std::min(first, second) < DOWNWARD.NearZ ? 1 : 0;
        } else {
          ++hidden;
        }
      }
    }
  }
  // Paths 1 and 3 each cross the near plane, and paths 1 and 4 have segments wholly behind it.
  REQUIRE(clipped == 2);
  REQUIRE(hidden > 0);

  const GraphOverlay overlay = buildGraphOverlay(world, DOWNWARD, WIDTH, HEIGHT);
  std::vector<LineName> names;
  names.reserve(overlay.Lines.size());
  for (const GraphLine &line : overlay.Lines) {
    names.push_back(nameOf(line));
  }
  std::ranges::sort(names);
  std::ranges::sort(expected);
  CHECK(names == expected);
}

TEST_CASE("A line runs between windowPoint of its segment's ends, an end behind the near plane "
          "replaced by the segment's point on it") {
  const World world = nearPlanePark();
  requireClearOfNearPlane(world);
  const GraphOverlay overlay = buildGraphOverlay(world, DOWNWARD, WIDTH, HEIGHT);

  // The end as drawn: itself in front of the near plane, and otherwise the segment's point at view
  // depth NearZ, found along the segment toward the other end.
  const auto drawnEnd = [](const CarrierPoint &end, const CarrierPoint &other) {
    const double depth = viewDepth(DOWNWARD, end);
    if (depth >= DOWNWARD.NearZ) {
      return requirePoint(DOWNWARD, WIDTH, HEIGHT, GroundPoint{end.X, end.Z});
    }
    const double t = (DOWNWARD.NearZ - depth) / (viewDepth(DOWNWARD, other) - depth);
    return requirePoint(DOWNWARD, WIDTH, HEIGHT,
                        GroundPoint{end.X + t * (other.X - end.X), end.Z + t * (other.Z - end.Z)});
  };

  size_t whole = 0;
  size_t clippedFirst = 0;
  size_t clippedSecond = 0;
  for (const GraphLine &line : overlay.Lines) {
    INFO("carrier " << static_cast<uint64_t>(line.Carrier) << ", segment " << line.Segment);
    const Carrier *carrier = findCarrier(parkNetwork(world, line.Kind), line.Carrier);
    REQUIRE(carrier != nullptr);
    REQUIRE(line.Segment + size_t{1} < carrier->Points.size());
    const CarrierPoint &first = carrier->Points[line.Segment];
    const CarrierPoint &second = carrier->Points[line.Segment + 1];
    const bool firstBehind = viewDepth(DOWNWARD, first) < DOWNWARD.NearZ;
    const bool secondBehind = viewDepth(DOWNWARD, second) < DOWNWARD.NearZ;
    whole += !firstBehind && !secondBehind ? 1 : 0;
    clippedFirst += firstBehind ? 1 : 0;
    clippedSecond += secondBehind ? 1 : 0;
    CHECK(near(line.From, drawnEnd(first, second)));
    CHECK(near(line.To, drawnEnd(second, first)));
  }
  // Path 3 runs from behind the near plane to in front of it, and path 1 the other way.
  CHECK(whole > 0);
  CHECK(clippedFirst == 1);
  CHECK(clippedSecond == 1);
}

TEST_CASE("buildGraphOverlay has one node at windowPoint of each node's ground position at or "
          "past the near plane, and no others") {
  const World world = nearPlanePark();
  struct KindNodes {
    PathKind Kind;
    uint32_t Count = 0;
    std::vector<GroundPoint> InFront;
  };
  // Every path end is a node, and guest paths 1 and 2 cross at (-10, 10). Of the ends, those at
  // z 40, 25, and 18 lie behind the near plane.
  const std::array<KindNodes, 2> kinds = {{
      {PathKind::Guest, 5, {{-10.0, 0.0}, {-10.0, 10.0}, {-20.0, 10.0}, {20.0, 10.0}}},
      {PathKind::Backstage, 4, {{5.0, -10.0}}},
  }};

  const GraphOverlay overlay = buildGraphOverlay(world, DOWNWARD, WIDTH, HEIGHT);
  std::vector<std::pair<PathKind, uint32_t>> expected;
  for (const KindNodes &kind : kinds) {
    const Network &network = parkNetwork(world, kind.Kind);
    REQUIRE(network.nodeCount() == kind.Count);
    size_t found = 0;
    for (uint32_t node = 0; node < network.nodeCount(); ++node) {
      const std::optional<GroundPoint> position = network.groundPoint(network.nodePlace(node));
      REQUIRE(position.has_value());
      const GroundPoint ground = position.value_or(GroundPoint{});
      const bool inFront = std::ranges::any_of(
          kind.InFront, [ground](GroundPoint point) { return near(ground, point); });
      const auto drawn = std::ranges::find_if(overlay.Nodes, [&kind, node](const GraphNode &at) {
        return at.Kind == kind.Kind && at.Node == node;
      });
      INFO("node " << node << " at (" << ground.X << ", " << ground.Z << ")");
      if (inFront) {
        ++found;
        expected.emplace_back(kind.Kind, node);
        REQUIRE(drawn != overlay.Nodes.end());
        CHECK(near(drawn->At, requirePoint(DOWNWARD, WIDTH, HEIGHT, ground)));
      }
    }
    REQUIRE(found == kind.InFront.size());
  }

  std::vector<std::pair<PathKind, uint32_t>> named;
  named.reserve(overlay.Nodes.size());
  for (const GraphNode &node : overlay.Nodes) {
    named.emplace_back(node.Kind, node.Node);
  }
  std::ranges::sort(named);
  std::ranges::sort(expected);
  CHECK(named == expected);
}

TEST_CASE("buildGraphOverlay gives an empty overlay for a window with no width or no height") {
  const World world = nearPlanePark();
  const GraphOverlay drawn = buildGraphOverlay(world, DOWNWARD, WIDTH, HEIGHT);
  REQUIRE_FALSE(drawn.Lines.empty());
  REQUIRE_FALSE(drawn.Nodes.empty());
  for (const auto &[width, height] :
       {std::pair{0.0f, HEIGHT}, std::pair{WIDTH, 0.0f}, std::pair{-WIDTH, HEIGHT}}) {
    INFO("width " << width << ", height " << height);
    const GraphOverlay overlay = buildGraphOverlay(world, DOWNWARD, width, height);
    CHECK(overlay.Lines.empty());
    CHECK(overlay.Nodes.empty());
  }
}

TEST_CASE("The graph colors are opaque and differ from each other and from both paths' colors") {
  const std::array<Rgba, 5> graph = {graphColor(PathKind::Guest), graphColor(PathKind::Backstage),
                                     GRAPH_NODE_COLOR, GRAPH_CONNECTOR_COLOR, GRAPH_ANCHOR_COLOR};
  const std::array<Rgba, 2> paths = {pathColor(PathKind::Guest), pathColor(PathKind::Backstage)};
  const auto differs = [](Rgba a, Rgba b) { return a.R != b.R || a.G != b.G || a.B != b.B; };
  for (size_t i = 0; i < graph.size(); ++i) {
    INFO("graph color " << i);
    CHECK(graph[i].A == 1.0f);
    for (size_t j = i + 1; j < graph.size(); ++j) {
      CHECK(differs(graph[i], graph[j]));
    }
    for (const Rgba &path : paths) {
      CHECK(differs(graph[i], path));
    }
  }
}

// A guest path along z = 0 and a backstage path along z = -12, with a shop between them facing +z:
// its front door, (0, -3), connects to the guest path and its back door, (0, -9), to the
// backstage path. Seen from DOWNWARD, all of it lies in front of the near plane.
constexpr std::string_view CONNECTED_PARK = "tpj-park 1\nseed 1\ntick 0\nnext-key 4\n"
                                            "\n[path]\n"
                                            "1 kind=guest points=[{x=-10 z=0} {x=10 z=0}]\n"
                                            "2 kind=backstage points=[{x=-10 z=-12} {x=10 z=-12}]\n"
                                            "\n[box]\n"
                                            "3 kind=shop x=0 z=-6 facing-x=0 facing-z=1\n";

World connectedPark() {
  World world = loadWorld(makeParkSchema(), CONNECTED_PARK);
  resolveWorld(world);
  return world;
}

TEST_CASE("A line is marked a connector exactly when its carrier is not the key of a path") {
  const World world = connectedPark();
  std::vector<EntityKey> paths;
  for (const ParkPath &path : parkPaths(world)) {
    paths.push_back(path.Key);
  }
  const GraphOverlay overlay = buildGraphOverlay(world, DOWNWARD, WIDTH, HEIGHT);
  size_t connectors = 0;
  for (const GraphLine &line : overlay.Lines) {
    INFO("carrier " << static_cast<uint64_t>(line.Carrier) << ", segment " << line.Segment);
    const bool path = std::ranges::find(paths, line.Carrier) != paths.end();
    CHECK(line.Connector == !path);
    connectors += line.Connector ? 1 : 0;
  }
  // The shop's two connectors, one straight segment each, beside the paths' segments.
  CHECK(connectors == 2);
  CHECK(overlay.Lines.size() > connectors);
}

TEST_CASE("Each graph node holds nodeAnchor of its node as its anchor") {
  const World world = connectedPark();
  const GraphOverlay overlay = buildGraphOverlay(world, DOWNWARD, WIDTH, HEIGHT);
  size_t anchored = 0;
  for (const GraphNode &node : overlay.Nodes) {
    INFO("kind " << static_cast<int>(node.Kind) << ", node " << node.Node);
    CHECK(node.Anchor == parkNetwork(world, node.Kind).nodeAnchor(node.Node));
    anchored += node.Anchor != NULL_KEY ? 1 : 0;
  }
  // The shop's two doors, one in each network.
  CHECK(anchored == 2);
  CHECK(overlay.Nodes.size() > anchored);
}

// The overlay is drawn every frame the checkbox is on, so it must leave nothing behind in what is
// saved or hashed.
TEST_CASE("Building a world's graph overlay leaves its save and hash unchanged") {
  const World world = nearPlanePark();
  const std::string save = saveWorld(world);
  const uint64_t hash = hashWorld(world);
  const GraphOverlay overlay = buildGraphOverlay(world, DOWNWARD, WIDTH, HEIGHT);
  REQUIRE_FALSE(overlay.Lines.empty());
  CHECK(saveWorld(world) == save);
  CHECK(hashWorld(world) == hash);
}

} // namespace
} // namespace tpj
