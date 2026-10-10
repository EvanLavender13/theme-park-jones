# Research: arrival-curve

The milestone's RESEARCH.md settles that arrivals are one keyed Bernoulli draw for each entrance and tick, at the rate of a piecewise-linear curve over the fraction of the opening hours that has passed. This file covers how to publish the curve, what the curve does when the park is open all day, and how the parks made by stepping keep the guests their checks need.

## What should arrivalCurve publish?

explained-needs will draw the curve in the Hours panel from a candidate with the tentative hours, and it must not compute the curve itself (the slice's medium map). A scratch program will also compare counted arrivals with the curve. Both need the rate at a time of day. A function that gives the rate at a second of the day, under the world's hours, serves both. It is exactly what the guests step uses, so a preview and the simulation can't disagree. The panel samples it at whatever spacing it draws. needCurve already gives a curve's value at an input, so the name and shape match the code.

Rejected: a list of the curve's corner points in clock time — its corners fall at fractions of the hours, which needn't be whole seconds, and hours past midnight make the times wrap. Rounding them would make the published curve differ from the one guests are admitted by. Publishing ARRIVAL_CURVE and the hours for the panel to combine — that is the panel computing the curve, which the slice rules out.

## What does the curve do when the park is open all day?

If the opening and closing times are equal, the park is open all day (sim/park/SPEC.md). The fraction of the hours passed then needs a length, and the hours give one: a whole day from the opening time. With that rule the curve stretches over 24 hours, so arrivals stop over the last part of the day before the opening time comes round again. No guest is sent home by closing in such a park, because the closing term is always 0, so the guests inside leave only through their needs. This is a rare setting, and the rule only has to avoid dividing by zero while staying the same rule as other hours.

Rejected: a constant rate all day — it is a second curve for one setting. No arrivals at all — the player who opens the park all day expects guests.

## How do the parks made by stepping keep the guests their checks need?

The slice parks are made by stepping a park from 9:00, the default opening time. Today one guest arrives at each entrance every 60 ticks, so warm.park, at 2,200 ticks, holds 36 guests, and warm-main-street.park, at 4,530 ticks, holds 68. Under the curve, the first hours of the default day admit about 5 to 15 guests a game hour at each entrance. That is about 10 guests over warm.park's warm-up and about 29 over warm-main-street's. The world checks need a guest waiting at warm.park's shop and one heading there, and warm-main-street's spec says it holds guests in every activity, some heading home. Because the draws are keyed, the remade parks are fixed by the seed, so whether they still hold those guests is checked once, after remaking them. If not, a longer warm-up reaches the morning peak and admits more. The full park places its 2,000 guests through addGuest, so the curve only adds a few during its runs.

Rejected: opening the slice parks earlier so they start at the peak — it changes the parks' hours, which the world checks then cover as an edit of their own, for no gain over a longer warm-up.
