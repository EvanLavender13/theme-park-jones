# Feature: Main Street Park

## Summary

tests/parks/main-street.park holds the good-paths slice's park, drawn through the tools. Its 3 m entrance path widens along a long taper into a 10 m main street, 60 m long. The street ends in a T, with a 3 m side path out each way. A second side path leaves the middle of the street to the west and bends round to meet the west arm, so the two make a block with rounded corners. The east arm runs on through a loop, a rounded square that it crosses twice, and then turns 45° along a wide arc. Eleven shops stand in rows along both sides of the street, each turned by the frontage snap to face it. A backstage path runs behind both rows and around the end of the street, crossing three guest paths without joining them. The depot sits behind the T, facing it. Every rounded corner has a concentric outside edge, and the street's junctions have curved inside edges. The park has no guests, and it is saved at tick 0. Drawing it settles the milestone's tuning values. The feature changes no code, and it adds no state.

The layout, with north toward the entrance (+z) and east to the right (+x). B marks the backstage path, and S marks shops:

```
                          entrance (0,126.5)
                              |  3 m path
                              /\  taper, 14 m long
            B    S S S   |          |  S S S S S S   B         shops at x = ±9,
            B            |   main   |                B         backstage at x = ±15
  (-40,73) .-------------+  street  |                B
           |    B   S S  |  10 m    |                B
           |    B        |          |                B          loop: rounded square,
  (-40,43) '----B--------+--(0,43)--+--------B--[ (28..52, 31..55) ]--. (65,43)
                B                            B                        \   radius 16
                '-------------B--------------'                         \  45°
                          [ depot ]                                     ' (84.8,23.2)
```

## Acceptance criteria

1. tests/parks/main-street.park holds the slice scenario's park with no guests, at tick 0: the entrance; the entrance path widening into the 10 m main street along a taper; the T at the street's end; the side paths, with their 90° and 45° bends, the 16 m radius arc, and the block's rounded corners; the loop, joined to the east arm at both crossings; eleven shops in rows along both sides of the street; the backstage path behind both rows; and the depot facing it. It has 18 path points and 19 segments, 16 guest and 3 backstage, and 12 boxes. Every edit that made it came from the tools, given pointer positions as a player gives them. (check: the drawing run in PLAN.md prints the counts and that every edit was accepted)
2. The park world checks pass over main-street.park, which they read with every other park in tests/parks. (ctest)
3. A capture of main-street.park shows one paved surface. The entrance path widens smoothly into the street. The T and the side path's junction with the street have curved inside edges. Every rounded corner's outside edge runs parallel to its inside edge. The loop's crossings with the east arm show no edge line across them. Where the backstage path crosses a guest path, each one's edge line shows across the other. Every free end is cut square. The shops stand in even rows, square to the street, each with its front face 1 m from the paving. (capture)
4. A capture of main-street.park with --graph shows a walkway from the depot's front door and from every shop's back door to the backstage path. (capture)
5. The milestone's Tuning section states the final values, and each one matches the constant in the code. (check: read the Tuning section beside src/tools/tools.h and src/sim/park/intent.h)
6. In the windows-release build, the user opens main-street.park and reshapes it with every tool: drawing, joining, dragging points, handles, deleting, removing a point, placing shops by frontage, holding Ctrl, and undo and redo. Snaps catch and release well both at the view the park opens in and zoomed in to draw, the defaults suit new paths, and the setback suits the rows. (play, by the user)

## Tests

None. The feature adds a park file and no state or behavior. The park world checks read every park in tests/parks, so they cover main-street.park once it is there (docs/testing.md).

## Medium

The feature adds no fields or flows, and it changes no code. The park file holds park intent through the existing save format: path points, segments, boxes, and the entrance.

While the park has no guests, nothing reaches the street's shops. Their front doors lie 6 m from the street's center line, beyond CONNECTION_REACH, as the milestone notes for wide segments. wide-routes measures reach from the paving's edge, and then those doors connect. The shops' back doors and the depot connect to the backstage path today.

## Spec changes

None. The park is test content, and no module's behavior changes.

Plan changes:

- plans/effortless-building/path-network/MILESTONE.md, Tuning: replace "These are starting values. Drawing main-street.park adjusts them, and each feature that uses one states its final value." with "These are the final values, settled by drawing main-street.park." Change any value the drawing changed.
- plans/slices/good-paths/SLICE.md, Open questions: replace the tuning item with "- Door reach: navigable-networks/wide-routes sets it while planning and adjusts it on main-street.park. path-network's Tuning section settles effortless-building's values."

## Files affected

- Create: tests/parks/main-street.park
- Modify: plans/effortless-building/path-network/MILESTONE.md
- Modify: plans/slices/good-paths/SLICE.md

## Dependencies

Features 5 to 11 of the milestone, all merged: the paved surface, edge radii, predictable editing, width tapers, snaps and guides, frontage snap, and undo and redo.

## Out of scope

- warm-main-street.park. spread-guests makes it with tpj_scenarios --slice-parks, as the slice says.
- Connectors for the street's shops. wide-routes adds them.
- Shops along the side paths, a round hub, and plazas. The scenario places shops only along the street, and plazas wait for the next slice.
- Checking in the drawing program. It stands in for the player's hand, like any drawn park, and the file is the result.

## Open questions

None.
