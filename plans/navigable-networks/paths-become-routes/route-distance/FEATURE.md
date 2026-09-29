# Feature: Route Distance

## Summary

The routes module publishes the route distance field, which gives every place on a network its distance along the network to each anchored entity and the first step of a shortest route there (principle 4). It is two entry fields, one per network kind, guest-route-distance and backstage-route-distance, so a shop's guest and backstage entries never mix. routeEntries computes a source's entry at every node of a network: its least route length, with lengths added in turn from the source's end, and its next step, a carrier and two of its stop distances, chosen among the steps that achieve the distance by carrier key, then direction, then stop. A resolver, route-distance, publishes each anchored entity's entries at its nodes' places into its network's field, and the field's sampleEdge gives a place inside an edge the better of the edge's two ends. So a guest can sample the distance to every shop it can reach, and a mover that follows next steps arrives at the source having walked exactly the distance it sampled. Nothing consumes the fields yet except their tests.

## Acceptance criteria

1. routeEntries(network, source) has one element per node. A node has an entry exactly when some route joins it to a node anchored to the source, and its Distance is the least, over the routes starting at one of those anchored nodes and ending at it, of the route's edge lengths added in turn from the anchored end, starting from 0.0. A route is a sequence of edges, each sharing a node with the next and walked in either direction, and an edge's length is its ToDistance minus its FromDistance. So a source's anchored nodes have distance 0.0, and a source anchoring no node of the network has no entries.
2. At a source's anchored node the entry's Next is RouteStep{}, carrier NULL_KEY. At every other node with an entry, Next is the step along an edge at the node, from its stop there (From) to the edge's other stop (To), whose other node's distance plus the edge's length equals the node's distance exactly. When several steps do, it is the one with the lowest carrier key, then one toward lower distances (To below From) before one toward higher, then the lowest From.
3. RouteDistance<kind>::sampleEdge, given a source's entries at an edge's two ends, gives one entry: of each entry at the From end, with the sample's FromOffset added to its distance and the step {edge carrier, ToDistance, FromDistance}, and each at the To end, with ToOffset added and the step {edge carrier, FromDistance, ToDistance}, the one with the least distance, ties to the From end and then to the earlier entry. It ignores entries inside the edge. A sample with no entry at either end gives none.
4. In a resolved world, for each kind, the field RouteDistance<kind> holds for each entity anchoring a node of parkNetwork(world, kind) exactly its routeEntries on that network: one entry for each node with an entry, at the node's nodePlace, in node order. It holds no other source, and no stepped entries. So sampleField of the kind's field on the kind's network at a node gives, for each source, routeEntries' entry there, and none for a source that cannot reach it.
5. On a network where adding an edge's length to a distance always increases it, as in every network path-networks derives, walking from any place with a source's entry reaches one of the source's anchored nodes. The walk samples the entry, walks along Next's carrier to Next.To, and samples again there, until Next's carrier is NULL_KEY. The lengths walked, |Next.To minus Next.From| from a node and |Next.To minus the place's distance| from inside an edge, added in turn from the anchored end starting from 0.0, equal the first sampled distance exactly.
6. The fields are derived from intent alone: for any world a randomized sequence of edits reaches, the world loaded from its save and resolved equals it, a candidate world made with an edit equals the world that commits it (decision 0025), and building none of them throws, whether a box is unconnected, a path is alone, or a source is unreachable (principle 2). A world before its first resolution samples no entries.
7. In tests/parks/routes.park resolved, the entrance's node on the guest network samples an entry for the shop, and the shop's node on the backstage network samples one for the depot.
8. src/sim/routes/SPEC.md and src/sim/SPEC.md describe route distance as built. linux-debug builds without warnings and its tests pass, windows-debug builds, and the cross-build check passes.

## Medium

Networks (read): the guest and backstage networks path-networks and box-connections derive, read through parkNetwork and the Network type's public edges(), nodeCount(), nodeAnchor, anchoredNodes, and nodePlace (sim/medium/SPEC.md). Route distance never reads how they were derived.

