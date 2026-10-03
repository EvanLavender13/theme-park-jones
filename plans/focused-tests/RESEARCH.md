# Research: focused-tests

## Why does the suite keep growing tests that enumerate cases or re-prove laws?

An audit read all 761 test cases against the rules now in docs/testing.md. 292 passed as written, 198 asserted a real property in the wrong shape, 231 were deleted, and 40 more re-prove a world-as-value law over real park state and wait for the law suite. Of the deletions, about 65 re-proved a law another module owns, about 65 enumerated inputs, error conditions, or entry points, about 56 pinned tuning, display text, vertex order, or a formula copied from the code, about 47 checked that one function forwards to another, about 20 tested test utilities, and about 14 checked that a function taking `const World &` leaves the world unchanged.

The causes sit upstream of the tests. The sim root proved copy, save, hash, candidate, and determinism laws only on hand-made synthetic schemas, never over the types makeParkSchema registers, so every feature that added state had a real reason to re-prove them for that state. planning-features asked each feature to name a test for every principle it could break, which for principles 1, 8, and 10 is always the same law. Specs fixed tessellation, formulas, and constants as their contract, so tests of them could only be change-detectors. The test-writer sees one feature at a time and cannot know what the suite already proves. Each problem found was answered with a new shape rule in whichever file was nearest, and a shape rule moves enumeration into another shape rather than removing it: the two-section limit pushed lists into separate TEST_CASEs.

No law can catch a field that a type's visitFields leaves out, since copy, equality, hash, and save all read through visitFields; that field is invisible to every law at once.

Rejected: adding more shape rules to the agent prompts — each closes one shape and the enumeration moves to another, as the section limit showed. A hard cap on test count — arbitrary, and a ratchet against a baseline does the same job without punishing growth that is earned.

## How can a law be proven once and inherited by everything that registers state?

Law libraries write each law once and check it against every instance: quickcheck-classes checks a typeclass's laws for any type with one call, and Meszaros's abstract test case, now usually called a contract test, runs one suite against every implementation of an interface. Hypothesis's stateful testing replaces matrices of example tests with one invariant checked over generated operation sequences. The project already has both halves: the walk's registry knows every component type, and tests/sim/support/route_edits.h generates randomized park edit sequences. What is missing is a law test that builds worlds from makeParkSchema, so that every registered type is filled and stepped, and checks each law there once. A feature that registers state is then covered by registering it, and adds to the edit generator when it adds commands.

Rejected: a templated or macro-generated test per component type — still enumeration, when the registry can drive it. Keeping per-feature randomized law runs — they are what the audit removed, and they multiply with every feature.

## How can the suite be kept from regrowing these shapes?

A ratchet commits a snapshot of a few metrics and fails any change that makes one worse, while allowing improvement; existing debt is grandfathered and only shrinks. Candidate metrics are test cases per module, test names longer than a threshold, and calls to the world-law functions outside the law suite. Mutation testing measures redundancy properly: a test is redundant when removing it leaves no mutant surviving that it alone killed. Mull is the C++ tool, but it needs Clang and LLVM IR, and the project builds with GCC, so it is a later option. Studies of LLM-written tests find redundant, verbose tests with duplicate assertions are characteristic of them, which argues for mechanical checks over another agent's judgment as the guard.

Rejected: a ratchet on suite metrics — Evan rejected it: the agents that write tests game a numeric check, as the two-section limit was gamed by splitting lists, and counts such as tests per module rightly grow as the game matures, so a ratchet either blocks honest growth or is worked around. Line coverage as the redundancy measure — it cannot tell a test that asserts from one that merely runs the code. An unaided agent review — it shares the bias of the agents that wrote the tests.

The guard adopted is an audit at each milestone's close, accepted with that risk in view. It differs from an unaided review in three ways: it applies written rules and cites one for every finding, so a judgment can be checked against the text; the auditing agent has fresh context and wrote none of the tests; and its report goes to Evan, who decides what is fixed. A shape the rules did not foresee is answered by changing docs/testing.md through a decision record, not by a new rule in whichever file is nearest.

Sources: https://www.stackage.org/package/quickcheck-classes — laws written once, checked for every instance; https://blog.thecodewhisperer.com/permalink/abstract-test-cases-20-years-later — abstract test cases as contract tests; https://zalas.pl/contract-test/ — contract tests versus test superclasses; https://hypothesis.works/articles/rule-based-stateful-testing/ — invariants over generated operation sequences; https://testing.googleblog.com/2015/01/testing-on-toilet-change-detector-tests.html — tests that mirror the implementation; https://arxiv.org/abs/2410.10628 — test smells in LLM-generated tests; https://arxiv.org/pdf/1809.02435 — mutant subsumption and redundant tests; https://mull.readthedocs.io/en/latest/Features.html — Mull requires Clang; https://github.com/leonkacowicz/ratchet — a metrics ratchet in CI.

## Which existing tests are in the wrong shape?

Each entry is a test the audit judged against docs/testing.md, named by the start of its name. A rewrite is a real property in the wrong shape: the entry gives the property the test should assert, why its present shape breaks the rules, and the tests it merges with. A held entry re-proves a world-as-value law over real park state, and is deleted once the law suite covers that state. A code S1 to S7 cites the rule of the same number in docs/testing.md.

app/command_line_test.cpp

- rewrite: "A command line the app does not accept exits with a nonzero…" Property: A refused command line exits nonzero with no hash line. Nine refusals that parseOptions' test already owns (S4); keep one for main's wiring

app/frame_clock_test.cpp

- rewrite: "advance gives at most MAX_FRAME_SECONDS, which is 0.25…" Property: Dt never exceeds MAX_FRAME_SECONDS; a clamped frame ends at its reading. Pins the 0.25 tuning value (S1)
- rewrite: "No frame steps more than MAX_FRAME_TICKS, which is 2…" Property: No frame steps more than MAX_FRAME_TICKS. Pins the value 2 (S1)

app/options_test.cpp

- rewrite: "parseOptions holds what each given option names and leaves e…" Property: Given options are held and the rest are defaults. The "--frames alone" section repeats "no options" and "every option" (S4)
- rewrite: "parseOptions gives no Options for a command line that prints…" Property: A command line the usage refuses gives no Options. Enumerates 15 refusals, one per error condition (S4)

app/scene/scene_sync_test.cpp

- rewrite: "syncLook returns true at its first call…" Property: syncLook asks exactly on first call, remake, or a look change. Three tests of one rule (S4) Rewritten as one test with "syncLook returns true when told the preview was made again,…", "syncLook returns true exactly when the look differs from the…".
- rewrite: "syncLook returns true when told the preview was made again,…" Property: syncLook asks exactly on first call, remake, or a look change. One trigger of the same rule Rewritten as one test with "syncLook returns true at its first call…", "syncLook returns true exactly when the look differs from the…".
- rewrite: "syncLook returns true exactly when the look differs from the…" Property: syncLook asks exactly on first call, remake, or a look change. Sections enumerate each field of the look (S4) Rewritten as one test with "syncLook returns true at its first call…", "syncLook returns true when told the preview was made again,…".

app/session/park_file_test.cpp

- held: "A park saved with saveParkFile and opened with openParkFile…" Property: S2. Re-proves save/load round-trip; owner: sim root (save)
- rewrite: "openParkFile of a path it cannot read gives no world and a m…" Property: A failed file operation gives no world and names the path. Exact message prefix (S1); one test per error condition (S4) Rewritten as one test with "openParkFile of text loadWorld refuses gives no world and a…", "saveParkFile to a path it cannot write gives a message namin…".
- rewrite: "openParkFile of text loadWorld refuses gives no world and a…" Property: A failed file operation gives no world and names the path. Another error condition Rewritten as one test with "openParkFile of a path it cannot read gives no world and a m…", "saveParkFile to a path it cannot write gives a message namin…".
- rewrite: "saveParkFile to a path it cannot write gives a message namin…" Property: A failed file operation gives no world and names the path. Another error condition Rewritten as one test with "openParkFile of a path it cannot read gives no world and a m…", "openParkFile of text loadWorld refuses gives no world and a…".

app/session/park_session_test.cpp

- rewrite: "useFileRequest with New makes the world resolvedNewPark() an…" Property: Replacing the world empties the queue. World equality is S5 Rewritten as one test with "useFileRequest with Open of a file openParkFile opens makes…".
- rewrite: "useFileRequest with Open of a file openParkFile opens makes…" Property: Replacing the world empties the queue. The same property through another entry point (S4) Rewritten as one test with "useFileRequest with New makes the world resolvedNewPark() an…".

integration/food_loop_test.cpp

- rewrite: "Once warm.park has served a guest that cut.park does not, mea…" Property: Cutting a shop's supply route leaves guests hungrier than the same park uncut. The right S7 consequence, but it runs a fixed HUNGER_TICKS (S1). Run until the uncut park has served a guest the cut one has not, and cut in the test from warm.park.

integration/park_laws_test.cpp

- held: "Every park file loads and saves back to identical text…" Property: S2. The "saving again gives identical text" law, owned by the sim root (save_test).
- held: "A world stepped from every park file equals the world regener…" Property: S2. The "save loads back and resolves equal" law, owned by the sim root.
- held: "Two runs of every park file, stepped side by side, end with t…" Property: S2. The "two runs give the same state" law, owned by the sim root. The cross-build check covers determinism across builds.

