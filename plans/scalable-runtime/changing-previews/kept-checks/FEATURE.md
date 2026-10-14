# Feature: Kept Checks

## Summary

kept-checks makes a changed frame plan its edit once, into lists the caller keeps. The acceptance check, applying the edit to the candidate, the shop context, and the ghost all read that one plan. None of them allocates once its lists have grown.

Today a changed frame of the path drag plans the edit five times. The preview's acceptance check plans it, applying the queued command to the candidate plans it again, and the ghost plans it for its own acceptance check, for editedSurface, and for pathCrossings. Each plan copies the whole layout, derives the whole park's ground two to four times, and makes new lists throughout. keptCurves alone makes two new lists for every path point in the park. A changed frame of the shop drag checks the box three times, each into new lists (RESEARCH.md).

After this feature:

- The preview plans the edit once into a PlannedEdit it keeps. It makes the candidate by applying that plan to the refilled copy, instead of queueing the command and planning it again.
- The ghost reads whether the edit is accepted and where it crosses from the plan. It derives the edited segments and surface from the plan once, into lists it keeps.
- Applying a command in a cycle passes the cycle's buffers, so a commit, an undo, or a redo plans in kept lists too.
- Every surface the checks build refills storage the caller keeps, which unit-surface needs before it stores the surface per owner.
- The opening-hours panel's arrival day keeps a preview, instead of making a new one through previewEdit at each change of the hours it shows.
- Resolution tells a trace when each step starts and ends. The allocation check uses it to watch both drags and a round of commits, pausing only inside the resolvers no feature has moved yet.

The planning still copies the layout and derives the whole park's ground and the surface around the changed points. That whole-park work is layered-candidate's: it reads only what the edit reaches.

## Acceptance criteria

