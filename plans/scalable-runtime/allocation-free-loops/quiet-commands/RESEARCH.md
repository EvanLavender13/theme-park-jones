# Research: quiet-commands

## Why queueing a command allocates, and what can keep its memory

The question is why each command the app queues allocates and frees, and how the queue can keep that memory instead. The findings come from reading src/sim/command_queue.h, src/sim/schema.h, src/sim/cycle.cpp, and each place the app queues a command.

CommandQueue holds each command as a QueuedCommand, which is the type's id and a std::any. push takes the command by value and moves it into the std::any. libstdc++'s std::any keeps a value in place only when it fits in a pointer's size and moves without throwing. DeleteSegment, DeleteBox, and RemovePoint hold one key and fit. Every other command holds more and goes on the heap. AddPath's stops and RestoreIntent's records are lists of their own, so copying either into the queue allocates those lists too. stepWorld and commitCommands empty the queue with clear(), which destroys each std::any and frees all of it. The queue's own list of QueuedCommand keeps its capacity across clear().

The app queues commands in three places. Interaction::useButtons queues the released edit through queueEdit, which visits the ParkEdit and pushes its command. applyStock and applyHours queue SetShopStock and SetOpeningHours. ParkSession::step and applyWithoutTick push the history's kept RestoreIntent onto the session's Alone queue. Each runs at a player action, so each allocates and frees once per action. The allocation check paused its watch around emptying its queue to let this through.

A queue that keeps a list of commands for each type, and reuses them, allocates nothing once grown. Queueing copy-assigns the command into the next kept value of its type. A std::vector's copy assignment reuses its capacity when the source fits, so AddPath's stops and RestoreIntent's records reuse their memory. Each command's own values hold no lists, so a smaller copy frees nothing either. Emptying the queue resets how many values of each type are in use, and keeps them. Each kept value sits behind its own pointer, so the address the queue lists stays valid while later commands of the same type are queued. The types stay erased behind a deleter, as TickBuffers does, so the queue stays a generic sim type. The schema's apply function then takes a pointer to the value instead of a std::any.

Rejected:

- Queueing references to commands the caller keeps. The scenarios and tests queue temporaries, and a queue that outlives its commands would read freed memory.
- A queue of ParkEdit values. The sim's queue serves every registered command type, including the cycle tests' own, so it can't name sim/park's commands.
- A std::any with a larger in-place buffer. AddPath's and RestoreIntent's lists would still be allocated at each copy and freed at each clear.
- A memory arena for the queue. It adds an allocator to every command type's lists, and the per-type kept values are simpler.

Applying a command resolves the world, and changing-previews owns that. This feature keeps queueing and emptying only.

Sources: src/sim/command_queue.h, src/sim/schema.h, src/sim/cycle.cpp, src/app/input/interaction.cpp, src/app/session/park_session.cpp; libstdc++'s <any> for its in-place rule.