integration/preview_test.cpp

- held: "In warm.park, the preview of a second shop touching both path…" Property: S2, S1. The main assert is the candidate-equals-commit law (sim root). The availability rise is one hand-built placement, composed of that law and legible/food's contribution rule.

legible/food_test.cpp

- rewrite: "Each contribution holds its shop's route distance and offer,…" Property: A contribution's distance is the guest route distance. Time and Term restate the formula (S1), and it sweeps two parks
- rewrite: "A place with no contributions, including one that does not r…" Property: A place off the guest network has no contributions. Enumerates four error conditions (S4); empty-sum-is-0 is already the sum law
- rewrite: "foodDiscount is the curve's Y at each of its points…" Property: foodDiscount never rises with time and stays within [0, 1]. Restates the authored curve's points (S1) Rewritten as one test with "foodDiscount interpolates linearly strictly between consecut…", "foodDiscount is 1 before the curve's first point, 0 beyond i…".
- rewrite: "foodDiscount interpolates linearly strictly between consecut…" Property: foodDiscount never rises with time and stays within [0, 1]. Restates the interpolation formula (S1 change-detector) Rewritten as one test with "foodDiscount is the curve's Y at each of its points…", "foodDiscount is 1 before the curve's first point, 0 beyond i…".
- rewrite: "foodDiscount is 1 before the curve's first point, 0 beyond i…" Property: foodDiscount never rises with time and stays within [0, 1]. ; NaN gives 0 may stay The 1 and 0 ends are the curve's tuned endpoints (S1) Rewritten as one test with "foodDiscount is the curve's Y at each of its points…", "foodDiscount interpolates linearly strictly between consecut…".

legible/inspect_test.cpp

- rewrite: "A guest's choice table has a row for each option guestOption…" Property: One choice row per option guestOptions gives, in order. Restates the two-decimal formatting (S1) and cross-multiplies 2 worlds by 3 guests

legible/park_summary_test.cpp

- rewrite: "summarizePark counts the guests, the mean of their hunger, a…" Property: Guests and Waiting count the guests' records. Expects warm.park's fixed 32 and 5 (S1); assert against the records
- rewrite: "summarizePark gives no guests, a mean hunger of 0, and none…" Property: Guests and Waiting count the guests' records. The empty case of the same counting property (S4)

legible/path_place_test.cpp

- rewrite: "nearestGuestPathPlace gives none for a point that is not fin…" Property: Gives none when no guest path place exists for the point. One test per error condition (S4) Rewritten as one test with "nearestGuestPathPlace gives none in a world whose guest netw…".
- rewrite: "nearestGuestPathPlace gives none in a world whose guest netw…" Property: Gives none when no guest path place exists for the point. A second error condition, itself swept over two worlds (S4) Rewritten as one test with "nearestGuestPathPlace gives none for a point that is not fin…".
- rewrite: "foodNear gives the food availability at the nearest guest pa…" Property: foodNear answers exactly when the point is within reach. Equality with foodAvailability is S5 Rewritten as one test with "foodNear gives none beyond the reach, or where there is no n…".
- rewrite: "foodNear gives none beyond the reach, or where there is no n…" Property: foodNear answers exactly when the point is within reach. Re-enumerates nearestGuestPathPlace's error conditions (S4) Rewritten as one test with "foodNear gives the food availability at the nearest guest pa…".

legible/preview_test.cpp

- rewrite: "A preview's candidate is the candidate of its edit when the…" Property: A preview has a candidate exactly when the edit is accepted. Equality with makeCandidate is S5; the candidate law is the sim root's
- rewrite: "shopContext of an AddBox of a shop gives a context for the l…" Property: An AddBox context names the added shop. The two-shop section pins the arbitrary lowest-key choice (S1)
- rewrite: "shopContext gives none for an edit that neither adds a shop…" Property: An edit that places no shop has no context. Adds enumerated inconsistent-input cases (S4)
- rewrite: "A shop context's connection is the first guest path place at…" Property: Connection is where the shop's connector meets the guest path. The junction section pins the arbitrary first-place order (S1)
- rewrite: "A shop context's supply is the nearest depot to its shop in…" Property: Context supply is the shop's supply once the edit commits. Keep the moved-off-backstage section; the added-shop section is S5
- rewrite: "keepPreview with nothing kept makes the preview, keeping pre…" Property: After keepPreview, Made is previewEdit of the world and edit. as the single coherence property
- rewrite: "keepPreview makes the preview again when the kept tick or ed…" Property: After keepPreview, Made is previewEdit of the world and edit. Sections enumerate each trigger (S4)

render/food_overlay_test.cpp

- rewrite: "foodColor of a positive value interpolates the ramp's stops,…" Property: Color saturates at OVERLAY_FULL and distinguishes values below it. Restates quarter stops and linear interpolation (S1).
- rewrite: "The overlay holds, for each guest path's line in key order,…" Property: the band covers ground within OVERLAY_BAND of each guest path, facing up, and nothing over connectors or backstage paths. Pins per-sample vertex and triangle counts and key-order layout (S1). Rewritten as one test with "Each row's center lies on top of the tent at its sample's gr…", "Each end cone fans OVERLAY_BAND around its end's ground poin…", "Every cone triangle faces up, and so does every triangle joi…".
- rewrite: "Each row's center lies on top of the tent at its sample's gr…" Property: the band covers ground within OVERLAY_BAND of each guest path, facing up, and nothing over connectors or backstage paths. Restates row vertex positions by index (S1). Rewritten as one test with "The overlay holds, for each guest path's line in key order,…", "Each end cone fans OVERLAY_BAND around its end's ground poin…", "Every cone triangle faces up, and so does every triangle joi…".
- rewrite: "Each end cone fans OVERLAY_BAND around its end's ground poin…" Property: the band covers ground within OVERLAY_BAND of each guest path, facing up, and nothing over connectors or backstage paths. Restates the cos/sin cone formula and its 17-vertex layout (S1). Rewritten as one test with "The overlay holds, for each guest path's line in key order,…", "Each row's center lies on top of the tent at its sample's gr…", "Every cone triangle faces up, and so does every triangle joi…".
- rewrite: "Every overlay vertex points up and has the color of the valu…" Property: The band shades each spot by the value at its nearest guest path place. Property is real, but vertices are located through the exact index layout (S1).
- rewrite: "Every cone triangle faces up, and so does every triangle joi…" Property: the band covers ground within OVERLAY_BAND of each guest path, facing up, and nothing over connectors or backstage paths. Upward facing is lasting but is checked through the RowStart/cone index layout (S1). Rewritten as one test with "The overlay holds, for each guest path's line in key order,…", "Each row's center lies on top of the tent at its sample's gr…", "Each end cone fans OVERLAY_BAND around its end's ground poin…".
- rewrite: "Between rows of a straight segment the surface falls linearl…" Property: Where bands overlap, the nearer line's surface is higher. Restates the linear tent formula and finds triangles by index layout (S1).

render/ghost_mesh_test.cpp

- rewrite: "GHOST_ALPHA is 0.5, and the tints are translucent and distin…" Property: Ghost alpha and tints are translucent and differ in hue from each other and from the kinds. Pins GHOST_ALPHA == 0.5 (S1); keep the translucency and distinctness.
- rewrite: "An AddBox's own ghost is its box in its kind's color at GHOS…" Property: a ghost is translucent in its kind's color when accepted and INVALID_TINT when refused. One of three near-identical per-edit-type tests (S4). Rewritten as one test with "A MoveBox's own ghost is the box its key holds at the new po…", "An AddPath's own ghost is its ribbon and joints in its kind'…".
- rewrite: "A MoveBox's own ghost is the box its key holds at the new po…" Property: a ghost is translucent in its kind's color when accepted and INVALID_TINT when refused. , with no ghost when the key holds no box Same shape as another test for another edit type (S4). Rewritten as one test with "An AddBox's own ghost is its box in its kind's color at GHOS…", "An AddPath's own ghost is its ribbon and joints in its kind'…".
- rewrite: "An AddPath's own ghost is its ribbon and joints in its kind'…" Property: a ghost is translucent in its kind's color when accepted and INVALID_TINT when refused. Same shape as another test for another edit type (S4). Rewritten as one test with "An AddBox's own ghost is its box in its kind's color at GHOS…", "A MoveBox's own ghost is the box its key holds at the new po…".
- rewrite: "An accepted AddBox or MoveBox's own ghost has, in order, the…" Property: an accepted edit's ghost has the geometry its commit draws. Real property, but slices by BOX_VERTICES and buildParkMesh's draw order (S1). Rewritten as one test with "An accepted AddPath's own ghost, joints included, has, in or…", "An accepted edit's ghost walkways have, in order, the positi…", "An accepted edit's ghost marks have, in order, the positions…".
- rewrite: "An accepted AddPath's own ghost, joints included, has, in or…" Property: an accepted edit's ghost has the geometry its commit draws. Slices by entrancesAndPaths order and key-order placement (S1). Rewritten as one test with "An accepted AddBox or MoveBox's own ghost has, in order, the…", "An accepted edit's ghost walkways have, in order, the positi…", "An accepted edit's ghost marks have, in order, the positions…".
- rewrite: "An accepted edit's ghost walkways have, in order, the positi…" Property: an accepted edit's ghost has the geometry its commit draws. Walkways found by slicing at draw-order offsets (S1). Rewritten as one test with "An accepted AddBox or MoveBox's own ghost has, in order, the…", "An accepted AddPath's own ghost, joints included, has, in or…", "An accepted edit's ghost marks have, in order, the positions…".
- rewrite: "An accepted edit's ghost marks have, in order, the positions…" Property: an accepted edit's ghost has the geometry its commit draws. Marks found by slicing at 24 vertices from the end (S1). Rewritten as one test with "An accepted AddBox or MoveBox's own ghost has, in order, the…", "An accepted AddPath's own ghost, joints included, has, in or…", "An accepted edit's ghost walkways have, in order, the positi…".
- rewrite: "A ghost given a candidate world is the edit's own ghost foll…" Property: a ghost draws the walkways and marks of the candidate it is given, and none without one Real API contract, but asserted as an exact concatenation (S1). : a ghost draws the walkways and marks of the candidate it is given, and none without one Real API contract, but asserted as an exact concatenation (S1). Rewritten as one test with "A ghost given no candidate world is the edit's own ghost alo…".
- rewrite: "A ghost given no candidate world is the edit's own ghost alo…" Property: a ghost draws the walkways and marks of the candidate it is given, and none without one Real API contract, but asserted as an exact concatenation (S1). The other half of C, asserted by exact mesh equality. Rewritten as one test with "A ghost given a candidate world is the edit's own ghost foll…".

