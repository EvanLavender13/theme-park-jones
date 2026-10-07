# 0038. A guest walking anywhere on its route's paving travels that route

Status: Accepted, 2026-10-07.

## Context

Principle 4 says distance is measured along the routes things actually travel. Until now guests walked along the networks' center lines and connectors, so the distance route distance measured was exactly the distance they walked.

Paths now have widths up to 16 m, and their paving has fillets, tapers, and walkways. A guest that follows center lines across a wide street to a shop's door detours by several meters where a person would walk straight. Drawing guests spread across the paving from a place on a center line failed at junctions, because the lines and the paving are different shapes there. The good-paths slice therefore has guests hold a position on the paving itself. The network still chooses their route, and each guest walks the straight way across the paving toward the farthest point along that route it can reach without leaving the paving.

A guest then walks a little less than the route distance it chose by. Measuring route distance across the paving would close that gap, but it would move route distance, every field, and every flow off the network.

## Decision

Route distance along the network remains the measure of a route. A guest walking anywhere on the paving of the segments, points, and walkways along its route is traveling that route, and principle 4 holds for it. Choices, fields, and flows keep using network route distance, which on wide paving is a little longer than the walk a guest takes.

A guest never leaves the paving. A shortcut over the grass is not traveling the route.

## Consequences

Guests walk straight across wide paving and keep to their own part of a path's width, while every module that reads route distance keeps working unchanged.

On wide paving, a guest's choice slightly overstates how far a shop is, consistently for every shop. This is accepted for good-paths.

Measuring distance across the paving itself is deferred. Plazas and open paving, the next slice, are where the gap is likely to matter, and that slice decides whether to close it.

believable-guests' capability brief, which says principle 4 is kept because every movement uses route distance, is updated to cite this record when spread-guests is planned again.
