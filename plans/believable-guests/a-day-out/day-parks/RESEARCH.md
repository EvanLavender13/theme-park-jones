# Research: day-parks

The slice describes the four parks, and the milestone gives the tuning values and the checks. This file covers how big day.park should be, how to make the four parks, and how the slice's comparisons are run before the slice's closing writes them as tests.

## How big should day.park be?

The clock runs one game minute for each second of play at 1x, and guests walk 1.3 m in that second. So a guest walks about 78 m a game hour. The milestone's need rates are set in game hours: hunger fills in 4 to 6, thirst in 3 to 4.5, and tiredness over 400 to 600 m of walking. A shop an hour's walk away is about as far as a guest can go for one need and still have time to use it.

The milestone first asked for the two shops to stand more than 312 m apart, where food availability's discount curve reaches 0. That curve was set in seconds of play before the park had a clock, so 312 m is four game hours of walking. A park that size would test the walk, not the facilities: a guest would get tired and hungry on the way to either shop, and many would turn for home before arriving. The user agreed that the curves were arbitrary choices. explained-needs will set each need overlay's discount curve in game time, so a compact park still shows each shop's overlay separately.

So day.park is a T: a short path in from the entrance to a junction, and an arm each way, with the food shop at one end and the drink shop at the other. The bench and toilet block stand on the path in from the entrance, as in facilities.park. The depot stands behind the junction on a backstage path that runs behind both shops. Each shop is about 70 m from the entrance, under an hour's walk, and the shops are about 90 m apart. The bench and toilet block are about 10 m from the entrance.

Rejected: shops more than 312 m apart — four game hours of walking between them, at the clock's scale. A bigger park — the walk, not the park's size, is what goes wrong. Facilities spread around a loop — guests wander a loop for hours without passing a dead end, which is harder to read while tuning. A dead end is where guests turn, which a T shows plainly.

## How should the four parks be made?

The full park is made from commands applied to the new-park template, so its layout lives in code that the world validates, not in a hand-written save (src/scenarios/full_park.cpp). day.park follows it. no-toilet.park is day.park with its toilet block deleted by a command, so every other entity keeps its key. Guests' draws are keyed on their keys and the tick, and the entrance's arrival draws on its key, so the two parks admit the same guests with the same needs. Then a comparison between them shows only what the toilet block changes. noon.park and evening.park are day.park stepped to 12:00 and 20:30. The cross-build check steps each park 3,000 ticks, 100 game minutes, so a run from 20:30 crosses closing at 21:00.

Rejected: a hand-written day.park text — a layout no command made can hold states the world would refuse. no-toilet.park laid out without the toilet block — every key after it shifts, so its guests draw different needs and the comparison mixes in luck.

## How are the slice's comparisons run now?

The slice's closing writes criteria 1 to 3 as integration tests. day-parks only shows that the made parks support them, with a scratch program that runs each comparison and prints both sides. A comparison that fails here means a tuning value or the layout needs changing. That is the user's call after watching a day, not one to settle by searching for values.