render/guest_mesh_test.cpp

- rewrite: "guestColor moves each of red, green, and blue from the sated…" Property: Sated (≤ 0) shows the sated color, hungry (≥ 1) the hungry color. Restates the linear interpolation formula (S1).
- rewrite: "appendGuest adds exactly what appendBox adds for an upright…" Property: each placed guest is drawn as a solid standing at its position in its hunger's color; unplaced guests are not drawn Asserts delegation to appendBox (S5). : each placed guest is drawn as a solid standing at its position in its hunger's color; unplaced guests are not drawn Asserts delegation to appendBox (S5). Rewritten as one test with "buildGuestMesh adds appendGuest for each guest whose record…".
- rewrite: "buildGuestMesh adds appendGuest for each guest whose record…" Property: each placed guest is drawn as a solid standing at its position in its hunger's color; unplaced guests are not drawn Asserts delegation to appendBox (S5). Pins key order by rebuilding the expected mesh (S1); reaches its state by stepping 2*ARRIVAL_INTERVAL + 1 ticks (tuning-dependent). Rewritten as one test with "appendGuest adds exactly what appendBox adds for an upright…".
- rewrite: "appendGuestEntity adds exactly what appendBox adds at guestP…" Property: A guest's highlight lies exactly where the guest mesh draws that guest. Asserts delegation to appendBox (S5); compare with buildGuestMesh's geometry instead.

render/park_mesh_test.cpp

- rewrite: "A guest path lies 4 cm above the ground and a backstage path…" Property: Guest ribbon clears backstage ribbon by at least backstage's lift. Pins 4 cm and 2 cm tuning values (S1); keep only the inequality that stops flicker.
- rewrite: "appendPath begins with two vertices per ground line point, l…" Property: a ribbon covers ground within half width of its line, facing up, at its lift. Pins vertex order (left then right, 2n first) and restates the tangent formula (S1). Rewritten as one test with "appendPath's first triangles cover the quad between each pai…", "appendPath follows its ribbon with a round joint beyond its…", "appendWalkway begins with two vertices per point, left edge…", "appendWalkway's first triangles cover the quad between each…", "appendWalkway follows its ribbon with a round joint beyond i…".
- rewrite: "appendPath's first triangles cover the quad between each pai…" Property: a ribbon covers ground within half width of its line, facing up, at its lift. Pins two-triangles-per-quad index layout (S1); coverage is the lasting part. Rewritten as one test with "appendPath begins with two vertices per ground line point, l…", "appendPath follows its ribbon with a round joint beyond its…", "appendWalkway begins with two vertices per point, left edge…", "appendWalkway's first triangles cover the quad between each…", "appendWalkway follows its ribbon with a round joint beyond i…".
- rewrite: "appendPath follows its ribbon with a round joint beyond its…" Property: a ribbon covers ground within half width of its line, facing up, at its lift. Pins 17 rim vertices, vertex/index counts, and restates the cos/sin rim formula (S1). Rewritten as one test with "appendPath begins with two vertices per ground line point, l…", "appendPath's first triangles cover the quad between each pai…", "appendWalkway begins with two vertices per point, left edge…", "appendWalkway's first triangles cover the quad between each…", "appendWalkway follows its ribbon with a round joint beyond i…".
- rewrite: "appendWalkway begins with two vertices per point, left edge…" Property: a ribbon covers ground within half width of its line, facing up, at its lift. Same vertex-order and tangent-formula restatement as another test (S1). Rewritten as one test with "appendPath begins with two vertices per ground line point, l…", "appendPath's first triangles cover the quad between each pai…", "appendPath follows its ribbon with a round joint beyond its…", "appendWalkway's first triangles cover the quad between each…", "appendWalkway follows its ribbon with a round joint beyond i…".
- rewrite: "appendWalkway's first triangles cover the quad between each…" Property: a ribbon covers ground within half width of its line, facing up, at its lift. Same index-layout pin as another test (S1). Rewritten as one test with "appendPath begins with two vertices per ground line point, l…", "appendPath's first triangles cover the quad between each pai…", "appendPath follows its ribbon with a round joint beyond its…", "appendWalkway begins with two vertices per point, left edge…", "appendWalkway follows its ribbon with a round joint beyond i…".
- rewrite: "appendWalkway follows its ribbon with a round joint beyond i…" Property: a ribbon covers ground within half width of its line, facing up, at its lift. , plus walkway meets its path with no notch Pins rim count and restates rim formula (S1). Rewritten as one test with "appendPath begins with two vertices per ground line point, l…", "appendPath's first triangles cover the quad between each pai…", "appendPath follows its ribbon with a round joint beyond its…", "appendWalkway begins with two vertices per point, left edge…", "appendWalkway's first triangles cover the quad between each…".
- rewrite: "appendWalkway given a face's normal moves its first left and…" Property: A walkway's start lies flush on its door's face line. Restates the move formula h*dot(r,n)/dot(t,n) (S1); assert dot(v - p, n) = 0 instead.
- rewrite: "appendWalkway given a face's normal keeps its square start w…" Property: A face normal never folds the walkway's start. Pins the square-start fallback and enumerates its two triggers (S1, S4); assert the first quad still faces up.
- rewrite: "appendWalkways adds a walkway for each connector, guest netw…" Property: Each connector, and nothing else, gets a walkway in its kind's color at the given alpha. Pins guest-then-backstage, key-order concatenation by rebuilding the expected mesh (S1).
- rewrite: "appendBox adds a top and four sides over the footprint's cor…" Property: a box covers its footprint's top and four sides, each triangle wound toward its outward normal. Pins 20 vertices and another test indices (S1). Rewritten as one test with "appendBox covers each face with two triangles wound toward i…".
- rewrite: "appendBox covers each face with two triangles wound toward i…" Property: a box covers its footprint's top and four sides, each triangle wound toward its outward normal. Pins 20/30 and two triangles per face (S1); winding toward the normal is the lasting part. Rewritten as one test with "appendBox adds a top and four sides over the footprint's cor…".
- rewrite: "buildParkMesh appends each entrance, then each path, then th…" Property: The park mesh draws every entrance, path, walkway, box, and starved mark. Pins append order by rebuilding the expected mesh (S1).
- rewrite: "A starved mark is a 1.5 m cube whose bottom floats at 5 m, a…" Property: A starved mark floats clear above a shop's roof. Pins 1.5 m and 5 m (S1); keep STARVED_MARK_BASE > boxHeight(Shop).
- rewrite: "appendStarvedMark first adds the faces appendBox adds for a…" Property: a starved mark is a closed cube over its pose, every triangle wound toward its outward normal. Pins face order and that it reuses appendBox's layout (S1). Rewritten as one test with "appendStarvedMark then adds a bottom face: four vertices at…".
- rewrite: "appendStarvedMark then adds a bottom face: four vertices at…" Property: a starved mark is a closed cube over its pose, every triangle wound toward its outward normal. Pins 24/36 counts and the bottom's index position (S1); closedness is the lasting part. Rewritten as one test with "appendStarvedMark first adds the faces appendBox adds for a…".
- rewrite: "appendStarvedMarks adds appendStarvedMark over each starved…" Property: A mark over each starved shop and none over supplied shops or depots, in STARVED_COLOR at the given alpha. Pins key order by rebuilding the expected mesh (S1).

render/picking_test.cpp

