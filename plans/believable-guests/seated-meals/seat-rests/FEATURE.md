# Feature: Seat Rests

## Summary

Tired guests sit on the furniture along the paths. When a guest chooses, each piece that best rest seats gives where it stands becomes an option, scored by tiredness and the piece's rest relief against the walk. A guest that picks a piece follows the field's next steps to the piece's seat place, even inside an edge. It walks to the edge of the paving beside the piece and sends the piece a seat visit. If the visit doesn't come back within the turn-away time, the guest is seated. When it comes back with a rest, tiredness falls. If it comes back with nothing, the piece was full: the guest remembers it for FULL_MEMORY_TIME and chooses again at once. Seated guests are drawn as shorter boxes on the piece's seats, and each keeps its seat until it stands up. The guest Inspector names pieces, counts seat options under tiredness, and lists the pieces the guest remembers as full. Parks without furniture play exactly as before. full.park, whose guest segments all carry furniture, is where this feature is played.

## Acceptance criteria

1. A guest's options include one for each piece that best rest seats gives at its place, unless the guest remembers the piece as full. Such an option has kind seat, the piece as its facility, and tiredness as its need. Its terms are Relief, which is RELIEF_WEIGHT times tiredness's curve times the piece's rest relief, Distance, which is DISTANCE_WEIGHT times the field's distance, and Commitment, which is COMMITMENT_BONUS while the guest heads to that piece. Its Score is the sum of the terms. (check: the scratch program of criterion 3 calls guestOptions for every guest at every tick it steps. Each seat option must match an entry of the field sampled at the guest's place, with matching terms. Expected: `seat options: N checked, 0 wrong`, with N above 0)
2. A guest that picks a piece follows the field's next steps to the piece's seat place, inside an edge or at a node. Over the last stretch, it moves toward the piece's side of the paving and stops at the paving's edge beside the piece, where its box fits. Then it sends one seat visit to the piece and waits. If the field at its place no longer gives the piece, as after an edit, the guest drops it and chooses again at once. (play full.park: follow a tired guest to a bench or table and see it stop on the paving beside it. Then, while a guest heads to a piece, clear that segment's furnishing and see the guest turn to another choice. Check: the scratch program's count `reached: N` is above 0)
3. If the visit hasn't come back VISIT_DELAY plus RETURN_DELAY ticks after the guest sent it, the guest is seated. When the visit comes back with a rest, tiredness falls by the rest relief the guest chose the piece by. If it comes back with nothing, the guest remembers the piece and the tick, and chooses again in that tick. A guest remembers at most SEAT_CHOICES pieces, each for FULL_MEMORY_TIME. If a guest finds one more piece full while it remembers SEAT_CHOICES pieces, it forgets the one it found full first. (check: a scratch program steps a copy of tests/parks/stress/full.park for 1800 ticks, with furnishing cleared from every guest segment but the five with the lowest keys, so pieces fill. It follows each guest's seat visits tick by tick and prints `seated: N wrong: 0`, `turned away: N wrong: 0`, and `reached: N`, each N above 0)
4. A seated guest is drawn as a box GUEST_SEATED_HEIGHT tall on one of the piece's seats, as seatPose gives the seats. No two guests are drawn on one seat. A guest keeps its seat while it stays seated, whoever else sits down or stands up there. A newly seated guest takes the lowest free seat. If the piece is gone while a guest still waits for its visit, the guest is drawn standing at its ground position. Clicking a seated guest selects it. (play full.park: watch a table fill and empty, and click a seated guest. Check: the scratch program of criterion 3 updates a SeatedGuests each tick and prints `seats: N checked, 0 shared, 0 moved`, with N above 0, counting a guest drawn on another guest's seat as shared, and a guest whose seat changed while it stayed seated as moved)
5. The guest Inspector names a piece of furniture as "bench" or "table" with its key, wherever it names a target or a facility. The tiredness row counts seat options as offers of tiredness. A "Full pieces" row lists the pieces the guest remembers as full, each with how long ago it found it full, or says none. (play full.park with the Inspector open on a guest heading to a seat, a seated guest, and a turned-away guest)
6. In a park without furniture, guests behave exactly as before. When tpj_scenarios --slice-parks and tpj_bench --full-park remake the parks, each park in tests/parks differs from its checked-in file only in the guest lines' new fields. full.park changes because its guests now sit. (check: after the remake, `for f in tests/parks/*.park; do git show HEAD:$f | cmp -s - <(sed -E 's/ target-owner=.*$//' $f) || echo $f; done` prints nothing)
7. bestSeatsAt gives exactly the values sampleField gives for best rest seats and best meal seats, at every node and at the middle of every edge of full.park. On full.park, the runtime report's over-budget count stays 0 of 25. Ticks change because guests now sit. (check: the scratch program prints `bestSeatsAt: N places, 0 differ`; runtime report compared with build/best-seats/after.report)
8. src/sim/guests/SPEC.md, src/sim/wayfinding/SPEC.md, src/sim/furniture/SPEC.md, src/render/SPEC.md, src/legible/SPEC.md, and src/app/SPEC.md describe the changes as built. (review)

