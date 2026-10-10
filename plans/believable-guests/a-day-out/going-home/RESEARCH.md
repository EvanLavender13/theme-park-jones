# Research: going-home

The milestone's RESEARCH.md settles how going home is scored and why its largest term names the reason. This file covers how to build the parts the milestone leaves open.

## How should guests notice that the park has closed or opened?

Guests need two moments: when the park closes, so every walking guest chooses at once, and when it opens, so the departure tally starts again. The clock is a pure function of the tick (src/sim/clock.h), and the hours are intent. So a step can tell whether the clock has just passed a time by asking whether the park was open at the previous tick and is closed at this one, both under the park's current hours. That needs no saved state. It also can't depend on the order systems run in, because both ticks' times come from the tick alone.

If the player changes the hours so that the park is closed now and was open a moment ago under the old hours, no moment is seen, because both ticks are judged by the new hours. Guests still pick going home at their next node, since the closing term is then at its full value. This is a rare edit, and the next node is at most a few seconds away.

Rejected: a saved flag for whether the park was open at the last step — it adds saved state for the same answer, and an hours edit would make the flag disagree with the hours. A day number saved on the tally, compared each tick — the tally would start again at midnight, not at opening.

## Where should the departure tally live?

The tally is saved state that only guests write. It needs one entity of its own, as the milestone says. The world gives a fixed key for such an entity through createDerivedEntity with a purpose and no owner, as the ground fields and guest ground use. The entity must be created by a resolver. A save holds no entity's derived origin, and a loaded park gets it back only when a resolver calls createDerivedEntity again, so an entity that a system created would load back unequal to the world saved. A resolver that creates the tally when it is missing means every resolved park holds it, with zeros until someone leaves. So the park world checks cover it in every park they open, even before a guest leaves.

Rejected: creating the tally in the guests system, at its first step or the first departure — the entity wouldn't load back equal, and a park where no guest has left yet wouldn't hold it. Keeping the counts on the opening-hours entity — that entity is intent owned by the park module, and guests may not write another module's component (principle 6).

## How should the Inspector show going home's terms?

Going home has seven terms: a base, five reasons, and the commitment bonus. Offers have four: relief, distance, wait, and commitment. Adding six columns to the choice table would leave most cells empty on every row. Keeping going home's row in the table, with its reason in the Need column and its commitment, score, and chance in theirs, keeps the table readable. A second two-column table below it, headed Going home, lists the base and each reason's term. RCT and Planet Coaster show a guest's thoughts as text, not terms, so the genre gives no layout to copy. This is the project's own explanation display (principle 8).

Rejected: six more columns — mostly empty. Only the largest reason — it hides how close the others were, which is the point of terms.