Route distance (produced): two entry fields in the medium's field type, guest-route-distance and backstage-route-distance, with one resolved entry per source per reachable node, and sampleEdge for places between nodes. Each anchored entity, an entrance, shop, or depot, is a source on each network it anchors. Later, believable-guests samples the guest field to choose and move, plausible-operations samples the backstage field for a supply route's length and delay, and legible-simulation samples both for the overlay and previews. In this feature only tests consume them. As the medium gives every field, each has a stepped layer, which stays empty, and the save of a resolved park holds each field's empty stepped entries.

## Principle checks

- Principle 1: the fields' resolved entries are derived and never saved; a resolved world's save loads and resolves back to the world (criterion 6).
- Principle 2: an unreachable node, an unanchored network, and a world before resolution give no entries and no error (criteria 1, 4, and 6).
- Principle 4: every distance is a sum of edge lengths along the network, checked against a reference shortest-path computation (criterion 1) and by walking next steps (criterion 5).
- Principle 6: route distance reads the networks only through their public queries, and consumers read it only through sampleField.
- Principle 10: distances and next steps are functions of the network alone, with a fixed tie rule (criterion 2), and the cross-build check covers routes.park (criterion 8).

## Spec changes

In src/sim/routes/SPEC.md, the first paragraph's "and publishes networks in the medium's Network type (sim/medium/SPEC.md), so other modules read them through parkNetwork and the Network type's queries, never through how they were derived (principle 6)." becomes "publishes networks in the medium's Network type (sim/medium/SPEC.md), so other modules read them through parkNetwork and the Network type's queries, never through how they were derived, and publishes route distance in the medium's fields, so other modules read it through sampleField (principle 6)."

In src/sim/routes/SPEC.md, Networks, "addRoutes registers the resolver path-networks, and makeParkSchema calls it after addParkEdits." becomes "addRoutes registers the resolver path-networks and then route distance's fields and resolver (Route distance), and makeParkSchema calls it after addParkEdits."

