# Implementation Plan: Unsaved Choices

## Goal

Remove the guest's stored last choice from its state, save, record, and panel, add guestOptions, which works out what a guest would weigh now, and show the panel's table from it, with guests' behavior unchanged bit for bit.

## Approach

The choice's option building moves into one internal function, which a guest's choice and guestOptions both call, and softmaxPick's probability arithmetic moves into another, which guestOptions calls without drawing. So the panel's options are scored by the very code a guest chooses with. warm.park and cut.park are remade, and their only difference from the committed files must be the removed field.

## Placement

Decision 0027 places each behavior this feature adds:

- What a guest would weigh now: sim, the guests module, guestOptions in src/sim/guests/guests.h and guests.cpp. The scoring and the guest's state are private to the module (principle 6), and computing the options there with the choice's own code keeps the panel's explanation and the guest's decisions from drifting apart.
- The panel's table of options: legible, inspectSubject in src/legible/inspect.h, unchanged in role. It now takes its Choices from guestOptions rather than the guest record.
- The Inspector window's table: app, src/app/ui/inspector_window.cpp, unchanged in role, drawing the rows it is given.
- The parks without the field: tests/parks/warm.park and cut.park, made by tpj_scenarios --slice-parks as before, and tests/parks/stress/winding-path.park, edited once since nothing generates it.

## Tasks

### Task 1: Update the guests spec

Files:
- Modify: `src/sim/guests/SPEC.md`

Step 1: Make the five changes listed under "src/sim/guests/SPEC.md:" in plans/scalable-runtime/measured-runtime/unsaved-choices/FEATURE.md: the Registration, Adding a guest, Choice, and Carrying edits, and the Inspection record edit with its new paragraph, written without the leading "> ".

### Task 2: Update the legible spec

Files:
- Modify: `src/legible/SPEC.md`, the Inspectors section

Step 1: Remove the Last choice bullet, and replace the paragraph beginning "Choices has a ChoiceRow for each option of LastChoice" with the text under "src/legible/SPEC.md, Inspectors:" in FEATURE.md, without the leading "> ".

### Task 3: Update the app spec

Files:
- Modify: `src/app/SPEC.md`, the Inspector paragraph

Step 1: Replace the clause quoted under "src/app/SPEC.md" in FEATURE.md with its replacement.

### Task 4: Remove the stored choice and declare guestOptions

Files:
- Modify: `src/sim/guests/guests.h`
- Modify: `src/sim/guests/internal/guest.h`
- Modify: `src/sim/guests/guests.cpp`
- Modify: `src/legible/inspect.h`, `src/legible/inspect.cpp`
- Modify: `src/app/ui/inspector_window.cpp`

Step 1: In guests.h, delete `template <typename Visitor> void visitFields(Visitor &visitor, ChoiceOption &option)` with its body. Delete the GuestChoice struct, its comment, and its visitFields. In GuestRecord, delete `std::optional<GuestChoice> LastChoice;`, and change its comment's "the meals it has eaten and the last of them, and its last choice." to "and the meals it has eaten and the last of them." After guestRecord's declaration, add the guestOptions declaration under "src/sim/guests/guests.h" in FEATURE.md, with its comment.

Step 2: In internal/guest.h, delete `GuestChoice LastChoice;` and `visitor.field("last-choice", guest.LastChoice);`, and change the comment's "the meals it has eaten and the last of them, and its last choice, whose options are empty until it first chooses." to "and the meals it has eaten and the last of them."

Step 3: In guests.cpp's choose, replace the four lines from `GuestChoice choice;` to `choice.Hunger = guest.Hunger;` with `std::vector<ChoiceOption> options;`, replace each `choice.Options` with `options`, delete `choice.Picked = picked;` and `guest.LastChoice = std::move(choice);`, and change its comment to "Scores the guest's options where it stands, picks one by softmax, and takes it up." In guestRecord, delete the three lines `if (!state->LastChoice.Options.empty()) {` to its closing brace. After guestRecord, add the stub:

```cpp
std::optional<std::vector<ChoiceOption>> guestOptions(const World & /*world*/,
                                                      EntityKey /*guest*/) {
  return std::nullopt;
}
```

Step 4: In inspect.h, delete `bool Picked = false;` from ChoiceRow, and change its comment to "One option a guest would weigh now, as text: the option, its terms, score, and probability. Carrying on and heading home have empty terms." Change Inspection's comment to "What an inspector shows: its title, whether its entity is gone, its lines, and the options a guest would weigh now."

Step 5: In inspect.cpp's guestRows, delete the block from `if (const std::optional<GuestChoice> &choice = record.LastChoice) {` through its `else` branch's closing brace. Replace choiceRows with:

```cpp
// Each option the guest would weigh now, its terms only for an offer.
std::vector<ChoiceRow> choiceRows(const std::vector<ChoiceOption> &options) {
  std::vector<ChoiceRow> rows;
  for (const ChoiceOption &option : options) {
    ChoiceRow row;
    if (option.Kind == ChoiceKind::Offer) {
      row.Option = "shop " + keyText(option.Shop);
      row.Relief = twoDecimals(option.Relief);
      row.Distance = twoDecimals(option.Distance);
      row.Wait = twoDecimals(option.Wait);
      row.Commitment = twoDecimals(option.Commitment);
    } else {
      row.Option = displayName(option.Kind);
    }
    row.Score = twoDecimals(option.Score);
    row.Probability = twoDecimals(option.Probability);
    rows.push_back(std::move(row));
  }
  return rows;
}
```

In inspectSubject, replace

```cpp
      if (record->LastChoice) {
        inspection.Choices = choiceRows(*record->LastChoice);
      }
```

with

```cpp
      if (const std::optional<std::vector<ChoiceOption>> options =
              guestOptions(world, subject.Key)) {
        inspection.Choices = choiceRows(*options);
      }
```

Step 6: In inspector_window.cpp, replace drawChoices with:

```cpp
// The options the guest would weigh now, one to a row.
void drawChoices(const std::vector<ChoiceRow> &choices) {
  ImGui::TextUnformatted("Options now");
  if (!ImGui::BeginTable("choices", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  for (const char *heading :
       {"Option", "Relief", "Distance", "Wait", "Commitment", "Score", "Chance"}) {
    ImGui::TableSetupColumn(heading);
  }
  ImGui::TableHeadersRow();
  for (const ChoiceRow &choice : choices) {
    ImGui::TableNextRow();
    for (const std::string *cell : {&choice.Option, &choice.Relief, &choice.Distance, &choice.Wait,
                                    &choice.Commitment, &choice.Score, &choice.Probability}) {
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(cell->c_str());
    }
  }
  ImGui::EndTable();
}
```

Step 7: Build the libraries and the app. The existing tests that read LastChoice no longer compile until the test pass rewrites them.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim tpj_legible tpj_app tpj_scenarios tpj_bench`
Expected: the build succeeds with no warnings.

### Task 5: Run the test pass

Dispatch the test-writer as implementing-features describes, with FEATURE.md; src/sim/guests/SPEC.md, src/legible/SPEC.md, src/app/SPEC.md, and src/sim/SPEC.md as the specs; and src/sim/guests/guests.h and src/legible/inspect.h as the public headers. Afterwards, `cmake.exe --build --preset windows-debug` must build every test executable.

### Task 6: Share the choice's scoring and probabilities

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: In the anonymous namespace, before choose, add:

```cpp
// The options a guest weighs where it stands, scored as a choice scores them, and each offer's
// relief by option index, for the meal it would give.
struct ScoredOptions {
  std::vector<ChoiceOption> Options;
  std::vector<double> Reliefs;
};

ScoredOptions scoreOptions(const World &world, const Guest &guest, const Network &network,
                           const RouteSample &routes, const std::vector<EntityKey> &entrances) {
  ScoredOptions scored;
  const double curve = hungerCurve(guest.Hunger);
  for (const SampledEntry<RouteEntry> &entry : routes) {
    const std::optional<OfferEntry> offer = suppliedOffer(world, network, entry.Source);
    if (!offer) {
      continue;
    }
    ChoiceOption option;
    option.Kind = ChoiceKind::Offer;
    option.Shop = entry.Source;
    option.Relief = RELIEF_WEIGHT * curve * offer->Relief;
    option.Distance = DISTANCE_WEIGHT * entry.Value.Distance;
    option.Wait = WAIT_WEIGHT * (static_cast<double>(offer->Wait) * SIM_TICK_SECONDS);
    option.Commitment = entry.Source == guest.Target ? COMMITMENT_BONUS : 0.0;
    option.Score = option.Relief + option.Distance + option.Wait + option.Commitment;
    scored.Options.push_back(option);
    scored.Reliefs.push_back(offer->Relief);
  }
  ChoiceOption carryOn;
  carryOn.Kind = ChoiceKind::CarryOn;
  carryOn.Score = CARRY_ON_SCORE;
  scored.Options.push_back(carryOn);
  if (world.Tick >= guest.StayUntil && homeEntry(routes, entrances) != nullptr) {
    ChoiceOption headHome;
    headHome.Kind = ChoiceKind::HeadHome;
    headHome.Score = HEAD_HOME_SCORE;
    scored.Options.push_back(headHome);
  }
  return scored;
}

