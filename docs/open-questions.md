# Open Questions

Undecided. These are Evan's to decide; ask about them one at a time instead of deciding them in code. When one is settled, record it as a decision in docs/decisions/ and remove it here.

Whether and how to reuse the planning skills from SteelJones. That project used a capability, milestone, feature, plan tree. It worked, but cross-cutting mechanics felt wrong because a tree gives each thing one owner. The intended fix is to keep the tree but put the principles above it and check every plan against them: a plan that makes one system depend directly on another's internals is a signal to route the interaction through the shared medium instead. SteelJones also has commit-message and pre-commit hooks and a reviewer agent that could be carried over.

Whether the SteelJones CONVENTIONS.md (orthodox C++) applies here. The skeleton follows its naming but does not adopt the whole document.

Which ECS to use, or whether to write one. The choice interacts with principle 6, since a typical ECS lets any system read any component.

Which immediate-mode UI library to use for tooling. Dear ImGui is the obvious candidate and has an SDL_GPU backend.

Staff and supplies as true agents or aggregate flows at first.

The guest decision rule and how its factors are weighted.

The shape of gradient curves (how quality responds to context).

Continuous integration: where it runs and what it gates.
