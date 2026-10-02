# 0029. Undo and redo commit without a preview

Status: Accepted, 2026-10-01

## Context

Principle 8 says every change can be previewed before it is committed. Placements, moves, and deletions meet it through ghosts and candidate worlds. Planning the paths-and-plazas slice added undo and redo of intent edits. An undo can delete or restore a shop or a path, with the same consequences as any edit, and making it previewable would mean a hover or hold-to-preview gesture on every undo, which slows the most frequent correction a builder makes.

## Decision

Undo and redo commit at once, with no preview. They are the one exemption from principle 8's preview requirement. Every other edit, including each of the tool edits an undo inverts, keeps its preview.

The exemption rests on what undo is for: it returns the park to a state the player has already seen, or forward to one they just left, and it is itself reversed by one more keystroke. Outcomes after an undo stay traceable to their causes, the other half of principle 8.

## Consequences

The slice's preview criteria cover every new edit but not undo and redo. A later feature may add an optional preview, such as showing the inverse edit's ghost while the undo control is hovered, without changing this record. Any other edit that wants to skip its preview needs its own decision.
