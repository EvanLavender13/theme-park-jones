#include "app/graph_view.h"
#include "render/graph_overlay.h"

#include <imgui.h>

namespace tpj {
namespace {

ImU32 imColor(Rgba color) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(color.R, color.G, color.B, color.A));
}

} // namespace

void drawGraphView(const World &world, const CameraView &view) {
  const ImVec2 size = ImGui::GetIO().DisplaySize;
  const GraphOverlay overlay = buildGraphOverlay(world, view, size.x, size.y);
  ImDrawList *drawList = ImGui::GetBackgroundDrawList();
  for (const GraphLine &line : overlay.Lines) {
    const Rgba color = line.Connector ? GRAPH_CONNECTOR_COLOR : graphColor(line.Kind);
    drawList->AddLine(ImVec2(line.From.X, line.From.Y), ImVec2(line.To.X, line.To.Y),
                      imColor(color), GRAPH_LINE_THICKNESS);
  }
  for (const GraphNode &node : overlay.Nodes) {
    const bool anchored = node.Anchor != NULL_KEY;
    drawList->AddCircleFilled(ImVec2(node.At.X, node.At.Y),
                              anchored ? GRAPH_ANCHOR_RADIUS : GRAPH_NODE_RADIUS,
                              imColor(anchored ? GRAPH_ANCHOR_COLOR : GRAPH_NODE_COLOR));
  }
}

} // namespace tpj
