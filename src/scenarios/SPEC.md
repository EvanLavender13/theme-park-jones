# scenarios

The scenario runner is a headless executable. It runs synthetic scenarios and park files and prints what every build must compute identically (decisions 0022 and 0036). It also generates the checked-in parks that are made rather than drawn. It is a test utility, checked by running it.

## Boundary

The library tpj_scenarios_lib holds the scenarios, the runner's output, and the park generators. It links only tpj_sim. It is simulation code:

- It builds with the simulation's floating-point flags.
- The sim symbol check runs on its archive.
- It draws only through sim/draw.h.
- It computes e^x and ln x only through sim/sim_math.h.
- Only main reads the clock.

It reads the math and draw tables in tests/sim/support. The executable tpj_scenarios adds only its main, and every preset builds it. scripts/cross-build-check.sh depends on its output and on --compare's exit statuses, described below.

## Scenarios

registeredScenarios lists the synthetic scenarios. Each is named and seeded, and they run in a fixed order. Between them they exercise:

- keyed draws
- sim math
- entity creation and destruction
- derived entities
- the medium's fields, ground fields, and flows
- random park edits, with the guests and shops they affect
- shops selling food and drink, whose supply is cut and restored, a bench and a toilet block, and a furnished guest path whose bench seats guests and turns them away when full

This way, if any part of the simulation's arithmetic or ordering could differ between builds, it shows up in the compared output.

## Output

tpj_scenarios [--ticks n] [file...] writes, for each scenario and then each park file, a line after resolution and after every one of n cycles. Then it writes the sim math table's results, then the keyed draws' results. A scenario's line is `scenario <name> tick <t> hash <h>`, and a file's line is `file <path> tick <t> hash <h>`. h is the world's hash as 16 lowercase hexadecimal digits. If two builds simulate identically, they write identical output. Timings go to standard error, so they never enter the compared output. If an option is unknown, --ticks is malformed, or a file cannot be read or loaded, it exits with a nonzero status and a message.

tpj_scenarios --compare left right exits with status 0 when the two outputs hold the same lines. Otherwise it prints the first line where they differ, from each side, and exits with status 1. It exits with status 2 when it is not given two files or cannot read one. Use this comparison to show that work leaves the simulation unchanged.

## Slice parks

tpj_scenarios --slice-parks fed warm cut main-street warm-main-street facilities stocked day no-toilet noon evening far-tables unfurnished makes the boxes-and-tubes slice's parks from fed, the good-paths slice's warm park from main-street, the day-at-the-park slice's stocked park from facilities, the day-at-the-park slice's four day parks, and the somewhere-to-sit slice's two parks that move or remove day's furniture:

- warm is the park after WARM_TICKS cycles, which is long enough for a guest to be waiting at the shop and a shipment to be in transit.
- cut is warm one cycle later with its backstage paths deleted, so the shop's supply is gone from the first tick.
- warm-main-street is main-street after MAIN_STREET_WARM_TICKS cycles, long enough that guests fill the park in every activity, some of them heading home. main-street and facilities are read and never written.
- stocked is facilities with a depot and a backstage path added, stepped until a shop holds food, a shop holds drink, and drink is still on its way. If that doesn't happen within STOCKED_LIMIT_TICKS cycles, it fails.
- day is a T of guest path from the entrance, with a food shop at one end of the T's arms and a drink shop at the other, a toilet block on the path in, and a depot on a backstage path behind both shops. The arm toward the food shop has tables on both sides, every 6 m, and the path in has benches every 10 m on the side away from the toilet block. Every facility is within about an hour's walk of the entrance. It has the default hours and starts at 9:00 with no guests. If a shop would be starved, or a shop or toilet block would have no guest connector, it fails.
- no-toilet is day with its toilet block deleted.
- noon is day at 12:00, NOON_TICKS cycles later, with guests in the park.
- evening is day at 20:30, EVENING_TICKS cycles later, so a run of tpj_scenarios's default 3,000 ticks crosses closing.
- far-tables is day with the arm toward the food shop unfurnished and the arm toward the drink shop holding tables on both sides, every 6 m. The benches stay.
- unfurnished is day with no furniture.

In no-toilet, far-tables, and unfurnished, each box, path, and entrance keeps the key it has in day, so all four parks admit the same guests with the same needs.

tests/parks/fed.park, warm.park, cut.park, warm-main-street.park, stocked.park, day.park, no-toilet.park, noon.park, evening.park, far-tables.park, and unfurnished.park are its output. If the simulation's rules change, running it again remakes the parks under the new rules. It writes no file when it fails.

## The full park

makeFullPark makes the stress park at the size the game means to support:

- about 4 km of guest path in a grid of 100 junctions
- 30 shops, each joined to the guest network and to a backstage row, a third each selling food, drink, and both
- three depots
- 15 toilet blocks and 45 benches, each joined to the guest network
- furniture on every guest segment, benches on its left side and tables on its right, every 10 m
- 2,000 guests spread evenly along the paths

By default it is stepped a minute into play (FULL_PARK_WARM_TICKS). Every shop is reachable and supplied, and every bench and toilet block is joined to the guest network. If that would not hold, it throws std::logic_error naming the edit, the shop, or the facility. Every park it gives is valid, and none of its shops is starved. Its save is the same text on every build. tpj_bench --full-park writes it to tests/parks/stress/full.park (bench/SPEC.md).

## Cross-build check

scripts/cross-build-check.sh builds tpj_scenarios with windows-debug, windows-release, and linux-release. It runs each build over every tests/parks/*.park file. It fails on the first line where a release build's output differs from windows-debug's. scripts/release-check.sh runs it at release (decision 0030).