- rewrite: "groundAtCursor gives none when the eye is not above the grou…" Property: groundAtCursor gives none exactly when the cursor's ray never meets the ground One test per failure condition (S4). : groundAtCursor gives none exactly when the cursor's ray never meets the ground One test per failure condition (S4). Rewritten as one test with "groundAtCursor gives none when the ray through the cursor do…".
- rewrite: "groundAtCursor gives none when the ray through the cursor do…" Property: groundAtCursor gives none exactly when the cursor's ray never meets the ground One test per failure condition (S4). Second failure-condition twin of another test (S4). Rewritten as one test with "groundAtCursor gives none when the eye is not above the grou…".
- rewrite: "rayEntry is the least ray parameter at which a ray from outs…" Property: rayEntry is the least t ≥ 0 at which the ray is in the closed box the pose bounds, none if never Five tests of one property with different rays (S4). : rayEntry is the least t ≥ 0 at which the ray is in the closed box the pose bounds, none if never Five tests of one property with different rays (S4). Rewritten as one test with "rayEntry counts the box's edges as inside it…", "rayEntry is 0 for a ray starting in the box…", "rayEntry gives none for a ray that never reaches the box…", "rayEntry follows the footprint the pose's facing turns…".
- rewrite: "rayEntry counts the box's edges as inside it…" Property: rayEntry is the least t ≥ 0 at which the ray is in the closed box the pose bounds, none if never Five tests of one property with different rays (S4). Boundary input of the same property (S4). Rewritten as one test with "rayEntry is the least ray parameter at which a ray from outs…", "rayEntry is 0 for a ray starting in the box…", "rayEntry gives none for a ray that never reaches the box…", "rayEntry follows the footprint the pose's facing turns…".
- rewrite: "rayEntry is 0 for a ray starting in the box…" Property: rayEntry is the least t ≥ 0 at which the ray is in the closed box the pose bounds, none if never Five tests of one property with different rays (S4). Input case of the same property (S4). Rewritten as one test with "rayEntry is the least ray parameter at which a ray from outs…", "rayEntry counts the box's edges as inside it…", "rayEntry gives none for a ray that never reaches the box…", "rayEntry follows the footprint the pose's facing turns…".
- rewrite: "rayEntry gives none for a ray that never reaches the box…" Property: rayEntry is the least t ≥ 0 at which the ray is in the closed box the pose bounds, none if never Five tests of one property with different rays (S4). Input case of the same property (S4). Rewritten as one test with "rayEntry is the least ray parameter at which a ray from outs…", "rayEntry counts the box's edges as inside it…", "rayEntry is 0 for a ray starting in the box…", "rayEntry follows the footprint the pose's facing turns…".
- rewrite: "rayEntry follows the footprint the pose's facing turns…" Property: rayEntry is the least t ≥ 0 at which the ray is in the closed box the pose bounds, none if never Five tests of one property with different rays (S4). Input case of the same property (S4). Rewritten as one test with "rayEntry is the least ray parameter at which a ray from outs…", "rayEntry counts the box's edges as inside it…", "rayEntry is 0 for a ray starting in the box…", "rayEntry gives none for a ray that never reaches the box…".

sim/cycle_test.cpp

- rewrite: "a new world is pending until resolveWorld calls every resolv…" Property: one test, "a resolution runs every resolver, then every finisher, once each in registration order". Same resolver-order assertion as another test's resolveWorld section, and another test already shows a new world resolving first. Rewritten as one test with "every resolution runs each finisher once, after all of its r…".
- rewrite: "a queue holding an unregistered command type is refused, nam…" Property: S4: unregistered command refused before any change. Sections repeat the test per entry point (stepWorld, makeCandidate).
- rewrite: "makeCandidate applies the commands in submission order and r…" Property: a candidate equals the world that commits the same commands, shown with commands whose order matters, which also shows they apply in submission order. The candidate law with order-sensitive commands proves this. The journal pins call order.
- rewrite: "isStepping is true while a system runs and false otherwise…" Property: one test, "isStepping, isResolving, and isFinishing are each true exactly while their stage runs". isResolving has no test today. Same shape as another test, one flag per test. Rewritten as one test with "isFinishing is true while a finisher runs and false otherwise…".
- rewrite: "isFinishing is true while a finisher runs and false otherwise…" Property: one test, "isStepping, isResolving, and isFinishing are each true exactly while their stage runs". isResolving has no test today. Same shape as another test. Rewritten as one test with "isStepping is true while a system runs and false otherwise…".
- rewrite: "every resolution runs each finisher once, after all of its r…" Property: one test, "a resolution runs every resolver, then every finisher, once each in registration order". Sections repeat the test per entry point (resolveWorld, cycle, candidate, load). Rewritten as one test with "a new world is pending until resolveWorld calls every resolv…".

sim/draw_test.cpp

- rewrite: "drawUniform is the draw's top 53 bits times 2^-53, in [0, 1)…" Property: drawUniform lies in [0, 1). Restates the bit formula (S1). Keep only the range.
- rewrite: "drawPick refuses weights it cannot pick from in proportion…" Property: drawPick refuses weights with no proportional pick. About 12 hand-listed error conditions and boundaries (S4).
- held: "a copy of a world draws the same values as the world…" Property: S2. Follows from the copy law and key-only draws.
- held: "a candidate draws the same values as the world its commands…" Property: S2. Re-proves the candidate law for draws.

sim/fp_environment_test.cpp

- rewrite: "in debug builds, constructing a World throws unless rounding…" Property: one test, "in debug builds, constructing a World by any route refuses a non-default floating-point environment". One rounding mode per section. Rewritten as one test with "in debug builds, constructing a World throws when subnormal…", "in debug builds, constructing a World throws when subnormal…", "in debug builds, every way of constructing a World checks th…".
- rewrite: "in debug builds, constructing a World throws when subnormal…" Property: one test, "in debug builds, constructing a World by any route refuses a non-default floating-point environment". Same shape, another deviation. Rewritten as one test with "in debug builds, constructing a World throws unless rounding…", "in debug builds, every way of constructing a World checks th…".
- rewrite: "in debug builds, constructing a World throws when subnormal…" Property: one test, "in debug builds, constructing a World by any route refuses a non-default floating-point environment". Same shape, another deviation. Rewritten as one test with "in debug builds, constructing a World throws unless rounding…", "in debug builds, every way of constructing a World checks th…".
- rewrite: "in debug builds, every way of constructing a World checks th…" Property: one test, "in debug builds, constructing a World by any route refuses a non-default floating-point environment". Sections repeat it per constructor. Rewritten as one test with "in debug builds, constructing a World throws unless rounding…", "in debug builds, constructing a World throws when subnormal…", "in debug builds, constructing a World throws when subnormal…".

sim/guests/add_guest_test.cpp

- rewrite: "addGuest throws std::invalid_argument for a place that does…" Property: addGuest refuses a place off the guest network, changing nothing. Three sections, one per way of not resolving (S4). Keep one input.
- held: "addGuest with the same arguments on two equal worlds leaves…" Property: S2. Re-proves the law that two runs give the same state, owned by the sim root (cycle_test).

sim/guests/arrivals_test.cpp

- rewrite: "Guests arrive exactly in the cycles stepping a tick one belo…" Property: Every connected entrance admits guests alike; an unconnected one admits none. Asserts the guest count tick by tick against ARRIVAL_INTERVAL, a tuning constant and timing (S1).
- rewrite: "A cycle's arrivals take keys from the counter in entrance ke…" Property: each arrival starts wandering at its own entrance's node. Key-counter and entrance-key order are implementation order (S1), and it loops over two arrivals (S4); keep only the per-entrance placement. Rewritten as one test with "A new guest's record shows it wandering at its entrance's no…".
- rewrite: "A new guest's record shows it wandering at its entrance's no…" Property: each arrival starts wandering at its own entrance's node. Enumerates every initial field (S4) and restates the stay and hunger draws through drawnStayUntil and drawnStartingHunger (S1). Rewritten as one test with "A cycle's arrivals take keys from the counter in entrance ke…".

sim/guests/choice_test.cpp

- rewrite: "Strictly between consecutive points a and b of HUNGER_CURVE,…" Property: hungerCurve is non-decreasing, and lies between the neighbouring points' values. Today it restates the interpolation formula bit for bit, "computed in that order" (S1).
- rewrite: "softmaxPick sets each option's Probability to its weight sim…" Property: Probabilities sum to 1, rank with Score, and tie for equal scores. Re-implements the weight formula (S1), and "returns drawPick's index" is forwarding (S5).
- rewrite: "A guest chooses where it stands when it starts its walk at a…" Property: A guest's new Activity and Target are always one of the options guestOptions lists where it chose. Predicts the exact pick through test::choiceOptions and checkPick, which copy the scoring formula and draw key (S1).
- rewrite: "A guest heading to a shop that keeps its target comes WALK_S…" Property: a walking guest covers WALK_STEP of route distance per cycle however the network is cut into edges, so a heading guest's route distance to its destination falls each cycle until it arrives. Three tests run the same shape for three activities (S4), and another test also restates the reflect-at-dead-end arithmetic (S1). Same shape as another test, for another activity (S4).
- rewrite: "A guest heading to a shop whose offer stops being reachable…" Property: a guest never keeps heading for a destination it has no route to, and takes heading home up again once a route exists. Today it is split by cause into sections and tests (S4), and another test checks the exact pick through the test copy of the scoring formula (S1). Two sections, one per cause (S4), and the exact pick comes from the copied formula (S1). Rewritten as one test with "A guest heading home that has no entrance entry at its place…".
- rewrite: "A guest heading home that has no entrance entry at its place…" Property: a guest never keeps heading for a destination it has no route to, and takes heading home up again once a route exists. Today it is split by cause into sections and tests (S4), and another test checks the exact pick through the test copy of the scoring formula (S1). The same property as another test, for heading home (S4). Rewritten as one test with "A guest heading to a shop whose offer stops being reachable…".

