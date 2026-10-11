# Research: day-panels

The milestone's RESEARCH.md covers why the panels exist, why the departures panel has fixed rows, and why the Hours panel reads the curve from a candidate of its own. This file covers how to sample and draw the curve, and when to make its candidate.

## When should the Hours panel make its candidate?

arrivalCurve reads only the park's opening hours (src/sim/guests/guests.h). A candidate with the chosen hours applied gives the same curve whatever else changes in the world, until the chosen hours change. So the panel needs a new candidate only when the hours it shows change, which happens on a press of - or +, on Apply, and when the park's hours change. When the hours shown are the park's own, the world itself gives the curve, with no candidate. Making a candidate copies the world, as each tick's preview already does, so one copy per press costs no more than a tick of a ghost.

Rejected: a candidate every frame — it copies the world sixty times a second for a value that changes only on a press. Reading the curve from the preview's candidate — while a ghost or a stock choice is the preview, that candidate has the park's hours, not the chosen ones (milestone RESEARCH.md).

## How should the curve be sampled and drawn?

Opening and closing times move in quarter hours, and ARRIVAL_CURVE is piecewise linear in the fraction of the open hours that has passed. Its peak is a plateau 0.15 of the open hours wide. The shortest hours the panel allows are 15 minutes, which makes the plateau 135 game seconds wide. Sampling at the start of each game minute, 1440 samples, puts a sample on every opening and closing time and at least one on the plateau for any hours, so the drawn peak is always the real one. Each sample is one cheap call, made only when the hours change.

ImGui's PlotLines draws a sampled curve, but its hover tooltip shows the sample's index, which means nothing to the player, and it has no way to mark a point on the curve. Drawing into a fixed-size area with ImGui's draw list gives the line, a vertical mark at the time of day, and the frame, with no tooltip. A fixed vertical scale, the curve's peak rate, keeps the shape comparable as the hours change: a shorter day draws a narrower hump of the same height, as arrivals are set per hour.

Rejected: PlotLines — its tooltip shows sample indexes, and it can't mark the time of day. Scaling to the largest sample — the same peak would look the same however the hours change, and a day with the park closed would divide by zero. Sampling each quarter hour — for hours shorter than 1 h 40 min no sample lands on the plateau, and the drawn peak falls short of the real one.
