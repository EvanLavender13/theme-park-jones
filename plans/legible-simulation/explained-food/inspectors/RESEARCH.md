# Research: inspectors

## How does a click meet a guest or a shop?

The milestone's research settles on a ray against the drawn boxes, nearest hit winning (../RESEARCH.md). Every drawn solid is an upright box over a footprint: an entrance over ENTRANCE_SIZE, 5 m tall, a shop or depot over boxSize, 4 m or 6 m tall, and a guest over GUEST_SIZE, 1.8 m tall, at its record's Position with the default facing. A footprint turns with its pose, so the slab test runs in the footprint's own frame: the ray's origin and direction are projected onto Forward and Right, measured from the pose's position, and the three slabs are the half depth along Forward, the half width along Right, and 0 to the height along +Y. The entry parameter is the largest of the three slabs' near crossings, clamped below at 0, and the ray hits when it is no greater than the least of their far crossings. A ray parallel to a slab hits only when its origin lies between the slab's planes.

The ray is the one groundAtCursor already defines, from the eye through the cursor, so the pick and the ground point agree on what lies under the cursor. Entrances and depots are tested as well as shops and guests, so a depot drawn in front of a guest takes the click and the inspector opens on nothing, as the player sees it. Paths, walkways, and starved marks are not tested: paths and walkways lie flat on the ground under the boxes, and a starved mark floats over its own shop's roof.

Rejected: testing only guests and shops. A guest hidden behind a depot would open on a click that visibly lands on the depot. Rejected: testing the starved marks as part of their shop. A mark is drawn a meter over the roof, so a click on it from a low camera would open a shop the ray never reached; it is not needed to open the shop, whose roof and sides lie right under it.

Sources: src/render/SPEC.md, groundAtCursor, appendBox, and Guests; src/sim/park/SPEC.md, Footprints; ../RESEARCH.md, How should a click find a guest or a shop.

## What does an inspector cost per frame?

Timed on warm.park after 3000 ticks, 61 guests and two boxes, with a temporary Catch2 case: every guest's record costs 0.091 ms in debug and 0.0037 ms in release, every shop's record 0.107 ms and 0.0038 ms, and one guest's record 0.0014 ms and 0.0001 ms. The Debug panel already reads every record each frame. An inspector reads one record and formats a dozen lines, so building its rows every frame costs nothing measurable, and it follows each tick without any rule for when it is stale. Picking tests every drawn box once per click, not per frame.

The highlight on the inspected entity lives in the ghost mesh, which the app rebuilds only when its preview is made again, at every tick, or its highlight or overlay changes. A guest's highlight moves with the guest because the preview is made again at every tick, so adding the inspected key to what the ghost was built with is all the highlight needs.

Rejected: keeping the rows between frames and rebuilding them only on a tick. It would add a cache for no measurable gain.

Sources: timings from a temporary Catch2 case in tpj_legible_tests on windows-debug and windows-release; src/app/SPEC.md, Tools.

## When is the click taken?

The app gives the tool the left button's press before the frame's ticks and before the camera moves, so the press acts on the world and view on screen. The pick belongs at the same moment: the guests on screen were drawn from the world before this frame's ticks, and the camera is still where the last frame drew it. A press reaches the app only while ImGui does not want the mouse, so a click on a panel picks nothing.

Sources: src/app/SPEC.md, Tools; src/app/main.cpp, useButtons and runLoop.