sim/guests/footfall_test.cpp

- rewrite: "A stretch some guest lies in after a cycle holds V + (S - V)…" Property: each stretch's value moves toward its guests' summed hunger (toward 0 when empty). Exact equality to the EMA formula with tuning constant FOOTFALL_TIME restates the implementation (S1). Rewritten as one test with "A kept entry of hungry footfall reads, k ticks after its tic…".
- rewrite: "A kept entry of hungry footfall reads, k ticks after its tic…" Property: same property, the empty-stretch half (decays toward 0, never grows). Exact equality to simExp(k * simLog(1 - 1/FOOTFALL_TIME)) is the formula restated with a tuning constant (S1). Rewritten as one test with "A stretch some guest lies in after a cycle holds V + (S - V)…".

sim/guests/guest_edits_test.cpp

- held: "Every world randomized park edits with guests walking, waitin…" Property: S2. The copy-equals-world law, owned by the sim root.
- held: "Every world randomized park edits with guests walking, waitin…" Property: S2. The save-loads-back-equal law, owned by the sim root (save_test).
- held: "A candidate made with an edit from a world of randomized park…" Property: S2. The candidate-equals-commit law, owned by the sim root (cycle_test).
- rewrite: "A candidate holds exactly its world's guests, each carried ov…" Property: An edit changes nothing of a guest but its place, and a surviving carrier keeps its ground point. Restates the carry rule's fallback chain (S1), and has four sections, one per edit (S4).

sim/guests/guest_options_test.cpp

- rewrite: "guestOptions gives none for a key that holds no guest…" Property: guestOptions gives none exactly when the key holds no guest standing on the guest network. Today it is one test per error condition, and another test also enumerates five key kinds (S4). Enumerates five key kinds, and is one of two error-condition tests (S4). Rewritten as one test with "guestOptions gives none for a guest whose place does not res…".
- rewrite: "guestOptions gives none for a guest whose place does not res…" Property: guestOptions gives none exactly when the key holds no guest standing on the guest network. Today it is one test per error condition, and another test also enumerates five key kinds (S4). The second error-condition test for the same property (S4). Rewritten as one test with "guestOptions gives none for a key that holds no guest…".
- rewrite: "guestOptions gives the options Choice lists at the guest's pl…" Property: An offer scores higher when the guest is hungrier or the offer nearer or quicker, and heading home appears only past StayUntil. Compares to test::choiceOptions, which copies the scoring formula and its weights (S1).

sim/guests/visits_test.cpp

- rewrite: "A guest creates guest-visits units only in a cycle that ends…" Property: A guest sends one visit, to its Target, only when it starts waiting at that shop. Asserts VISIT_DELAY == 1 and that the shop holds the visit at the cycle's end, which is tuning and timing (S1).
- rewrite: "A served guest consumes its one meal as eaten, its MealsEaten…" Property: Eating a meal lowers the guest's hunger, never below 0, and counts the meal. Restates the hunger arithmetic with MEAL_RELIEF and the drawn rate (S1).

sim/guests/walks_test.cpp

- rewrite: "A guest's Hunger rises each cycle by its own rate, drawn when…" Property: An unfed guest's hunger never falls and never exceeds 1. Restates min(1, h + drawn rate) through the draw formula (S1).
- rewrite: "A wandering guest walks WALK_STEP each cycle along the carrie…" Property: a walking guest covers WALK_STEP of route distance per cycle however the network is cut into edges, so a heading guest's route distance to its destination falls each cycle until it arrives. Three tests run the same shape for three activities (S4), and another test also restates the reflect-at-dead-end arithmetic (S1). Re-implements the reflect-at-dead-end arithmetic on one line park (S1), and shares the grouped property shape (S4). Rewritten as one test with "A guest heading home with an entrance entry at its place come…".
- rewrite: "A guest heading home with an entrance entry at its place come…" Property: a walking guest covers WALK_STEP of route distance per cycle however the network is cut into edges, so a heading guest's route distance to its destination falls each cycle until it arrives. Three tests run the same shape for three activities (S4), and another test also restates the reflect-at-dead-end arithmetic (S1). Same shape as another test, for heading home (S4). Rewritten as one test with "A wandering guest walks WALK_STEP each cycle along the carrie…".
- rewrite: "At a node, a wandering guest leaves by the step drawPick pick…" Property: At a junction, a wanderer never takes its way back when another step exists. Re-derives the draw key and the step order to predict the exact step (S1).
- rewrite: "A guest with no entrance entry at its place wanders on past i…" Property: a guest never keeps heading for a destination it has no route to, and takes heading home up again once a route exists. Today it is split by cause into sections and tests (S4), and another test checks the exact pick through the test copy of the scoring formula (S1). Its distinct half is the grouped property. Its walk-step half repeats the grouped property (S4).

sim/medium/carry_over_test.cpp

- rewrite: "carryOver moves a place whose carrier's points changed to th…" Property: Changed geometry carries to nearestPlaceOn of old ground point. Four SECTIONs, one per kind of change; show it on one or two reasoned inputs.

sim/medium/field_index_test.cpp

- rewrite: "every slot's kept order by place equals orderByPlace of its…" Property: A slot's order by place equals orderByPlace of its entries. Name and body list every transition (commands, steps, swaps, copies, loads); state the invariant once.

sim/medium/field_test.cpp

- rewrite: "addField registers the field's derived component as <name>-r…" Property: addField registers -resolved Derived, -stepped State, -field. absorb stepped_field_test's -stepped State check.
- rewrite: "isResolving is true while a resolver runs and false otherwis…" Property: one test, "isStepping, isResolving, and isFinishing are each true exactly while their stage runs". isResolving has no test today. isResolving belongs with the cycle's stage flags, and nothing else tests it.
- rewrite: "a publishResolved that throws leaves the field's entries unc…" Property: A refused field publication changes nothing. two SECTIONs with several refusals, show one refused call.
- held: "resolved entries never appear in a save, and loading the sav…" Property: S2. Save holds no derived data (save_test ) and load-resolve equals (save_test ).
- rewrite: "publishing the same sources in different orders gives the sa…" Property: Publication order does not change the stored field. cover both layers (absorb stepped ), compare worldsEqual only, drop the hash (S2).
- rewrite: "at a place that resolves to a node, a source's sampled entri…" Property: At a node, entries at the node's stop places, source order. add field_index's -0.0, repeated-place and NaN inputs.
- rewrite: "at a place strictly inside an edge, a field without sampleEd…" Property: Inside an edge, entries at exactly the place. add the nextafter-of-a-stop inputs from another test.
- rewrite: "sampleEdge is given the edge, the place's offsets as resolve…" Property: What sampleEdge is given: edge, offsets, From/To/inside entries. add field_index's entries nearest each end.
- held: "a candidate made with makeCandidate samples every field as t…" Property: S2. Candidate-equals-commit is a world-as-value law (cycle_test ).

sim/medium/flow_test.cpp

- rewrite: "an operation that throws changes nothing…" Property: A refused flow operation changes nothing. three SECTIONs sweep every rejection, show one refused call.
- held: "a save holds ledgers, and loading the save of a world with p…" Property: S2. Save/load/resolve equals, owned by another test.
- held: "changing any packet, stock, created count, or consumed count…" Property: S2. Hash covers registered state, owned by another test; five SECTIONs too.
- held: "a copy of a world with flows stepped forward equals the orig…" Property: S2. Copy-equals and lockstep runs, owned by another test and another test.

sim/medium/kept_field_test.cpp

- rewrite: "a kept entry reads as its owner's rule of its held value and…" Property: A kept entry reads as its owner's rule of elapsed ticks, clamped at 0. Reaches the later-tick case by editing the save text ("\ntick 3\n"), so it breaks when the save layout changes (S1).
- held: "a saved world holding kept entries, loaded and resolved, rea…" Property: S2 (save laws, sim root: save_test.cpp), S3. Re-proves that a save loads back equal and saves again identically for kept state; that reading leaves the save unchanged is const-guaranteed.

sim/medium/network_test.cpp

- rewrite: "the constructor refuses each malformed input the spec lists…" Property: Malformed constructor input is refused with invalid_argument. Table of another test conditions; the spec lists them, the test shows one refused input.
- rewrite: "resolve gives no position for a missing carrier or a distanc…" Property: A place off its carrier resolves to nothing. Ten enumerated places; keep the boundaries just past each end and one absent carrier.
- rewrite: "nodePlace and nodeAnchor refuse a node not below the node co…" Property: Node queries throw out_of_range past the node count. four checks over two entry points, show one.
- held: "a world holding networks copies equal and hashes equal…" Property: S2. Copy equals and hashes equal, owned by another test.
- held: "changing a carrier point, stop, or anchor of a held network…" Property: S2. Hash covers registered state, owned by another test; enumerates seven fields (S4).
- held: "a save holds no network…" Property: S2. A save holds no derived data, owned by another test.
- held: "a place in a registered state component saves and loads back…" Property: S2. Save round trip over a registered type, owned by another test.

sim/medium/stepped_field_test.cpp

