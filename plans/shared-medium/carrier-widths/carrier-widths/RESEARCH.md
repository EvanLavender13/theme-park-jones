# Research: carrier-widths

## Which direction should a polyline report at a place, including at its corners?

Curve libraries that answer a tangent at an arc length use the piece of the line that holds that length. Godot's Curve2D samples its baked points and turns the stretch between the two nearest points into a rotation. Unity's Splines evaluates the tangent of the curve piece that holds the given position. At a corner of a polyline the direction is undefined, so a library has to pick a side. The usual choice is the piece ahead, in the direction of increasing length, with the last piece used at the very end so the end still has a direction.

For carriers, this suggests a direction of travel toward higher distances, taken from the polyline piece ahead of the place, and from the last piece at the carrier's end. A caller that wants a smooth turn through a corner, such as a guest drawn across a path's width, blends directions itself. The medium doesn't smooth.

Rejected: averaging the two pieces at a corner — at a 180 degree turn the average is zero, and no caller needs it yet. Using producer arc lengths to scale the direction — they need not equal ground lengths, so the result would not be a unit vector.

Sources: https://docs.godotengine.org/en/stable/classes/class_curve2d.html — sample_baked_with_rotation turns the local stretch of the baked line into a direction; https://docs.unity3d.com/Packages/com.unity.splines@2.0/api/UnityEngine.Splines.SplineUtility.html — EvaluateTangent gives the tangent of the piece holding a position.
