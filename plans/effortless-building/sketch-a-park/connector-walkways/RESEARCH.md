# Research: connector-walkways

## How does the renderer tell a connector from a path?

A network's carriers are its paths' ground lines and its connectors, and the renderer draws paths from intent already, so it needs only the connectors. The routes spec gives two ways to find them. The graph overlay uses one: a carrier is a connector when no path parkPaths gives has its key. That rule looks at paths of both kinds, so in a hand-written save where a guest path holds the key of a backstage connector, the connector would be taken for a path and never drawn. The other way is the anchor contract: each connector's first stop is at a node no other stop has, anchored to its door's entity, and those are the networks' only anchors. So a carrier is a connector exactly when its first stop's node is anchored. That reads only the Network type's public queries, holds for every world path-networks derives, and needs no intent at all.

Rejected: the parkPaths key rule — it misreads cross-kind keys in hand-written saves, and a same-kind path with an empty ground line hides its connector. Recognizing connectorKey in the carrier's key — a hand-written save can give a path a derived key, and the renderer would have to know every entity and face.

## What does a walkway look like?

A connector is a straight two-point carrier from a door on a box face to the nearest point of a line of its kind. Laying the ribbon rule appendPath already uses along those two points gives a flat quad of the kind's width, lifted as paths are, running from the face to the path's center line. Its far end lies inside the path's own ribbon, which has the same width, color, and lift, so the overlap draws as one surface: coplanar faces of one color and normal shade identically, whatever depth wins. At a path's end the walkway's square corners can stick out past the path's own square end; the milestone's crossings candidate covers cleaner joins.

Rejected: drawing the walkway through appendPath with the door and connection point as clicked points — groundLine resamples a curve through them, so the ribbon's points would not be the connector's, and principle 8's check against the network would be indirect.

## What should a ghost show?

The milestone asks that a ghost for a valid edit show the walkways of the edit's candidate world. makeCandidate copies the world, applies the edit, and resolves it, so the candidate's networks are exactly those the commit gives (decision 0025, and box-connections' candidate criterion). Appending every candidate walkway at GHOST_ALPHA keeps the rule one line and the principle 8 check direct. A candidate walkway the committed world already has lies exactly on its opaque twin, and a translucent copy of a color blended over the same color gives that color, so only walkways the edit adds or moves show as ghosts. A box placed out of reach shows none. A refused edit changes nothing, so its ghost has no walkways.

Walkways an edit removes, such as a deleted box's, still draw opaque from the committed world until the commit. Marking them in DELETE_TINT is a deepening candidate.

Rejected: ghosting only the walkways that differ from the committed world's — it needs a comparison of connectors by key and points in the renderer, for nothing the blend does not already give.

## When is the park mesh rebuilt?

The app rebuilds the park mesh when intent changes. Networks are derived from intent alone and resolved in the same cycle that applies an edit, and every world the app holds is resolved (start, open, new, and each step), so an intent change is exactly when the networks can change. The app needs no new trigger. The ghost is rebuilt when its edit changes or the park mesh is rebuilt, which covers its candidate too.