- rewrite: "sources are sampled in ascending key order across both layer…" Property: Sample order: ascending source across layers, then source order. give resolved sources several entries so field_test can go.
- held: "a save holds stepped entries, and loading the save of a reso…" Property: S2. Save/load/resolve equals, owned by another test.
- held: "changing any stepped entry changes the world's hash…" Property: S2. Hash covers registered state, owned by another test.
- held: "a copy stepped forward equals the original stepped forward…" Property: S2. Copy equals and lockstep runs, owned by walk_test and cycle_test.
- held: "systems that publish into fields give the same world and the…" Property: S4. the grouped property, fold into field_test ; its hash half is S2.
- held: "a candidate made with makeCandidate samples every field, bot…" Property: S2. Candidate equals commit, owned by another test.

sim/operations/food_offer_test.cpp

- rewrite: "After a resolution, food-offer's resolved entries hold one s…" Property: Each shop box offers one entry at its guest anchor, or none. Pins MEAL_RELIEF == 0.5 and restates the wait as ORDER_DELAY + D; keep the structure and state the wait as a prediction.

sim/operations/operations_edits_test.cpp

- held: "Every world a randomized park edit sequence with synthetic g…" Property: S2. Re-proves save-load-resolve and lockstep laws; owner sim root (save_test).
- held: "A candidate made with an edit from a world of a randomized p…" Property: S2. Re-proves the candidate law; owner sim root (cycle_test).

sim/operations/operations_test.cpp

- rewrite: "shipmentDelay gives 1 for a result below 1 or a NaN…" Property: shipmentDelay is a valid delay for every distance. assert 1 <= delay <= UINT32_MAX and non-decreasing, not a list of inputs.
- rewrite: "shipmentDelay gives the largest uint32_t for a result above…" Property: shipmentDelay is a valid delay for every distance. Second half of the same bound.
- rewrite: "inventoryPosition counts the supplies the shop holds, suppli…" Property: Position counts exactly the four kinds addressed to the shop. Thirteen sections, one per unit kind; keep the one combined case.
- rewrite: "A supplied shop at or below the reorder point sends one pack…" Property: A shop orders only at or below REORDER_POINT, up to ORDER_UP_TO. Pins REORDER_POINT, ORDER_UP_TO and ORDER_DELAY; with another test, one threshold property.
- rewrite: "A shop above the reorder point sends no orders…" Property: A shop orders only at or below REORDER_POINT, up to ORDER_UP_TO. The other side of the same threshold.

sim/operations/service_test.cpp

- rewrite: "Serving a guest consumes one supply as served and sends the…" Property: Serving trades one supply for one meal sent with the visit. Real contract; drop the SERVICE_INTERVAL == 90 pin.
- rewrite: "A shop serves the guest whose visit reached it at an earlier…" Property: Queue order is arrival swap, then guest key. Same-shape pair with another test; one test with mixed arrivals.
- rewrite: "A shop serves visits that reached it at the same swap in asc…" Property: Queue order is arrival swap, then guest key. Second member of the queue-order pair.
- rewrite: "A starved shop keeps queued only its first c visits, c being…" Property: A starved shop keeps as many visits as supplies held or coming. Seven sections enumerate cover components, pins RETURN_DELAY == 1, and times the return; keep the combined case.

sim/park/edit_sequences_test.cpp

- rewrite: "Random command sequences from the new park leave a physicall…" Property: one randomized test, "on a valid world, a command is accepted exactly when the world it describes is valid, so every cycle leaves the park valid". It drops the copy and save assertions. A command whose kind is not an enum value describes nothing and must count as refused. Re-proves the copy, round-trip, and re-save laws, owned by walk_test and save_test (S2). Keep only physical validity.

sim/park/edits_test.cpp

- rewrite: "An accepted AddPath gives the next key an entity holding a p…" Property: one test, "an accepted command leaves exactly the intent it describes, with new keys from the counter". One test per command for one property. Rewritten as one test with "An accepted AddBox gives the next key an entity holding a bo…", "An accepted MoveBox replaces the box's pose exactly with the…", "An accepted DeletePath or DeleteBox destroys the entity it n…".
- rewrite: "An accepted AddBox gives the next key an entity holding a bo…" Property: one test, "an accepted command leaves exactly the intent it describes, with new keys from the counter". Same. Rewritten as one test with "An accepted AddPath gives the next key an entity holding a p…", "An accepted MoveBox replaces the box's pose exactly with the…", "An accepted DeletePath or DeleteBox destroys the entity it n…".
- rewrite: "An accepted MoveBox replaces the box's pose exactly with the…" Property: one test, "an accepted command leaves exactly the intent it describes, with new keys from the counter". Same. Rewritten as one test with "An accepted AddPath gives the next key an entity holding a p…", "An accepted AddBox gives the next key an entity holding a bo…", "An accepted DeletePath or DeleteBox destroys the entity it n…".
- rewrite: "An accepted DeletePath or DeleteBox destroys the entity it n…" Property: one test, "an accepted command leaves exactly the intent it describes, with new keys from the counter". Same. Rewritten as one test with "An accepted AddPath gives the next key an entity holding a p…", "An accepted AddBox gives the next key an entity holding a bo…", "An accepted MoveBox replaces the box's pose exactly with the…".
- held: "Commands given to makeCandidate are each applied exactly whe…" Property: S2. Follows from another test and the candidate law.
- rewrite: "On a physically valid world, a command naming what it acts o…" Property: one randomized test, "on a valid world, a command is accepted exactly when the world it describes is valid, so every cycle leaves the park valid". It drops the copy and save assertions. A command whose kind is not an enum value describes nothing and must count as refused. Hand-picked expected validity depends on boxSize and pathWidth literals (S1). Assert the equivalence over random commands.

sim/park/geometry_test.cpp

- rewrite: "groundLine is empty exactly when a point is not finite or ou…" Property: groundLine is empty exactly for degenerate input, and never throws. Boundaries are hand-placed at the 1 cm literal (S1), with one input per check (S4).
- rewrite: "keptPoints gives the points groundLine keeps, in order, each…" Property: groundLine depends only on keptPoints, which keep MIN_POINT_SPACING apart. The expected list is hand-computed from 1 cm (S1).
- rewrite: "Each ground line distance is the previous plus the step's le…" Property: Distances start at 0 and strictly increase. The bit-exact sum restates the formula (S1).
- rewrite: "footprintOf's corners are front left, front right, back righ…" Property: Corners are in FL, FR, BR, BL order at plus or minus half size. Literal corners are tied to the ENTRANCE_SIZE and boxSize values (S1). Sections per pose.
- held: "Ground lines and footprints computed twice from the same inp…" Property: S2. Re-proves determinism for stateless pure functions.

sim/park/intent_test.cpp

- held: "A save of finite intent of every kind loads, saves to identi…" Property: S2. Re-proves the round-trip and re-save laws for park types, using exact text.
- held: "A world of degenerate finite intent copies and hashes as a l…" Property: S2. Re-proves the copy law for park types.

sim/park/sketch_park_test.cpp

- held: "tests/parks/sketch.park loads with makeParkSchema and saves…" Property: S2/S7. Re-runs the re-save law over a park file.

sim/park/validity_test.cpp

- rewrite: "Two footprints overlap when one reaches 2 mm into the other,…" Property: one test, "objects that only touch never conflict, and objects that interpenetrate beyond CONTACT_TOLERANCE always do". It covers solid against solid, line against solid, solid against edge, and line against edge, with offsets computed from boxSize, pathWidth, and ENTRANCE_SIZE instead of literals. Literal offsets 8 and 9 are the shop and entrance sizes (S1). Sections per facing. Rewritten as one test with "A ground line meets a footprint when a point of its segments…", "A footprint leaves the park exactly when a corner passes the…", "A ground line leaves the park exactly when a point of it pas…".
- rewrite: "A ground line meets a footprint when a point of its segments…" Property: one test, "objects that only touch never conflict, and objects that interpenetrate beyond CONTACT_TOLERANCE always do". It covers solid against solid, line against solid, solid against edge, and line against edge, with offsets computed from boxSize, pathWidth, and ENTRANCE_SIZE instead of literals. Literal 5.0 is shop half width plus backstage half width (S1). Keep the segment-not-points case. Rewritten as one test with "Two footprints overlap when one reaches 2 mm into the other,…", "A footprint leaves the park exactly when a corner passes the…", "A ground line leaves the park exactly when a point of it pas…".
- rewrite: "A footprint leaves the park exactly when a corner passes the…" Property: one test, "objects that only touch never conflict, and objects that interpenetrate beyond CONTACT_TOLERANCE always do". It covers solid against solid, line against solid, solid against edge, and line against edge, with offsets computed from boxSize, pathWidth, and ENTRANCE_SIZE instead of literals. Literal 4.0 is the shop half width (S1). Same touch-versus-tolerance shape. Rewritten as one test with "Two footprints overlap when one reaches 2 mm into the other,…", "A ground line meets a footprint when a point of its segments…", "A ground line leaves the park exactly when a point of it pas…".
- rewrite: "A ground line leaves the park exactly when a point of it pas…" Property: one test, "objects that only touch never conflict, and objects that interpenetrate beyond CONTACT_TOLERANCE always do". It covers solid against solid, line against solid, solid against edge, and line against edge, with offsets computed from boxSize, pathWidth, and ENTRANCE_SIZE instead of literals. Same shape. Keep the "judges the line, not the points" case. Rewritten as one test with "Two footprints overlap when one reaches 2 mm into the other,…", "A ground line meets a footprint when a point of its segments…", "A footprint leaves the park exactly when a corner passes the…".
- rewrite: "A world is not physically valid when an entrance or box has…" Property: Intent with no footprint or ground line makes the world invalid. The path cases re-enumerate groundLine's emptiness rule, which geometry 84 owns (S4).

