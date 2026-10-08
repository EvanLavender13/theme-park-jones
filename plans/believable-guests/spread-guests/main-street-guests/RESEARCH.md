# Research: main-street-guests

## How long is main-street.park warmed?

main-street.park has one entrance. It admits a guest every ARRIVAL_INTERVAL, 60 ticks, and each guest stays from STAY_MIN to STAY_MAX, 1800 to 3600 ticks, before it walks home. So the number of guests rises for the first 1800 ticks, and the first guests start leaving between 1800 and 3600 ticks. After about 3600 ticks, guests leave about as fast as they arrive, and the park holds about 50 to 60 of them. Warming it for 4530 ticks, about two and a half minutes of play, takes it past that point. 4530 isn't a multiple of ARRIVAL_INTERVAL, so no guest arrives on the last tick without yet having walked and set its guest presence entry. The park then holds guests in every state: wandering, heading to a shop, waiting at one, and heading home, spread along the main street and the side paths. That is what the slice's capture and its integration test of a crossing need.

Rejected: WARM_TICKS, 2200 — the park is still filling, no guest has headed home yet, and most guests are near the entrance end of the street.

## How does --slice-parks make it?

--slice-parks takes fed, warm, and cut today, and writes all three. It reads fed and saves it again after resolving it, so fed.park always holds what resolution gives it. main-street.park is authored intent, the slice's hand-built park, so it is read and not written. Saving it again would only add empty field sections, and it would make it look like generated output. The command takes main-street and warm-main-street after the three it takes now, and writes warm-main-street from main-street warmed MAIN_STREET_WARM_TICKS cycles with no commands, as warm comes from fed.

Rejected: a separate option for warm-main-street — the milestone and the slice say --slice-parks makes it, and one command remaking every generated park keeps them in step when the rules change. Rewriting main-street.park as well — see above.

## What becomes the baseline?

Runtime reports aren't checked in, and nothing compares against a stored one. The slice's baseline is the report taken on main after this feature, with full.park made again so it holds guest presence. The plan keeps it as build/good-paths-baseline.report and writes its full.park medians into the plan, so later work can see what it was compared against.