## Tests

None. full.park holds guests that sit, so the park world checks cover the new guest fields through it only once a park in tests/parks holds them. tests/parks has no furniture with guests until seat-parks. The guest fields are part of the guest state already checked in every park with guests, so copies, saves, and hashes cover their format now, and seat-parks makes noon.park hold them with values other than 0.

## Medium

- Samples best rest seats (resolved entry field, from navigable-networks' best-seats) at the guest's place for choice and heading, and at each node of its route ahead for the next step.
- Reads seat places (resolved entry field, from effortless-building's furnished-paths) from the target piece's segment's slot, for the piece's pose: to walk to its side of the paving, and for the guest record while seated.
- Supplies seat visits (flow, to plausible-operations' served-seats): one to the piece the guest reaches. Each comes back after SEAT_TIME with a rest, or after RETURN_DELAY with nothing.
- Draws rests (flow, from served-seats): consumed with RESTED_CAUSE when a seat visit comes back with one.
- Guests (inspection record): the record adds the seat while seated, a SeatPlace, and the pieces remembered as full. The renderer draws seated guests from it, and the Inspector reads it.
- Guest presence: unchanged. A guest waiting at a piece or seated there has no entry, as a guest waiting at a facility has none.

The guest reads nothing of the piece but the fields and the flow. It learns it is seated from the visit's timing, and that the piece is full from the visit coming back empty (principle 6). The new state is part of the guest entity, which every park with guests holds.

## Spec changes

src/sim/guests/SPEC.md:

- Boundary, the read list: add "the best rest seats field, through bestSeatsAt;" after the need offer fields, and "the seat places field, for the pose of the piece the guest heads to or sits at;" after it. Change the stocks line to add "seat-visits and rests" to the list.
- Boundary, after "A guest visits facilities, ...": "A guest also visits pieces of furniture, through seat-visits, and receives rests from them."
- Choosing, the options list, after the offers: "- each piece that best rest seats gives at its place, other than the pieces it remembers as full (Seats);"
- Choosing, after "An offer scores higher...": "A seat scores as an offer does, with the piece's rest relief and no wait. If the guest is heading to a piece, that piece's option scores a little higher."
- Choosing, the last paragraph's last sentence: "A guest also chooses as soon as its target becomes unreachable or stops offering the need it heads there for, when its visit comes back, and when the park closes (Going home)." becomes "A guest also chooses as soon as its target becomes unreachable, stops offering the need it heads there for, or is a piece the field at its place no longer gives, when its visit comes back, and when the park closes (Going home)."
- Walking, after the paragraph beginning "The network chooses a guest's route.": "When a guest heads to a piece of furniture, its route follows the best rest seats field's next steps to the piece's seat place, which may lie inside an edge. Over the route's last stretch, the guest moves toward the piece's side of the paving, as far as its box fits, instead of to its own side. So it stops on the paving's edge beside the piece."
- A new section after Visits and servings:

      ## Seats

      When a guest reaches the seat place of the piece it heads to, it sends the piece one seat visit and waits. If the visit hasn't come back VISIT_DELAY plus RETURN_DELAY ticks after it was sent, the piece has seated the guest, and the guest is seated until the visit comes back. When the visit comes back with a rest, tiredness falls by the rest relief the piece offered when the guest chose it. If it comes back with nothing, the piece was full. The guest remembers it, with the tick it found it full, and chooses in that same tick, wherever it stands. Either way, the guest chooses again at once. It consumes the returned visit with FINISHED_CAUSE and the rest with RESTED_CAUSE.

      A guest remembers a piece as full for FULL_MEMORY_TIME ticks after it found it full, and remembers at most SEAT_CHOICES pieces. If it finds one more piece full while it remembers that many, it forgets the one it found full first. While it remembers a piece, that piece is never one of its options.

- Carrying across an edit: "If it is heading to a facility, it keeps its target until its next walk." becomes "If it is heading to a facility or a piece of furniture, it keeps its target until its next walk."
- Inspection: the record sentence adds "while it is seated and its piece is still laid, the seat place of the piece it sits at, and the pieces it remembers as full, each with the tick it found it full" before "meals eaten". The guestOptions sentence adds "A seat's terms are Relief, Distance, and Commitment, added in that order." after the offer's terms.

src/sim/wayfinding/SPEC.md, Boundary: "Other modules read the fields by sampling BestRestSeats and BestMealSeats, from sim/wayfinding/best_seats.h." becomes "Other modules read the fields by sampling BestRestSeats and BestMealSeats, or through bestSeatsAt, from sim/wayfinding/best_seats.h."

src/sim/wayfinding/SPEC.md, a new paragraph at the end of Best seats:

    bestSeatsAt(world, network, place, use, scratch, out) fills out with the entries of best rest seats or best meal seats at the place, by use. They are exactly the values sampleField gives there, in the same order. It reuses out and the scratch's lists, so it allocates only while they grow. A guest can therefore read the field in every tick.

src/sim/furniture/SPEC.md, Boundary: "and its seats through the seat places field and furnitureSeats" becomes "and its seats through the seat places field, furnitureSeats, and seatPose".

src/sim/furniture/SPEC.md, after the Seat places paragraph:

    seatPose(kind, pose, seat) gives the pose of a seat of a piece of the kind standing at the pose. A bench's two seats lie side by side along it, 0.45 m either side of its middle, facing as the bench faces. A table's four seats lie in pairs on its two long sides, 0.45 m either side of its middle along it and 0.5 m from its middle across it, each facing the middle. Seats are numbered from the left, seen from the paving, and a table's paving side comes first.

src/render/SPEC.md, Boundary: the read of "the furniture through parkFurniture" becomes "the furniture through parkFurniture and seatPose", and "guest_mesh.h for buildGuestMesh and appendGuestEntity" becomes "guest_mesh.h for buildGuestMesh, appendGuestEntity, and SeatedGuests".

src/render/SPEC.md, Guests, replacing its paragraph:

    buildGuestMesh draws each guest that has a position. A guest is drawn as an upright box where it stands, or a box GUEST_SEATED_HEIGHT tall on its seat while it is seated. The box is shaded from blue when every need of the guest is at 0 to pink when its most pressing need is at 1. appendGuestEntity adds one guest's box, where buildGuestMesh draws it, in a given color. It adds nothing when the guest has no position. The light dims a guest's box but never changes its hue (The light).

    SeatedGuests keeps the seat each seated guest is drawn on, from one update to the next. A guest is drawn seated when its record gives a seat place. It keeps its seat while it stays seated at the same piece, whoever else sits down or stands up there. Guests newly seated take the lowest free seat of their piece, in key order, with the seats seatPose gives. If no seat of its piece is free, a guest is drawn standing. The seats are drawing only, and no save holds them. After a park opens, its first update seats guests in key order.

src/render/SPEC.md, Picking: "It uses the shapes the park and guest meshes draw." becomes "It uses the shapes the park and guest meshes draw, with the same seated guests."

src/legible/SPEC.md, Inspectors: the guest's first bullet adds "the pieces it remembers as full, each with how long ago it found it full," after "last meal". After the bullets: "Wherever the inspector names a target or a facility that is a piece of furniture, it names it as a bench or a table with its key. A need's row counts seat options as offers of that need."

src/app/SPEC.md, The scene: after "The guests are rebuilt whenever the world ticks or the park's intent changes, as an edit applied while paused changes it.": "Each seated guest keeps its seat from one rebuild to the next, and a click picks guests where they are drawn."

## Public interface

sim/guests/guests.h:

- GuestActivity gains HeadingToSeat and Seated after HeadingHome, with names "heading-to-seat" and "seated".
- ChoiceKind gains Seat after HeadingHome, with name "seat". The ChoiceOption comment adds that a seat's terms are Relief, Distance, and Commitment.
- `struct FullPiece { EntityKey Piece = NULL_KEY; uint64_t Found = 0; bool operator==(const FullPiece &) const = default; };` with visitFields visiting `piece` and `found`. Its comment: a piece of furniture a guest found full, and the tick it found it full.
- GuestRecord gains `std::optional<SeatPlace> Seat;` and `std::array<FullPiece, SEAT_CHOICES> FullPieces{};`, after Reason. An entry with Piece NULL_KEY is empty, so building a record allocates nothing. guests.h includes `sim/wayfinding/best_seats.h` for SEAT_CHOICES.
- `inline constexpr uint64_t FULL_MEMORY_TIME = 900;` with the comment "Ticks a guest remembers a piece of furniture as full."

sim/wayfinding/best_seats.h:

- `enum class SeatUse : uint8_t { Rest, Meal };` with the comment "Which best seats field a reader samples: best rest seats or best meal seats."
- `void bestSeatsAt(const World &world, const Network &network, const Place &place, SeatUse use, SampleScratch<BestPlace> &scratch, std::vector<BestPlace> &out);` with the comment "Fills out with the use's field's entries at the place, exactly the values sampleField gives there and in its order, or none where the place doesn't resolve. Reuses out and the scratch's lists, so it allocates only while they grow."

sim/furniture/furniture.h:

- `Pose seatPose(FurnitureKind kind, const Pose &piece, uint32_t seat);` with the comment from the spec. Throws std::out_of_range for a seat not below furnitureSeats(kind).

render/guest_mesh.h:

- `inline constexpr float GUEST_SEATED_HEIGHT = 1.2f;`
- `class SeatedGuests { public: void update(const World &world); [[nodiscard]] std::optional<Pose> seatOf(EntityKey guest) const; private: ... };`
- `void appendGuestEntity(ParkMesh &mesh, const World &world, const SeatedGuests &seated, EntityKey key, Rgba color);`
- `ParkMesh buildGuestMesh(const World &world, const SeatedGuests &seated);`

render/picking.h: `std::optional<EntityKey> entityAtCursor(const World &world, const SeatedGuests &seated, const CameraView &view, float aspect, float ndcX, float ndcY);`

## Files affected

- Modify: src/sim/guests/guests.h, src/sim/guests/guests.cpp, src/sim/guests/internal/guest.h, src/sim/guests/internal/route_ahead.h, src/sim/guests/route_ahead.cpp, src/sim/guests/SPEC.md
- Modify: src/sim/wayfinding/best_seats.h, src/sim/wayfinding/best_seats.cpp, src/sim/wayfinding/SPEC.md
- Modify: src/sim/furniture/furniture.h, src/sim/furniture/furniture.cpp, src/sim/furniture/SPEC.md
- Modify: src/render/guest_mesh.h, src/render/guest_mesh.cpp, src/render/picking.h, src/render/picking.cpp, src/render/SPEC.md
- Modify: src/legible/inspect.cpp, src/legible/SPEC.md
- Modify: src/app/scene/scene_sync.h, src/app/scene/scene_sync.cpp, src/app/scene/scene_uploads.h, src/app/scene/scene_uploads.cpp, src/app/input/cursor.h, src/app/input/cursor.cpp, src/app/application.cpp, src/app/SPEC.md
- Modify: src/bench/stages.cpp
- Modify: tests/parks/cut.park, evening.park, noon.park, warm.park, warm-main-street.park, tests/parks/stress/full.park, parks/busted-path.park, parks/jank.park, parks/radii.park

## Dependencies

- navigable-networks/best-seats: the best rest seats field and sampleBestPlaceEdge. Merged.
- plausible-operations/served-seats: seat visits, rests, and RETURN_DELAY. Merged.
- effortless-building/furnished-paths: seat places and full.park's furniture. Merged.

## Out of scope

- Carrying food and eating at a seat: carried-meals.
- Furniture in the day parks, far-tables.park, unfurnished.park, and the world checks' changes: seat-parks.
- The rest overlay reading best rest seats: legible-simulation's explained-seating.
- Retiring the bench box: the slice's last step. Guests still weigh bench boxes' rest offers.
- Moving choose's sampleField of route distance off the per-tick allocation path. It allocated before this feature, and only at a choice. It is in scalable-runtime's deepening candidates.

## Open questions

None.