sim/routes/connections_test.cpp

- rewrite: "An entity none of whose doors has a connector leaves the net…" Property: An unconnected entity leaves the networks as they are without it. Real property, but its "just beyond reach" case hard-codes 7.01 m; state that case relative to CONNECTION_REACH.
- rewrite: "Every stop of a path carrier other than its first and last l…" Property: Interior stops come only from meetings, connections included. Same property and shape as networks_test:235; merge into one test over parks with paths and connectors.

sim/routes/network_edits_test.cpp

- held: "Every world a random edit sequence reaches has the networks,…" Property: S2. Re-proves "a save loads back and resolves equal" for networks; owner sim root (save_test).
- held: "A candidate made with an edit has the networks, anchors incl…" Property: S2. Re-proves "a candidate equals the committed world" for networks; owner sim root (cycle_test).

sim/routes/networks_test.cpp

- rewrite: "Every stop but a carrier's first and last lies within the to…" Property: Interior stops come only from meetings, connections included. Family with connections_test:428; collapse the two into one test.

sim/routes/route_distance_test.cpp

- rewrite: "Among steps achieving a node's distance, Next is on the lowe…" Property: Next is the least achieving step by key, direction, then From. Family of three same-shape tests, one per tie level; one network with ties at every level.
- rewrite: "Among steps achieving a node's distance on one carrier, Next…" Property: Next is the least achieving step by key, direction, then From. Second member of the tie-break family. Rewritten as one test with "Among steps achieving a node's distance on one carrier in on…".
- rewrite: "Among steps achieving a node's distance on one carrier in on…" Property: Next is the least achieving step by key, direction, then From. Third member of the tie-break family. Rewritten as one test with "Among steps achieving a node's distance on one carrier, Next…".
- rewrite: "sampleRouteEdge gives one entry, the least of each end's ent…" Property: Edge sample is the least end entry plus offset, stepping toward it. Four sections enumerate inputs of one rule; assert it once over drawn end entries.
- held: "Every world a random edit sequence reaches equals its save l…" Property: S2. Re-proves the save-load-resolve law (even calls worldsEqual); owner sim root (save_test).
- held: "A candidate made with an edit has the route distance of the…" Property: S2. Re-proves the candidate law for route distance; owner sim root (cycle_test).

sim/routes/routes_park_test.cpp

- held: "tests/parks/routes.park loads with makeParkSchema and saves…" Property: S2. Re-proves "saving again gives identical text" on a park file; owner sim root (save_test).

sim/save_test.cpp

- held: "a loaded and resolved world steps in lockstep with the world…" Property: S2/S4. Follows from round trip and determinism. Its order-free synthetic system cannot expose hidden state.
- rewrite: "a loaded world holds the saved seed, tick, and next key, wit…" Property: A loaded world is pending resolution. The header values repeat the round trip. The edge-values section enumerates boundaries (S4).
- rewrite: "a save holds no derived data…" Property: A save is independent of derived data. Sections enumerate mutation kinds (change, add, remove).
- rewrite: "text that is not a save fails the load naming the first line…" Property: A failed load names the first unreadable line. About 40 hand-listed error conditions (S4).
- rewrite: "blank lines and carriage returns do not change what a save l…" Property: A load ignores CRs and blank lines. Three sections are variants of one input. Combine them into one variant.
- rewrite: "a schema refuses a component type named entities and is left…" Property: one test, "a schema refuses a repeated or reserved registration and is left unchanged". A reserved name is one case of the schema's registration refusals.

sim/schema_test.cpp

- rewrite: "a schema accepts names of lowercase letters, digits, and hyp…" Property: one test, "a schema accepts a name exactly when it is lowercase letters, digits, and hyphens". The accepting half of the name rule. Rewritten as one test with "a schema refuses a malformed name and is left unchanged…", "a schema refuses a resolver with a malformed name and is lef…".
- rewrite: "a schema refuses a malformed name and is left unchanged…" Property: one test, "a schema accepts a name exactly when it is lowercase letters, digits, and hyphens". The refusing half, which loops malformed names. Rewritten as one test with "a schema accepts names of lowercase letters, digits, and hyp…", "a schema refuses a resolver with a malformed name and is lef…".
- rewrite: "a schema refuses a repeated name and is left unchanged…" Property: one test, "a schema refuses a repeated or reserved registration and is left unchanged". One of five same-shape duplicate-refusal tests. Rewritten as one test with "a schema refuses a type registered twice and is left unchang…", "a schema refuses a resolver with a repeated name and is left…", "a schema refuses a command type registered twice and is left…".
- rewrite: "a schema refuses a type registered twice and is left unchang…" Property: one test, "a schema refuses a repeated or reserved registration and is left unchanged". Same shape. Rewritten as one test with "a schema refuses a repeated name and is left unchanged…", "a schema refuses a resolver with a repeated name and is left…", "a schema refuses a command type registered twice and is left…".
- rewrite: "a schema refuses a resolver with a malformed name and is lef…" Property: one test, "a schema accepts a name exactly when it is lowercase letters, digits, and hyphens". The same name rule for another entry point. Rewritten as one test with "a schema accepts names of lowercase letters, digits, and hyp…", "a schema refuses a malformed name and is left unchanged…".
- rewrite: "a schema refuses a resolver with a repeated name and is left…" Property: one test, "a schema refuses a repeated or reserved registration and is left unchanged". Same shape. Rewritten as one test with "a schema refuses a repeated name and is left unchanged…", "a schema refuses a type registered twice and is left unchang…", "a schema refuses a command type registered twice and is left…".
- rewrite: "a schema refuses a command type registered twice and is left…" Property: one test, "a schema refuses a repeated or reserved registration and is left unchanged". Same shape. Rewritten as one test with "a schema refuses a repeated name and is left unchanged…", "a schema refuses a type registered twice and is left unchang…", "a schema refuses a resolver with a repeated name and is left…".

sim/sim_math_test.cpp

- rewrite: "simExp returns exactly 1 for zero of either sign…" Property: one test, or extra rows in the reference tables, "simExp and simLog return IEEE's special values" (sim_math 71 to another test). One special input per test (S4). Rewritten as one test with "simExp returns +infinity for +infinity and for any argument…", "simExp returns +0 for -infinity and for any argument of -746…", "simExp returns a NaN for a NaN…", "simLog returns -infinity for zero of either sign…", "simLog returns +infinity for +infinity…", "simLog returns +0 for 1…", "simLog returns a NaN for any argument below zero…", "simLog returns a NaN for a NaN…".
- rewrite: "simExp returns +infinity for +infinity and for any argument…" Property: one test, or extra rows in the reference tables, "simExp and simLog return IEEE's special values" (sim_math 71 to another test). Same. Rewritten as one test with "simExp returns exactly 1 for zero of either sign…", "simExp returns +0 for -infinity and for any argument of -746…", "simExp returns a NaN for a NaN…", "simLog returns -infinity for zero of either sign…", "simLog returns +infinity for +infinity…", "simLog returns +0 for 1…", "simLog returns a NaN for any argument below zero…", "simLog returns a NaN for a NaN…".
- rewrite: "simExp returns +0 for -infinity and for any argument of -746…" Property: one test, or extra rows in the reference tables, "simExp and simLog return IEEE's special values" (sim_math 71 to another test). Same. Rewritten as one test with "simExp returns exactly 1 for zero of either sign…", "simExp returns +infinity for +infinity and for any argument…", "simExp returns a NaN for a NaN…", "simLog returns -infinity for zero of either sign…", "simLog returns +infinity for +infinity…", "simLog returns +0 for 1…", "simLog returns a NaN for any argument below zero…", "simLog returns a NaN for a NaN…".
- rewrite: "simExp returns a NaN for a NaN…" Property: one test, or extra rows in the reference tables, "simExp and simLog return IEEE's special values" (sim_math 71 to another test). Same. Rewritten as one test with "simExp returns exactly 1 for zero of either sign…", "simExp returns +infinity for +infinity and for any argument…", "simExp returns +0 for -infinity and for any argument of -746…", "simLog returns -infinity for zero of either sign…", "simLog returns +infinity for +infinity…", "simLog returns +0 for 1…", "simLog returns a NaN for any argument below zero…", "simLog returns a NaN for a NaN…".
- rewrite: "simLog returns -infinity for zero of either sign…" Property: one test, or extra rows in the reference tables, "simExp and simLog return IEEE's special values" (sim_math 71 to another test). Same. Rewritten as one test with "simExp returns exactly 1 for zero of either sign…", "simExp returns +infinity for +infinity and for any argument…", "simExp returns +0 for -infinity and for any argument of -746…", "simExp returns a NaN for a NaN…", "simLog returns +infinity for +infinity…", "simLog returns +0 for 1…", "simLog returns a NaN for any argument below zero…", "simLog returns a NaN for a NaN…".
- rewrite: "simLog returns +infinity for +infinity…" Property: one test, or extra rows in the reference tables, "simExp and simLog return IEEE's special values" (sim_math 71 to another test). Same. Rewritten as one test with "simExp returns exactly 1 for zero of either sign…", "simExp returns +infinity for +infinity and for any argument…", "simExp returns +0 for -infinity and for any argument of -746…", "simExp returns a NaN for a NaN…", "simLog returns -infinity for zero of either sign…", "simLog returns +0 for 1…", "simLog returns a NaN for any argument below zero…", "simLog returns a NaN for a NaN…".
- rewrite: "simLog returns +0 for 1…" Property: one test, or extra rows in the reference tables, "simExp and simLog return IEEE's special values" (sim_math 71 to another test). Same. Rewritten as one test with "simExp returns exactly 1 for zero of either sign…", "simExp returns +infinity for +infinity and for any argument…", "simExp returns +0 for -infinity and for any argument of -746…", "simExp returns a NaN for a NaN…", "simLog returns -infinity for zero of either sign…", "simLog returns +infinity for +infinity…", "simLog returns a NaN for any argument below zero…", "simLog returns a NaN for a NaN…".
- rewrite: "simLog returns a NaN for any argument below zero…" Property: one test, or extra rows in the reference tables, "simExp and simLog return IEEE's special values" (sim_math 71 to another test). Same. Rewritten as one test with "simExp returns exactly 1 for zero of either sign…", "simExp returns +infinity for +infinity and for any argument…", "simExp returns +0 for -infinity and for any argument of -746…", "simExp returns a NaN for a NaN…", "simLog returns -infinity for zero of either sign…", "simLog returns +infinity for +infinity…", "simLog returns +0 for 1…", "simLog returns a NaN for a NaN…".
- rewrite: "simLog returns a NaN for a NaN…" Property: one test, or extra rows in the reference tables, "simExp and simLog return IEEE's special values" (sim_math 71 to another test). Same. Rewritten as one test with "simExp returns exactly 1 for zero of either sign…", "simExp returns +infinity for +infinity and for any argument…", "simExp returns +0 for -infinity and for any argument of -746…", "simExp returns a NaN for a NaN…", "simLog returns -infinity for zero of either sign…", "simLog returns +infinity for +infinity…", "simLog returns +0 for 1…", "simLog returns a NaN for any argument below zero…".

