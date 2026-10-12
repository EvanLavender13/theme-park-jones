# Research: allocation-free-loops

## Where reused buffers live between ticks

The rule says to allocate a buffer once and reuse it. Today most reusing code keeps its buffers only for one call. stepGuests makes its WalkLists at the start of each tick (src/sim/guests/guests.cpp:989), so the lists grow back from empty every tick, and the audit didn't count that. To reuse a buffer across ticks, it has to live somewhere that outlasts a tick. Games use three places:

- Inside the world, as scratch that isn't part of its value: not walked, hashed, saved, or compared, and empty in a copy. The buffers stay with the world they serve. A world is a value here (principle 1, decision 0039's keyed storage), so this needs a stated exception to what a world holds.
- Per thread, as thread_local buffers or a per-thread scratch arena that each system clears before use. Engines often keep a per-thread linear allocator that resets after each frame, so a frame's temporary lists cost a pointer bump and no heap call. It needs no signature changes, but the state is hidden and global.
- Owned by the caller and passed in: stepWorld, overlay shading, and the ghost take a buffers object that the app, the runner, or a test owns and keeps. The state is explicit, and each caller decides how long its buffers live. Every caller of those functions changes.

## How a check tells a growing buffer from churn

The usual check replaces the global operator new and operator delete, and counts calls inside a scope. Fuchsia's AllocChecker and crpcut's heap assertions work this way. Unity checks for per-frame garbage-collector allocations, and its issue tracker treats a single per-frame allocation as a bug.

Growth to a new largest size is allowed, and so is the world's own storage growing when an entity is created. Repeating the same work twice from a restored save doesn't separate these from churn here. Loading a save makes the world's component stores and field lists again, so the second run would grow them again too.

A rule on block lifetimes separates them without repeating anything. Step the park past a warm-up, then count over a stretch of ticks. Fail if any heap block is both allocated and freed inside the stretch. A temporary list made and dropped in a tick fails. So does a list rebuilt every tick, which frees the one before it. A buffer growing past its largest size keeps its new block, and the block it frees came from before the stretch, so it passes. A guest created in the stretch keeps its storage, and one leaving frees storage made before the stretch, so both pass. The same rule applies to repeated calls of a display function, such as shading an overlay at every tick. A buffer that grows twice inside one stretch would be flagged by the rule alone. On full.park no warm-up is long enough for that: the crowd builds through the morning, so ledgers and queues keep doubling for game hours. The check therefore passes a free that is a list moving to a larger block: the heap call just before it allocated a larger block, still live, holding the freed block's contents at its start, or split around an inserted value. Temporaries and lists rebuilt each tick are freed without that, so they still fail.

The rule needs buffers that outlive the work: a buffer made at the start of each call fails it. Caller-owned buffers, passed in, give them that.

Rejected: counting allocations per tick against a limit — a limit is a number agents can game, and the rule's number is zero. Running the same stretch twice from a restored save — loading the save makes the world's storage again, so the second run counts its growth as well. Telling growth from churn by call site or block size — it needs stack traces and guesses.

Sources: https://gamedev.net/blogs/entry/2271578-introduction-to-allocators-and-arenas — linear allocators used as per-frame allocators; https://dev.to/jonahgoldsmith/temporary-allocations-in-cc-oi8 — per-thread scratch allocators reset after each frame; https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/scopestacks-public.pdf — Frostbite's scoped linear allocation; https://fuchsia.googlesource.com/fuchsia/+/refs/heads/main/zircon/kernel/tests/alloc_checker_tests.cc — counting allocations by overriding operator new; https://crpcut.sourceforge.net/1.9.4/heap-management.html — asserting a scope's heap use in a test; https://issuetracker.unity3d.com/issues/urp-has-a-gc-dot-alloc-every-frame — a per-frame allocation treated as a bug.

## What happens to the world's own storage