// Sets each option's softmax probability and gives the weights they came from. Throws
// std::invalid_argument for no options or a score that is not finite.
std::vector<double> softmaxWeights(std::span<ChoiceOption> options) {
  if (options.empty()) {
    throw std::invalid_argument("a choice needs at least one option");
  }
  double greatest = -std::numeric_limits<double>::infinity();
  for (const ChoiceOption &option : options) {
    if (!std::isfinite(option.Score)) {
      throw std::invalid_argument("a choice's score is not finite");
    }
    greatest = std::max(greatest, option.Score);
  }
  // Scores are shifted so the greatest weighs exactly 1: no weight overflows, and the total is at
  // least 1.
  std::vector<double> weights;
  double total = 0.0;
  for (const ChoiceOption &option : options) {
    weights.push_back(simExp((option.Score - greatest) / CHOICE_TEMPERATURE));
    total += weights.back();
  }
  for (size_t i = 0; i < options.size(); ++i) {
    options[i].Probability = weights[i] / total;
  }
  return weights;
}
```

Step 2: Replace choose's body with:

```cpp
  ScoredOptions scored = scoreOptions(world, guest, network, routes, entrances);
  const size_t picked =
      softmaxPick(drawKey(world, key, hashName("guest-choice"), choices++), scored.Options);
  const ChoiceOption &option = scored.Options[picked];
  guest.Target = NULL_KEY;
  if (option.Kind == ChoiceKind::Offer) {
    guest.Activity = GuestActivity::HeadingToShop;
    guest.Target = option.Shop;
    guest.MealRelief = scored.Reliefs[picked];
  } else if (option.Kind == ChoiceKind::HeadHome) {
    guest.Activity = GuestActivity::HeadingHome;
  } else {
    guest.Activity = GuestActivity::Wandering;
  }
```

Step 3: Replace softmaxPick's body with:

```cpp
  const std::vector<double> weights = softmaxWeights(options);
  return drawPick(key, std::span<const double>(weights));
```

The expressions are the ones choose and softmaxPick used, in the same order, so every choice is unchanged by a bit.

Step 4: Build and run the guests tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#choice_test],[#walks_test],[#visits_test],[#arrivals_test],[#add_guest_test],[#guest_edits_test],[#footfall_test]"`
Expected: no warnings. Every test passes except the test pass's tests of guestOptions, which the stub still fails.

### Task 7: Implement guestOptions

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Replace the stub of Task 4 with:

```cpp
std::optional<std::vector<ChoiceOption>> guestOptions(const World &world, EntityKey guest) {
  const entt::entity entity = world.findEntity(guest);
  if (entity == entt::null) {
    return std::nullopt;
  }
  const Guest *state = world.Registry.try_get<Guest>(entity);
  if (state == nullptr) {
    return std::nullopt;
  }
  const Network &network = parkNetwork(world, PathKind::Guest);
  if (!network.resolve(state->At)) {
    return std::nullopt;
  }
  const RouteSample routes = sampleField<GuestRouteDistance>(world, network, state->At);
  ScoredOptions scored = scoreOptions(world, *state, network, routes, entranceKeys(world));
  softmaxWeights(scored.Options);
  return std::move(scored.Options);
}
```

Step 2: Build and run the sim and legible tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_legible_tests tpj_integration_tests && build/windows-debug/tpj_sim_tests.exe && build/windows-debug/tpj_legible_tests.exe && build/windows-debug/tpj_integration_tests.exe`
Expected: no warnings, and every test in the three executables passes, wherever the test pass put its tests of guestOptions and the inspection.

### Task 8: Remake the slice parks

Files:
- Modify: `tests/parks/warm.park`, `tests/parks/cut.park`

Step 1: Remake them.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park`
Expected: exit status 0.

Step 2: Compare each with its committed version stripped of the field (criterion 6).

Run: `for f in warm cut; do git show HEAD:tests/parks/$f.park | sed 's/ last-choice=.*$//' | cmp - tests/parks/$f.park && echo "$f identical"; done`
Expected: `warm identical` and `cut identical`, and no cmp output. Any difference means behavior changed: stop and follow the deviation procedure.

### Task 9: Strip the field from winding-path.park

Files:
- Modify: `tests/parks/stress/winding-path.park`

Step 1: Remove the field.

Run: `sed -i 's/ last-choice=.*$//' tests/parks/stress/winding-path.park && grep -c last-choice tests/parks/stress/winding-path.park`
Expected: `0`.

Step 2: Check that it loads.

Run: `cmake.exe --build --preset windows-release --target tpj_bench && build/windows-release/tpj_bench.exe tests/parks/stress/winding-path.park`
Expected: a `park tests/parks/stress/winding-path.park ticks 300 ...` line and six stage lines, with exit status 0. The food-overlay, park-mesh, and guest-mesh vertex counts are those of build/runtime-report/created-guests-after.txt: 9372, 3394, and 800.

### Task 10: Confirm the acceptance criteria

Step 1: Windows.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: no warnings; every test passes.

Step 2: Linux and tidy.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, tidy is clean.

Step 3: The cross-build check.

Run: `scripts/cross-build-check.sh`
Expected: it ends with `cross-build-check: both builds wrote the same <n> lines; passed.`

### Task 11: Commit

Stage everything with `git add -A && git reset -q parks/`, so the untracked parks/routes.park and parks/sketch.park stay out. Commit once via the commit-hygiene skill, subject `Sim: Work out a guest's options on inspect, not from saves`.