sim/walk_test.cpp

- rewrite: "worlds built by the same calls are equal and hash equal, eve…" Property: one test, "world equality compares schemas by content". The equal half. Same-schema equality also repeats another test. Rewritten as one test with "worlds on different schemas are never equal…".
- rewrite: "worlds on different schemas are never equal…" Property: one test, "world equality compares schemas by content". The unequal half. Rewritten as one test with "worlds built by the same calls are equal and hash equal, eve…".

sim/world_checks_test.cpp

- rewrite: "validateWorld refuses a component of an unregistered type, n…" Property: one test, through copyWorld or hashWorld, "the walk refuses a world holding state it cannot cover, and accepts every world it can" (world_checks, all 7). One error condition per test (S4). Rewritten as one test with "validateWorld accepts an empty storage of an unregistered ty…", "validateWorld refuses an entity not created by the world…", "validateWorld refuses a live key whose entity was destroyed…", "validateWorld refuses a NaN in a registered double, naming t…", "validateWorld accepts every world the walk can cover…", "in debug builds, copyWorld, worldsEqual, and hashWorld run v…".
- rewrite: "validateWorld accepts an empty storage of an unregistered ty…" Property: one test, through copyWorld or hashWorld, "the walk refuses a world holding state it cannot cover, and accepts every world it can" (world_checks, all 7). The accept side of the same property. Rewritten as one test with "validateWorld refuses a component of an unregistered type, n…", "validateWorld refuses an entity not created by the world…", "validateWorld refuses a live key whose entity was destroyed…", "validateWorld refuses a NaN in a registered double, naming t…", "validateWorld accepts every world the walk can cover…", "in debug builds, copyWorld, worldsEqual, and hashWorld run v…".
- rewrite: "validateWorld refuses an entity not created by the world…" Property: one test, through copyWorld or hashWorld, "the walk refuses a world holding state it cannot cover, and accepts every world it can" (world_checks, all 7). Another error condition. Rewritten as one test with "validateWorld refuses a component of an unregistered type, n…", "validateWorld accepts an empty storage of an unregistered ty…", "validateWorld refuses a live key whose entity was destroyed…", "validateWorld refuses a NaN in a registered double, naming t…", "validateWorld accepts every world the walk can cover…", "in debug builds, copyWorld, worldsEqual, and hashWorld run v…".
- rewrite: "validateWorld refuses a live key whose entity was destroyed…" Property: one test, through copyWorld or hashWorld, "the walk refuses a world holding state it cannot cover, and accepts every world it can" (world_checks, all 7). Another error condition. Rewritten as one test with "validateWorld refuses a component of an unregistered type, n…", "validateWorld accepts an empty storage of an unregistered ty…", "validateWorld refuses an entity not created by the world…", "validateWorld refuses a NaN in a registered double, naming t…", "validateWorld accepts every world the walk can cover…", "in debug builds, copyWorld, worldsEqual, and hashWorld run v…".
- rewrite: "validateWorld refuses a NaN in a registered double, naming t…" Property: one test, through copyWorld or hashWorld, "the walk refuses a world holding state it cannot cover, and accepts every world it can" (world_checks, all 7). Sections per field shape (top-level, vector, nested). Rewritten as one test with "validateWorld refuses a component of an unregistered type, n…", "validateWorld accepts an empty storage of an unregistered ty…", "validateWorld refuses an entity not created by the world…", "validateWorld refuses a live key whose entity was destroyed…", "validateWorld accepts every world the walk can cover…", "in debug builds, copyWorld, worldsEqual, and hashWorld run v…".
- rewrite: "validateWorld accepts every world the walk can cover…" Property: one test, through copyWorld or hashWorld, "the walk refuses a world holding state it cannot cover, and accepts every world it can" (world_checks, all 7). The accept side, with sections. Rewritten as one test with "validateWorld refuses a component of an unregistered type, n…", "validateWorld accepts an empty storage of an unregistered ty…", "validateWorld refuses an entity not created by the world…", "validateWorld refuses a live key whose entity was destroyed…", "validateWorld refuses a NaN in a registered double, naming t…", "in debug builds, copyWorld, worldsEqual, and hashWorld run v…".
- rewrite: "in debug builds, copyWorld, worldsEqual, and hashWorld run v…" Property: one test, through copyWorld or hashWorld, "the walk refuses a world holding state it cannot cover, and accepts every world it can" (world_checks, all 7). The contract belongs on the public walk, but sections repeat it per entry point. The rewritten test asserts through one walk call. Rewritten as one test with "validateWorld refuses a component of an unregistered type, n…", "validateWorld accepts an empty storage of an unregistered ty…", "validateWorld refuses an entity not created by the world…", "validateWorld refuses a live key whose entity was destroyed…", "validateWorld refuses a NaN in a registered double, naming t…", "validateWorld accepts every world the walk can cover…".

tools/path_tool_test.cpp

- rewrite: "A path tool not holding shows an AddPath through its drawn p…" Property: The ghost shows the next point unless that point would finish. Pins FINISH_REACH == 1.0 and its exact boundary (S1); section 2 duplicates another test
- rewrite: "A path tool highlights nothing…" Property: Only an idle MoveBox highlights. One test per tool of "highlights nothing" (S4). Rewritten as one test with tools_test's "The move tool highlights nothing while it holds…".

tools/picking_test.cpp

- rewrite: "boxAt gives the least-keyed box whose footprint holds the po…" Property: boxAt gives a box holding the point, or none; never the entrance. Pins the arbitrary least-key tie-break (S1) Rewritten as one test with "boxAt gives none where no box's footprint holds the point, a…".
- rewrite: "boxAt gives none where no box's footprint holds the point, a…" Property: boxAt gives a box holding the point, or none; never the entrance. The negative half of the same property Rewritten as one test with "boxAt gives the least-keyed box whose footprint holds the po…".
- rewrite: "pathAt gives the least-keyed path whose ground line has a se…" Property: pathAt gives a path within half its width, or none; never the entrance. Least-key pin (S1); keep the segment-not-point case Rewritten as one test with "pathAt gives none where no path's ground line comes within h…".
- rewrite: "pathAt gives none where no path's ground line comes within h…" Property: pathAt gives a path within half its width, or none; never the entrance. The negative half of the same property Rewritten as one test with "pathAt gives the least-keyed path whose ground line has a se…".
- rewrite: "snapToPath gives the point itself when no ground line of a p…" Property: A point out of reach of every path of the kind is unchanged. Pins SNAP_REACH == 2.0 and 2.1 m boundary points (S1)

tools/tools_test.cpp

- rewrite: "A place tool's press lands the box at the pointer, and while…" Property: Facing follows a far drag, and a near jitter keeps it. Pins MIN_FACING_DRAG == 1.0, its exact boundary, and non-normalization (S1)
- rewrite: "The move tool highlights nothing while it holds…" Property: Only an idle MoveBox highlights. One test per tool of "highlights nothing" (S4). Rewritten as one test with path_tool_test's "A path tool highlights nothing…".
