# Research: sidestep

The milestone's RESEARCH.md gives the reasons for predicting the nearest approach and the models it follows. This file covers how guests do it in today's code.

## What does a guest publish, and when?

Each guest sets its entry in guest presence at the end of each tick it walks: where it stands, and its velocity over that tick, which is how far it moved divided by SIM_TICK_SECONDS. The entry becomes readable at the swap that ends the tick. So every guest that steps in the next tick reads where the others stood at the start of that tick and how they were moving, whatever order guests step in (principle 10).

A guest waiting at a shop drops its entry on the tick it starts waiting, and sets it again on the first tick it walks. Waiting guests all stand on the door's point. If they stayed in guest presence, a guest walking to that door would step aside from a point it has to reach. Dropping only on the tick it starts waiting keeps the work in proportion to the guests that change. A drop counts as the guest's change for the tick even when it has no entry, and inserts a marker into the tick's working list. Dropping every tick it waits would insert one marker for each waiting guest each tick.

A guest that leaves drops its entry before it is destroyed. The swap would also remove the entry of a guest that is gone, but the milestone asks for the drop, and it makes leaving explicit in the guests module.

A new guest has no entry until the end of its first step. addGuest is also called outside stepping, by park generators, where a change to a ground field is refused. Guests arrive at each entrance 60 ticks apart, so a guest that isn't yet sensed has no one near it.

Across an edit, resolution leaves the entries as they are, so a guest the edit moved is sensed where it stood before the edit until it next steps. Finishers can't change a ground field, since changes come only while stepping. The difference lasts one tick.

Rejected: publishing from the finisher after an edit — it isn't stepping, so the change is refused. Keeping waiting guests in guest presence and ignoring guests that stand still — a guest stalled in the walk also stands still, and the door's point would still be crowded with entries every other walker samples.

## How does a guest predict a collision?

A guest walking toward its aim has a velocity: the unit direction toward its aim times its speed. Each sampled guest has its entry's position and velocity. The relative position r is the other's position less the guest's, and the relative velocity w is the other's velocity less the guest's. If r·w is negative, they are closing. The time of nearest approach is t = −(r·w)/(w·w), and their separation then is c = r + w·t. This is Reynolds' unaligned collision avoidance, and it is the time-to-collision idea Karamouzas' model weights its response by. It needs only + − × ÷ and one square root, for the length of c (decision 0022).

A guest ignores another when:

- the other isn't ahead of it: r·d is at most 0, where d is its direction. People don't react to those behind them, and the one behind reacts instead. Without this, a guest being overtaken and the guest overtaking it would both step the same way.
- they aren't closing, or move at the same velocity (w·w is 0). Guests walking side by side at one pace don't swerve.
- the nearest approach is more than SIDESTEP_HORIZON away.
- the separation then is at least SIDESTEP_DISTANCE.

## How far does it step aside?

For each threat, the guest steps aside at the speed that would open the separation to SIDESTEP_DISTANCE by the time of nearest approach: (SIDESTEP_DISTANCE − |c|)/t. It steps away from where the other would be: left if the other would be right of it, right if left, and right if dead ahead, the same side its lane keeps. The speeds of all threats add, signed by side, and the sum is held within SIDESTEP_SPEED either way. The step aside over the tick is that speed times SIM_TICK_SECONDS, square to its direction.

The guest works this out again every tick from the new positions. If it stepped too little, the time left is shorter and the speed it needs rises. If the separation has opened, the threat drops out. Both guests in a head-on meeting step aside, each by the whole amount, so the separation opens early and the threat ends. Its response falls to 0 as the separation reaches SIDESTEP_DISTANCE, so it doesn't jerk on and off at the edge. Once the threat ends, the guest walks toward its aim on its side again, and the aim, up to AIM_LOOKAHEAD ahead, draws it back gently.

A guest overtaking one 0.3 m/s slower reaches a nearest approach 3 s away when it is 0.9 m behind. With SIDESTEP_HORIZON 3 s, it starts stepping out then. A head-on meeting closes at up to 3.2 m/s, so within the 3 m radius it is seen about a second before. Opening 0.9 m in that second needs 0.9 m/s between the two guests. Each steps aside at up to SIDESTEP_SPEED, 0.6 m/s, so together they open 1.2 m/s, which covers it. A sideways 0.6 m/s against a walk of 1.0 to 1.6 m/s turns a guest by at most about 30°.

Rejected: a force that only depends on distance — guests swerve at walkers passing safely, as the milestone's research records. A weight that falls with the time to approach, not tied to the separation needed — it has no natural scale, so the step is too small for head-on meetings or too large for slow overtaking. Stepping aside by half the needed amount, trusting the other to do the rest — a guest being overtaken doesn't react, so the overtaker would close too slowly.

## How is the sidestep kept on the paving?

walkTowardAim walks a guest straight toward its aim, and relies on the line to the aim being clear: advanceAim only checks lines from where the guest stands, and the next pass starts from the aim it saved. The sidestep is applied on the pass that uses up the rest of the tick's walk, where the guest stops short of its aim. There it adds the step aside to the step toward the aim. It keeps the stepped point only if the line from where it stands to the stepped point is clear, and the line from the stepped point to its aim is clear too, both by GuestGround's clearLine. Otherwise it takes the plain step. So the box stays on the paving, and the guest can always still walk straight to its aim. It costs two clear-line tests for each guest that steps aside in a tick, and none for the others.

When a pass reaches the aim, the guest stands at it, and reachedEnd can still find it at the end of its route. A step aside is only ever added on a pass that stops short of the aim.

A guest's place then follows from where it stands, as before (nearestSpot), so route distance and its choices read the same place.

Rejected: moving the guest's side for a while instead of its step — the side moves every point of its route, which needs fit tests for each, and it would need state saying how long the move lasts. Moving the aim aside — the aim is saved as a spot on the route, and an aim off the route would break advanceAim's checks.

## What does it cost?

Each walking guest samples guest presence once a tick within SIDESTEP_RADIUS, 3 m, which covers at most four of the field's 4 m cells. The sample's work grows with the guests near it (ground-fields criterion 7). The prediction is a few multiplications for each guest sampled. The set at the end of the tick is a binary search into the tick's working list, which is copied once a tick from the readable entries. Nothing grows with the size of the park (decision 0031). The runtime report on full.park measures it.