Each component store keeps its components in a std::map by key (decision 0039), and the world keeps its entity records the same way. Every add allocates a node, and every remove frees one. Seat service comes and goes as each piece fills and empties, so on full.park nodes are made and freed every few ticks. Under the lifetime rule that is churn, and it is. A node pool for each store returns a removed node to the pool, and the next add reuses it, so once the pools reach the park's size the nodes cost nothing. A component's own heap members still allocate when it is made and free when it is removed: seat service holds its free seats in a std::vector, so it moves to a fixed-size array, since a piece has at most a few seats. A std::map takes an allocator, so the key order, and with it every walk, save, and hash, stays as it is. A copy of the world gets its own pools, so candidates don't share memory with the world.

Rejected: leaving the world's storage out of the check — the churn would keep running in every tick, unchecked. Keeping seat service on a piece once added — it changes the saved form and the hash, so the simulation would no longer compare unchanged.

## What a kept preview must reproduce

A tick changes no intent, so a candidate's derived data stays valid while the edit is unchanged. Copying the world's state into a kept candidate isn't enough, though. makeCandidate applies the edit and resolves, and the finishers change state: carryGuests sets every guest's aim and turn on the new network, and settleSteppedEntries clears stepped entries the new derived data no longer matches. Applying an edit that adds a box also takes a key from the counter, which the world may give to an arriving guest at the next tick. A kept candidate must give exactly what makeCandidate gives at that tick (principle 8). The kept-preview feature finds how, and the park world checks compare the two.

## The work the app repeats while the park plays

The app's frame loop (src/app/application.cpp, runFrames) does this work in every frame, or at every tick, while nothing but the clock, the pointer's hover, and the views change:

- stepping the world, as many ticks as the speed asks;
- the scene's sync: finding whether intent changed, the seated guests, and the guest mesh at every tick;
- moving the pointer: snapping for path tools and the frontage search for place tools;
- the tool's tentative edit, its highlight, a path tool's start and finish marks, and Edit path's handles and hovered handle;
- the preview: kept while the edit is unchanged, made again at every tick while an overlay is shown;
- the overlay's shape check and its shades, and the ghost, built again when the look changes;
- the panels' data: the guest count, the departures, and the arrival day;
- the Inspector's inspection, the overlay tooltip's availability, the shop ghost's context, and the Graph view's overlay;
- recording the frame's time, which today keeps every frame's time for as long as the app runs.

The ghost's assembly, the look, and the panels' data are compiled only into the tpj_app executable today, so they move into tpj_app_core for the check to run them.

Uploading meshes and drawing need the GPU, and the panels' drawing needs ImGui. Everything above them runs without a window.

## The audit's places

An audit of main, before this milestone, found these allocations in per-tick and per-frame loops:

- Tick: a guest's choice (guests.cpp choose, scoreOptions, softmaxWeights, and sampleField's lists); stepShops' depot route lookups (operations.cpp nearestDepot, deliveryDelay, routeLengthsAt); stepDepots' stock copies and route lookups (shipGood); offer entries rebuilt per source, with swapSteppedEntries dropping Pending's capacity; admitGuests' anchored-node list per entrance; ledgerSwap's arriving and orphaned lists.
- Display work at every tick: shadeOverlay's stable_sort per line (paving_overlay.cpp); appendStarvedMarks' depot lookup per shop, appendWalkways' and appendStraightPath's temporary lists, and the Remove point ghost's appendEntity per segment (park_mesh.cpp); the Inspector's boxText per row (inspect.cpp).
- Every frame: SceneSync's intent snapshot rebuilding the ground (scene_sync.cpp); Edit path's handleAt, editHandles, and hoveredHandle rebuilding corners, edges, tapers, and segments (tools.cpp); the Inspector's strings; Furnish hover's segment copies (guestSegmentAt); snapping's scene and anchors (snapping.cpp); the frontage search's lists (frontage.cpp); the ghost mesh (scene_uploads.cpp); the tooltip's contributions and the Graph view's overlay; the Debug panel's guest key copy.

Reviews of this milestone added: the tick's working lists made again each tick, such as stepGuests' WalkLists; nodes allocated and freed as components and entities come and go, such as seat service; the preview made again and resolved at every tick; the ghost validating its edit at every build; the guest mesh and the seated guests rebuilt at every tick; and parkGuests' key list.