In src/sim/routes/SPEC.md, a new section after Stops and nodes:

    ## Route distance

    A route on a network is a sequence of edges, each sharing a node with the next and walked in either direction, and its length is its edges' lengths, ToDistance minus FromDistance, added in turn from its first edge, starting from 0.0. A RouteStep names an edge walked one way: a carrier key and two of its stop distances, From and To, the edge between them, walked from From toward To. A RouteEntry is a Distance and a Next step.

    routeEntries gives a source's route distance at each node of a network, one element per node, indexed by node. A node has an entry exactly when some route joins it to a node anchored to the source, and its Distance is the least length of such a route starting at the anchored node. So a source's anchored nodes have distance 0.0, each other node's is the least, over the edges joining it to a node with an entry, of that node's distance plus the edge's length, and a source anchoring no node of the network has no entries. At the source's anchored nodes Next is RouteStep{}, with carrier NULL_KEY: the route has arrived. At every other node with an entry, Next is the step along an edge at the node, from its stop at the node to the edge's other stop, whose other node's distance plus the edge's length equals the node's distance. When several steps do, it is the one with the lowest carrier key, then one toward lower distances (To below From) before one toward higher, then the lowest From. So distances and steps are functions of the network alone, never of the order a search visits nodes in (principle 10). A carrier that stops at one node twice gives the node a step from each stop, and From tells a mover which stop to leave from.

    Route distance is two entry fields whose entries are RouteEntry: RouteDistance<PathKind::Guest>, named guest-route-distance, on the guest network, and RouteDistance<PathKind::Backstage>, named backstage-route-distance, on the backstage network. A field per kind keeps a box's entries for its two networks apart, even where a hand-written save gives both networks a carrier key. After path-networks, addRoutes registers the guest field, then the backstage field, then the resolver route-distance, which depends on path-networks and both fields' resolvers. In each resolution, for each kind, route-distance publishes into the kind's field, for each entity anchoring a node of parkNetwork(world, kind), in ascending key order, the entity's routeEntries on that network: one entry for each node with an entry, in node order, at the node's nodePlace. So every anchored entity is a source on each network it anchors, and sampleField of the kind's field on the kind's network at a node gives, for each source, its entry there, and none for a source that cannot reach it. Nothing publishes stepped entries into the fields, so they stay empty, and a resolved park's save holds them empty.

    At a place strictly inside an edge, sampleEdge gives one entry for each source with an entry at either end: of each of its entries at the From end, with the place's FromOffset added to its distance and the step {the edge's carrier, ToDistance, FromDistance}, and each at the To end, with ToOffset added and the step {the edge's carrier, FromDistance, ToDistance}, the one with the least distance, ties to the From end and then to the earlier entry. It ignores entries inside the edge, which route-distance never publishes. So a place inside an edge takes the better of the edge's two ends, measured along its carrier (principle 4).

    A mover follows route distance by walking Next from the place it samples: along Next's carrier to its stop at Next.To, joining the carrier at Next.From when it stands at a node, and then sampling again there, until Next's carrier is NULL_KEY at the source's anchored node. On a network where adding an edge's length to a distance always increases it, as in every network path-networks derives, whose edges are longer than JUNCTION_TOLERANCE, the walk arrives, and the lengths walked, |Next.To - Next.From| from a node and |Next.To - the place's distance| from inside an edge, added in turn from the anchored end, starting from 0.0, give exactly the distance first sampled.

In src/sim/SPEC.md, "then the routes resolver that derives the park's networks (sim/routes/SPEC.md)" becomes "then the routes module's resolvers and fields, which derive the park's networks and route distance (sim/routes/SPEC.md)".

The public interface, in the new header src/sim/routes/route_distance.h:

```cpp
// An edge walked one way: along the carrier from its stop at From to its stop at To. The carrier
// is NULL_KEY at the source itself, where the route has arrived.
struct RouteStep {
  EntityKey Carrier = NULL_KEY;
  double From = 0.0;
  double To = 0.0;

  bool operator==(const RouteStep &) const = default;
};

// A source's distance along the network from a place, and the first step of a shortest route.
struct RouteEntry {
  double Distance = 0.0;
  RouteStep Next;

  bool operator==(const RouteEntry &) const = default;
};

// At a place inside an edge: the better of the edge's two ends, ties to its From end.
std::vector<RouteEntry> sampleRouteEdge(const EdgeSample<RouteEntry> &sample);

// The route distance field on the kind's network.
template <PathKind NetworkKind> struct RouteDistance {
  using Entry = RouteEntry;
  static constexpr std::string_view Name =
      NetworkKind == PathKind::Guest ? "guest-route-distance" : "backstage-route-distance";
  static constexpr FieldKind Kind = FieldKind::Entry;
  static std::vector<RouteEntry> sampleEdge(const EdgeSample<RouteEntry> &sample) {
    return sampleRouteEdge(sample);
  }
};

// The source's entry at each node of the network, indexed by node, none where no route reaches it.
std::vector<std::optional<RouteEntry>> routeEntries(const Network &network, EntityKey source);

// Registers both kinds' route distance fields and then the resolver route-distance. addRoutes calls
// it after registering path-networks.
void addRouteDistance(WorldSchema &schema);
```

with visitFields for RouteStep (carrier, from, to) and RouteEntry (distance, next).

## Files affected

- Create: src/sim/routes/route_distance.h
- Create: src/sim/routes/route_distance.cpp
- Modify: src/sim/routes/networks.h (addRoutes comment)
- Modify: src/sim/routes/networks.cpp (addRoutes calls addRouteDistance)
- Modify: src/sim/CMakeLists.txt
- Modify: src/sim/routes/SPEC.md
- Modify: src/sim/SPEC.md
- Create (test pass): tests/sim/routes/route_distance_test.cpp, and any other file under tests/sim/routes/
- Modify (test pass): tests/sim/CMakeLists.txt

## Dependencies

- box-connections: connectors and their anchored door nodes. Merged.
- path-networks: the networks, parkNetwork, JUNCTION_TOLERANCE. Merged.
- shared-medium's first-field-and-flow: addField, publishResolved, sampleField, sampleEdge's EdgeSample. Met.

## Out of scope

- Route distances in the graph view, hovering a node to list each source's distance and next step: the milestone's deepening candidate.
- Fields with no stepped layer, so a save holds nothing for a field that is only ever resolved. Sent to first-field-and-flow's deepening candidates.
- Consumers: guests choosing and moving (believable-guests), supply routes (plausible-operations), overlays (legible-simulation).

## Open questions

None.