1. Both drags get faster, measured over the whole changed frame, and nothing else gets slower. The runtime report's --compare against build/flat-ground.report marks shop-drag and path-drag faster on full.park with windows-release. path-drag's median falls by at least 1.5 ms and shop-drag's by at least 0.15 ms. In the derived-units profiles, planning the path edit four extra times was about 2.4 ms of the path frame, and two extra box checks about 0.2 ms of the shop frame. No other stage is marked slower, and none is marked result-changed. (check: scripts/runtime-report.sh build/kept-checks.report, then --compare with build/flat-ground.report)
2. A changed frame plans its edit once. Take four Very Sleepy profiles of windows-release tpj_bench --ticks 1 on full.park. Under each drag's timing function:
    - no sample holds applyCommand;
    - no sample holds both buildGhostMesh and planEdit;
    - every sample that holds planEdit also holds keepPreview.

    (check: the profiles PLAN.md's confirming task runs)
3. The allocation check watches the changed frames and the commits. After its edit rounds, it drags full.park's lowest-keyed shop along its face and draws a guest path out and back from a guest segment, one tick between frames, through the kept preview and the ghost, as tpj_bench's drags do. Then, on a copy of the world, it commits edits there and back, round after round:
    - the shop moved 0.5 m along its face, and back;
    - a guest path point moved 0.5 m, crossing nothing, and back;
    - a guest segment's width set 0.5 m wider by a RestoreIntent, as an undo sets it, and back.

    In all of these it watches everything, and pauses only inside the resolvers no feature has moved yet and around the finishers. The second stop's press while drawing a path is watched too. The check fails if watched work frees a block that watched work allocated. (ctest: the allocation check)
4. The result is exact:
    - The candidate check's drags on warm.park make each frame's kept candidate the preview's way, by applying the frame's plan. Each kept candidate equals the candidate makeCandidate makes from the queued command, and the world loaded from its save.
    - The save checks pass on every chosen edit, which a cycle now applies with its buffers.
    - tpj_scenarios --compare of windows-debug output before and after the feature shows no difference, for its scenarios, every park in tests/parks, and full.park over 300 ticks.
    - The runtime report's --compare marks no stage result-changed. The drag stages' result is the last kept candidate's hash, which the queued command made before this feature.

    (ctest, with the changed candidate check below; tpj_scenarios --compare; the report's --compare)
5. Only callers outside frames make new lists for a check. These are scenario builders, tpj_bench's own stages, and tests, through the returning forms of isAccepted, pathCrossings, editedSegments, editedSurface, applyCommand, shopContext, previewEdit, and buildGhostMesh. Every frame and cycle path calls the kept forms. The functions on these paths are planEdit, applyPlanned, editedParts, the buffered applyCommand forms, the shop context's kept form, the ghost with its plan, and the arrival day, which keeps a KeptPreview in place of calling previewEdit. Each of them, and everything they call, allocates nothing once its lists have grown. (check: read each of them, and grep their callers in src)
6. src/sim/SPEC.md, src/sim/park/SPEC.md, src/legible/SPEC.md, src/render/SPEC.md, and src/tools/SPEC.md say what changed, as do docs/conventions.md, docs/testing.md, and decision 0041. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Tests

No new test. One existing world check changes, so that it checks the candidate the way the preview now makes it.

- The candidate check's drags, "A candidate kept and refilled on each frame of a drag on warm.park, with a tick before each frame, equals the candidate made new for the frame's edit and the world loaded from its save", in tests/sim/park_world_checks_test.cpp. Setup: as now, warm.park, the shop drag's and the path drag's frames, a tick before each frame. Change: on each frame, plan the frame's edit with planEdit into one PlannedEdit kept across the frames, with TickBuffers kept across them. Then refill the kept candidate with the makeCandidate form that takes a change, where the change applies the plan with applyPlanned and the kept TickBuffers. Expected: as now, the kept candidate equals makeCandidate on the queued edit, made new, and equals the world loaded from the kept candidate's save and resolved.

The save checks cover applying commands in a cycle. The allocation check gains the drags and commits as criterion 3 says.

## Medium

None. A plan, its lists, and the trace are how the edit checks and resolution do their work. They never appear in a world's walk.

## Spec changes

src/sim/SPEC.md's Commands section gains, after its first paragraph:

    applyCommand can also take TickBuffers. When a command type has that form, stepWorld, makeCandidate, and commitCommands call it with their own buffers, so the command's module can keep the lists applying it works with.

The Units section's sentence "makeCandidate(world, commands, buffers, into) makes into the candidate." gains after it:

    makeCandidate(world, change, context, buffers, into) makes into the candidate of a change that isn't a queued command. It refills into from the world, calls change(into, context), and resolves into. A caller that has already planned an edit uses it to apply the plan without planning it again. If the resolving caller's TickBuffers holds a ResolveTrace in its ResolveTraceSlot, resolution tells it each step's name as the step starts and ends: each resolver by its name, and the finishers together as finishers. Both makeCandidate forms that refill into also tell it copy-world around the refill.

Memory in play's second paragraph becomes:

    Queueing commands and emptying the queue free no memory they allocated once the queue has grown. Applying a command passes the cycle's buffers. The allocation check holds the queue to it over each tool's release and each undo and redo on full.park.

src/sim/park/SPEC.md's Boundary list of public functions gains PlannedEdit, planEdit, applyPlanned, EditedParts, and editedParts, after editedSurface.

src/sim/park/SPEC.md gains, after the paragraph on editedSurface:

    planEdit plans an edit on a world into a PlannedEdit the caller keeps. It keeps the lists it works with in TickBuffers the caller keeps. Once they have grown, planning again allocates nothing. isAccepted on the plan says what isAccepted on the edit says. pathCrossings on the plan gives what pathCrossings on the edit gives. applyPlanned makes the change applyCommand makes, from the plan, on the world it was planned on or a copy of it, keeping the lists it works with in TickBuffers the caller keeps. editedParts gives what editedSegments and editedSurface give, from the plan, in EditedParts the caller keeps. Neither allocates once the lists have grown. applyCommand can also take TickBuffers the caller keeps, and then allocates nothing once they have grown. The returning forms of isAccepted, pathCrossings, editedSegments, editedSurface, and applyCommand plan with lists of their own, for callers that check or apply once.

src/legible/SPEC.md's Previews section: in the keepPreview paragraph, "Once its lists have grown, keeping the preview allocates nothing." becomes:

    It plans the edit once, and makes the candidate by applying that plan. Once its lists have grown, keeping the preview allocates nothing apart from the resolvers and finishers no feature has moved yet (sim/SPEC.md).

The section gains, after the list of what a shop preview tells:

    A preview holds the edit's plan, as planEdit gives it on the world, which the ghost reads. shopContext can also use a KeptFieldSample and TickBuffers the caller keeps, and then allocates nothing once they have grown.

src/render/SPEC.md's ghost paragraph: "buildGhostMesh(world, edit, candidate, lists) builds the same ghost in GhostLists the caller keeps, clearing them first. Apart from the lists isAccepted, editedSurface, editedSegments, and pathCrossings make for it, it allocates nothing once the lists have grown." becomes:

    buildGhostMesh(world, edit, planned, candidate, lists) builds the same ghost from the edit's plan, as planEdit gives it on the world, in GhostLists the caller keeps, clearing them first. It allocates nothing once the lists have grown.

src/tools/SPEC.md's sentence "The allocation check holds every tool on full.park to it (decision 0041)." gains before it:

    A path tool's press that adds a stop asks whether the world accepts the path with lists the ToolPark keeps, so it frees no memory it allocated either.

docs/conventions.md's Memory in loops: "Only ImGui's own drawing, previews that resolve a new world, and applying a command stay outside it (decision 0041)." becomes:

    Only ImGui's own drawing, and the resolvers and finishers changing-previews hasn't moved yet, stay outside it (decision 0041).

docs/testing.md's sentence "Only ImGui's own drawing, the previews that resolve a new world, and applying a command stay outside it." becomes:

    Only ImGui's own drawing, and the resolvers and finishers changing-previews hasn't moved yet, stay outside it.

src/legible/SPEC.md's arrival day paragraph gains, after its sentence on keepArrivalDay:

    It keeps a preview of the hours in the kept day, so once its lists have grown, making the day again allocates nothing apart from the resolvers and finishers no feature has moved yet.

Decision 0041's Decision section, second paragraph: "Applying the command resolves the world, so the watch pauses around it. Only ImGui's own drawing, previews that resolve a new world, and applying a command stay outside it." becomes:

    The watch pauses while the session applies the command. The commits below watch the same work. Only ImGui's own drawing, and the resolvers and finishers changing-previews hasn't moved yet, stay outside it.

Its last paragraph becomes:

    Previews of an edit that changes every frame, such as dragging a path, resolve the candidate each time. So does a preview that holds a candidate while an overlay is shown, which the app makes again at each tick. While changing-previews makes resolution re-derive only what changed, in storage it reuses, the check drags a shop and draws a path through the kept preview and the ghost, a tick between frames. It also commits edits there and back: a moved shop, a moved path point, and a segment's width restored as an undo restores it. In both, it watches everything except the resolvers no feature of that milestone has moved yet and the finishers. It pauses around those, and tells them apart by the trace resolution tells. watched-previews extends the watch to the whole resolution, and to the preview an overlay keeps at each tick.

Public interface, in src/sim/world.h:

    // Told the name of each step of a resolution as it starts and as it ends: each resolver by its
    // name, the finishers together as "finishers", and "copy-world" while makeCandidate refills a
    // kept world.
    class ResolveTrace {
    public:
      virtual ~ResolveTrace() = default;
      virtual void started(std::string_view step) = 0;
      virtual void ended(std::string_view step) = 0;
    };

    // The trace resolution tells, held in the resolving caller's TickBuffers. None by default.
    struct ResolveTraceSlot {
      ResolveTrace *Trace = nullptr;
    };

    // Makes into the candidate of a change that isn't a queued command: refills it with
    // copyWorld(world, into), calls change(into, context), and resolves it, passing the buffers to
    // every resolver and finisher.
    void makeCandidate(const World &world, void (*change)(World &into, const void *context),
                       const void *context, TickBuffers &buffers, World &into);

In src/sim/schema.h, CommandType.Apply becomes `void (*Apply)(World &world, const void *command, TickBuffers &buffers)`. addCommand's Apply calls `applyCommand(world, command, buffers)` when the type has that form, and `applyCommand(world, command)` otherwise.

In src/sim/park/edits.h:

    struct PlanLists;

    // An edit planned on a world, which the caller keeps from frame to frame. planEdit fills it, and
    // planning again into it allocates nothing once its lists have grown. It can be moved but not
    // copied.
    struct PlannedEdit {
      PlannedEdit();
      ~PlannedEdit();
      PlannedEdit(PlannedEdit &&) noexcept;
      PlannedEdit &operator=(PlannedEdit &&) noexcept;
      PlannedEdit(const PlannedEdit &) = delete;
      PlannedEdit &operator=(const PlannedEdit &) = delete;

      std::unique_ptr<PlanLists> Lists;
    };

    // Plans the edit on the world into planned, keeping the lists it works with in the buffers.
    void planEdit(const World &world, const ParkEdit &edit, PlannedEdit &planned,
                  TickBuffers &buffers);
    // Whether the world the edit was planned on accepts it, as isAccepted on the edit gives.
    bool isAccepted(const PlannedEdit &planned);
    // The crossings pathCrossings on the edit gives, from its plan.
    std::span<const ParkPoint> pathCrossings(const PlannedEdit &planned);
    // Makes the change applyCommand makes for the edit, from its plan, without planning it again.
    // The world is the one the edit was planned on, or a copy of it. Changes nothing when the plan
    // is refused. Keeps the lists it works with in the buffers.
    void applyPlanned(World &world, const ParkEdit &edit, const PlannedEdit &planned,
                      TickBuffers &buffers);

    // What editedSegments and editedSurface give for an edit, kept by the caller.
    struct EditedParts {
      std::vector<ParkSegment> Segments;
      PavedSurface Surface;
    };
    // Puts in parts what editedSegments and editedSurface give for the edit the plan holds, from the
    // plan, clearing them first and keeping the lists it works with in the buffers.
    void editedParts(const World &world, const PlannedEdit &planned, EditedParts &parts,
                     TickBuffers &buffers);

    // As above, keeping the lists applying works with in the buffers.
    void applyCommand(World &world, const AddPath &command, TickBuffers &buffers);
    ... one for each of the sixteen commands ...

In src/legible/preview.h, Preview gains `PlannedEdit Planned;` after Edit, KeptPreview drops Queue, and:

    // As above, sampling the footfall with the sample and keeping the lists it works with in the
    // buffers. Allocates nothing once they have grown.
    std::optional<ShopContext> shopContext(const World &world, const World &candidate,
                                           const ParkEdit &edit, KeptFieldSample &footfall,
                                           TickBuffers &buffers);

In src/render/park_mesh.h, GhostLists gains `EditedParts Parts;`, and the kept form becomes:

    // The ghost buildGhostMesh(world, edit, candidate) gives, from the edit's plan on the world, put
    // in lists.Ghost, using lists the caller keeps, which it clears first. Allocates nothing once the
    // lists have grown.
    void buildGhostMesh(const World &world, const ParkEdit &edit, const PlannedEdit &planned,
                        const std::optional<World> &candidate, GhostLists &lists);

## Files affected

- Modify: src/sim/world.h, src/sim/schema.h, src/sim/cycle.cpp, src/sim/SPEC.md
- Modify: src/sim/park/edits.h, src/sim/park/edits.cpp, src/sim/park/surface.cpp, src/sim/park/curves.cpp, src/sim/park/internal/layout.h, src/sim/park/internal/curves.h, src/sim/park/SPEC.md
- Modify: src/legible/preview.h, src/legible/preview.cpp, src/legible/arrivals.h, src/legible/arrivals.cpp, src/legible/SPEC.md
- Modify: src/render/park_mesh.h, src/render/park_mesh.cpp, src/render/SPEC.md
- Modify: src/tools/tools.cpp, src/tools/internal/tool_lists.h, src/tools/SPEC.md
- Modify: src/app/scene/scene_sync.cpp, which passes the preview's plan to the ghost
- Modify: src/bench/stages.cpp, which passes the kept preview's plan to the ghost
- Modify: tests/checks/allocations/allocation_check.cpp
- Modify: tests/sim/park_world_checks_test.cpp, in the test pass
- Modify: docs/conventions.md, docs/testing.md, docs/decisions/0041-allocation-check.md

## Dependencies

- derived-units: the ground units, the drag stages, the candidate check's drags, and the preview's candidate refilled in place. Merged.
- flat-ground: the kept lists of the ghost and the walkways. Merged.
- The milestone's decisions don't bear on this feature.

## Out of scope

Each of these has an owner in the milestone.

- Planning still copies the layout and derives the whole park's hypothetical ground. It scans every segment for meetings, and builds the surface around the changed points through whole-layout passes. The ghost derives the edited surface for drawing once per changed frame. All of this reads the whole park without allocating: layered-candidate reads only what the edit reaches.
- The candidate is still a copy of the world: layered-candidate.
- The resolvers no feature has moved yet, and the finishers: unit-surface, unit-connections, spatial-units, graph-updates, and local-carrying. The paved-surface and walked-surface resolvers still return new surfaces: unit-surface.
- The check's undo and redo rounds still pause while the session applies the command, because the session's buffers are its own. The check's commits watch the same applyCommand and planning code.

## Open questions

None.
