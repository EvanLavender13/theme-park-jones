# Research: quiet-tick

## What the tick allocates on full.park

An inventory of main read every system and swap that runs in a cycle with no commands, and everything they call. No resolver or finisher runs in such a cycle. These allocate and free memory at every tick:

- stepGuests:
  - its WalkLists, made at the start of each tick;
  - parkGuests' key list;
  - parkMoment's entrance list, and the lists behind parkEntrances and parkHours, which runs twice a tick;
  - admitGuests' anchoredNodes list and second parkEntrances list for each entrance;
  - the leaving list.
- A guest's choice:
  - sampleField's slot list, its result, and the scratch, run, and single lists sampleSlotsAt makes on every call;
  - scoreOptions' three lists;
  - softmaxWeights' list.
- stepFootfall:
  - its parkGuests list and its list of guests on stretches;
  - the temporary buffer std::stable_sort takes.
- stepShops:
  - its ShopBuffers, made each tick;
  - parkBoxes twice, and boxKeys three times for each shop;
  - routeLengthsAt twice for each shop, each making an anchoredNodes list, a sampleField per anchored node, and its result list;
  - stockOf copies in abandonGone;
  - offerEntries' anchoredNodes list and entries list, for each published offer.
- stepDepots: boxKeys twice, stockOf copies for each depot and good, and routeLengthsAt for each held order.
- stepFacilities: its FacilityBuffers, made each tick, parkBoxes, stockOf copies, and offerEntries for each facility.
- stepSeats: its SeatBuffers, made each tick, and seat service added and removed with its free-seat vector.
- The stepped-field swap moves Pending into Readable. That frees the old slots, each slot's entries, and each slot's order by place. It also leaves Pending with no capacity. Only the four offer fields publish stepped entries, so they pay this. Then every slot's order by place is built again at its first read, with a new list and the temporary buffer std::stable_sort takes.
- ledgerSwap copies the due packets into a new list for each of the 13 flow kinds, and keeps orphaned stocks in another.
- Each guest that arrives adds a node to the world's entity map and to the guest store, and each guest that leaves frees both.

These keep their memory already:

- the ground field's swap, which trades its two lists;
- the kept field's swap;
- the ledgers' own lists;
- the shop and facility service components, which are added once and never removed;
- the lists that stepSeats, stepShops, and stepFacilities share between the shops of one tick.

Name strings built from a kind's name on each call fit libstdc++'s 15-character short-string buffer, so they never reach the heap.

## Where the systems' lists live

Decision 0041's milestone settled that the stepping caller owns the buffers and passes them in. Systems and swaps are plain function pointers that take only the world today. They need the caller's buffers as a second argument.

Each module keeps different lists, and principle 6 keeps each module's internals private. A type-keyed holder lets each module ask for its own private struct by type: get<WalkLists>() makes the struct on first use and returns the same one at every later tick. The schema already names types by typeIdOf, so the holder can look them up the same way. A short list searched linearly does it, since about a dozen types ask. Lookups allocate nothing after the first.

A buffer's contents never carry meaning from one tick to the next. If a system cleared none of its lists, a world's results would depend on its caller. Every system already clears a list before filling it, and the fixes keep that pattern. tpj_scenarios --compare across the change shows that kept buffers give the same hashes as fresh ones.

Rejected: keeping the buffers in the world as unwalked scratch. The milestone chose caller-owned buffers, and a world is a value. Rejected: thread_local buffers. Their state is hidden and global, and the milestone rejected them.

## How the world reuses removed storage

std::map's extract takes a node out of the map without freeing it, and insert puts a node handle back in at its key's place. If a store keeps the node handles it extracts as spares, the next add can take a spare, set its key and value, and insert it. Once a store has held as many components as it ever holds at once, adds and removes allocate nothing. The map still orders by key, so the walk, saves, hashes, and equality are unchanged (decision 0039). A copy of a store copies only its components and gets no spares. The same applies to the world's entity map.

This gives what RESEARCH.md for the milestone called a node pool, without a custom allocator. A pool allocator would need std::map's allocator parameter on every store and on the world's map, and a pool object that a moved World can't leave behind.

Setting a spare's value assigns a new value over the old one. If a component owns a vector, that assignment frees the vector's old memory. Seat service is the only such component that is added and removed in play. Its free-at list holds one tick for each taken seat, and a piece has at most four seats (furnitureSeats). A list with a fixed capacity of four holds it with no heap memory at all. To leave saves and hashes unchanged, the walk, save, and load must treat that list exactly like a vector: its size, then its values, written between brackets.

Rejected: keeping seat service on a piece once added. That changes the save and the hash. Rejected: recycling component values and their vectors in the store. It needs a per-type rule for resetting a value while keeping its memory.

## Stepped fields that publish every tick

The four offer fields publish a slot for each shop or facility at every tick. To keep their memory, the swap must keep the slots it retires. It can move each one's entries list into a retired list that the walk never visits, since a moved vector keeps its buffer. Then the next publish takes a retired slot, clears its entries, and fills them. Each slot's order by place needs the same treatment. If it is a kept list with a flag saying whether it is current, rebuilding it clears the list and refills it instead of making a new one.

std::stable_sort takes a temporary buffer from the heap. Sorting positions with std::sort by place, then by position, gives exactly the stable order, because no two positions are equal. Footfall's stable sort by stretch can do the same with each guest's index in key order.

## The check's bookkeeping

The check replaces operator new and delete in its own executable. Windows executables link the runtime statically (cmake/ProjectOptions.cmake), so every allocation in the program goes through the replacement, including those inside the standard library.

While a stretch is watched, the replacement records each block it allocates in a fixed open-addressed table of addresses. The table is a static array, so recording allocates nothing. When a block in the table is freed, that block was both allocated and freed during the stretch, and the check records the block's size, the tick it was allocated at, and the tick it was freed at. A block that was allocated before the stretch isn't in the table, so freeing it passes, as a growing buffer does when it replaces its old block. To find where a reported block comes from, stop at the check's report function in a debugger.

The rule only sees a block whose whole life falls inside the watched ticks. A seat service lives at least SEAT_TIME, 300 ticks, and longer while guests keep sitting, since it ends only when its piece empties. The check sees one start and end only if its piece empties within the watched ticks. Twice SEAT_TIME gives room for that, and turning node reuse off proves that it happens on full.park. A guest stays for game hours, thousands of ticks, which debug can't step in a check. The check therefore can't see a guest's storage made and freed. Reusing the entity map's nodes still keeps guests arriving and leaving from allocating, but no check proves it.

windows-debug steps full.park at about 5 ticks a second in tpj_scenarios, which also hashes the world after every tick. The tick alone is faster. A warm-up and a stretch of a few hundred ticks each therefore takes a minute or two in ctest.

Sources: https://en.cppreference.com/w/cpp/container/map/extract — taking a node out without freeing it, and inserting a node handle; https://en.cppreference.com/w/cpp/memory/new/operator_new — replacing the global allocation functions; https://en.cppreference.com/w/cpp/algorithm/stable_sort — stable_sort's temporary buffer.
