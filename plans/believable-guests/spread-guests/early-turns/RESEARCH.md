# Research: early-turns

The milestone's RESEARCH.md gives the reasons to choose before a node and to walk through shaping points. This file covers how guests do it in today's code.

## Where does a wanderer's pick live between choosing and reaching the node?

Today a wanderer picks its edge at the node itself. walk calls wanderStep while the guest stands on the node, and the draw is keyed on the tick. If the pick moves 10 m ahead of the node, the guest must remember it for the 8 or so seconds until it gets there. A pick drawn again each tick would come out differently each time. So the guest needs one new piece of saved state: the step it takes out of the node it has chosen for.

That same step also records that the guest has chosen for the node. A guest heading to a shop or home needs only that record, because route distance gives its way out of the node. For those guests, the step is an empty step at the node. Its carrier and From still name the node.

Clear the step when the guest's place passes its node. Without that, a wanderer on a loop with only one junction comes back to the same node and finds its old choice still there. It would never choose again, so it would never head home.

Rejected: a pick drawn without the tick, keyed on the guest and how many nodes it has passed. It needs a saved counter anyway, and it ties the pick to how nodes are counted. Rejected: storing a planned route of several picks. One pick, for the next node, is all the aim needs.

## How is "10 m before the node" measured?

Measure it along the route ahead from the guest's place, adding the network distance of each stretch. This is the same distance route distance uses. The place lags a little behind where the guest stands on wide paving, so a guest chooses slightly later than 10 m by straight-line distance. That doesn't matter at this scale. AIM_LOOKAHEAD is 30 m, so the node is always inside the traced route.

## What happens at a dead end?

At a path's free end or a door, the only way out is the way back. If the route ran on past the node along that way, it would double back toward the guest. The aim would then find a clear line to a point behind the guest, and the guest would turn back up to 10 m short of the end. Its place would also jump to the returning stretch, so route distance would count the end of the path that the guest never walked. When the picked way is the way back, the route stops at the node. The guest walks as far toward the end as its box allows and then turns, as it does today.

## What happens to a choice across an edit?

Forgetting every guest's choice on every edit would make each guest within 10 m of a node choose again whenever the player builds anything, anywhere. Wanderers would visibly change course across the park at each commit. A guest keeps its choice if the edit leaves its node where it was. A wanderer also needs the edge it picked to be whole and where it was. carryOver shows this when it keeps each end of the turn on the turn's own carrier at the same ground position, within ON_GROUND_TOLERANCE. Compare ground positions, not distances, because an edit that moves a carrier's start point changes every distance along it. The network shows that the node is still a node and that the edge still runs between the same two stops. Otherwise the guest forgets the choice and makes it again, once it is within 10 m of a node.

Rejected: carrying the step's places across with carryOver and keeping whatever they land on. An edit that splits the picked edge would leave a step that runs over a new node without stopping there. If that node is a new junction, the guest would walk past it without choosing.
