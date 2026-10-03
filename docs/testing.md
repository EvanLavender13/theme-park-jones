# Testing

What a test is, and what it is not (decision 0032). This document ranks below docs/principles.md and above every spec, plan, skill, and agent. Skills and agents apply these rules and cite them by number; they never restate them.

A test exists to catch a real defect that nothing else catches. It asserts a lasting property of a public interface: a standard, an invariant, or a contract that still holds after any refactor or retuning. A test that breaks when the code changes, rather than when the code is wrong, costs more than it catches.

## Rules

1. A test asserts a lasting property. Its expected outcome never depends on a tuning constant, a tick count, the outcome of one hand-built scenario, an exact text layout, field order, or vertex order, or a formula copied from the code. Its setup never depends on an exact text layout either: a case that reaches its state by editing a save's text breaks when the save layout changes. A concrete value appears only where the value itself is the contract another module or the player relies on. An invariant checked over a run also asserts that its subject happened, so it cannot pass on a run where nothing moved.

2. Each property is proven once, by the module that owns it. The world-as-value standards are the sim root's: a copy equals its world; a save loads back and resolves equal, and saves again to identical text; the hash covers all registered state; a candidate equals the world that commits the same commands; two runs give the same state. They are proven over every type the park schema registers, so a feature that registers state inherits them and restates none of them. Flow conservation is sim/medium's, and every other standard belongs to the module that defines it. A feature's plan names the existing standard that covers its new state instead of writing a criterion for it.

3. Nothing the compiler guarantees is tested. A function taking `const World &` cannot change the world, so whether it leaves the world unchanged is not tested, except where a `mutable` cache could break it.

4. One test per property, never one per input, error condition, entry point, field, or overload. A property is shown on a few inputs, each chosen for a reason that can be stated: a typical case, a boundary, an edge the spec names. A refusal is shown with one refused input. Random inputs serve only a statistical property, with the smallest sample its tolerance allows. A list of two is still a list.

5. Wiring is not tested. That one function forwards to another, returns its answer, or calls it is not a property, unless the wiring is itself a contract that nothing else exercises.

6. Test utilities get no tests: tpj_bench and its report, tpj_scenarios, the park generators, and the scripts. They are checked by running them.

7. An integration test checks only a consequence no single module can show, stated as lasting cause and effect: cutting a shop's supply starves it, and its guests go hungry. It never re-runs a module's standards over park files, and never compares tuned runs.

8. Specs state properties, not layouts. A spec that fixes vertex order, a formula's terms, or a tuning constant as its contract turns every test of it into a change-detector. Such detail belongs in the code, unless another module depends on it.
