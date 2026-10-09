# Research: time-of-day

## How should the clock be computed and shown?

The milestone's RESEARCH.md settles the main question: the clock is a function of the saved tick, so it adds no state. What is left is its arithmetic and its display.

The clock counts whole game seconds. Each tick is two game seconds, so the time of day for a tick is a sum and a remainder in unsigned 64-bit integers. Floating point would round differently at large ticks, and the C runtime's floor and fmod are not needed. A 64-bit tick at two seconds a tick doesn't overflow in any park's lifetime.

Park and city games show the time as a 24-hour clock or with AM and PM, next to the day or date, in a small panel with the speed controls. Planet Coaster 2's park day shows the hour beside its speed buttons, and Cities: Skylines shows the date and time in its bottom bar beside the speed. A 24-hour clock in hours and minutes is the plainest form, and it needs no locale. Seconds change too fast to read at 4x, so the panel leaves them out.

Rejected: the time as a fraction of a day in floating point — rounding at large ticks, and two builds could disagree at a boundary. A saved time-of-day component — it duplicates the tick (principle 1). Showing seconds — unreadable at speed.

Sources: the milestone's RESEARCH.md; https://gamingtrend.com/reviews/planet-coaster-2-review-the-park-builder-of-my-dreams/ — Planet Coaster 2's park day and its controls.
