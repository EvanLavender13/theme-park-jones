# Research: placement-step

## What is the Placement section, and how does the check recognize it?

Five plans already carry one, written after decision 0027 under the guidance that preceded this feature: the four explained-food plans and layer-check's. Each is a "## Placement" heading between Approach and Tasks, a sentence citing 0027, and one bullet per behavior naming the module, the component, and why it owns the behavior, with main.cpp's share stated as composition only. That shape becomes the format.

The check recognizes the section by its heading alone: a line that is "## Placement", with nothing after it but blanks. Whether the placements are right is judgement, which the reviewer carries, so the check does not read the section's body. CMake's file(STRINGS) drops carriage returns, so a plan saved with CRLF line endings reads the same as one saved with LF. Without ENCODING UTF-8 it reads ASCII strings and ends one at any byte above 0x7F, so a heading followed by an em-dash and more words would split into a line that matches; read as UTF-8, the line stays whole.

Rejected: requiring a non-blank line under the heading — it refuses only an empty section, and placeholder text passes it just as easily, so it buys nothing review does not. Matching the word Placement anywhere — prose that mentions the section would pass a plan that has none.

Sources: cmake --help-command file — file(STRINGS) ignores carriage returns; plans/legible-simulation/explained-food/*/PLAN.md and plans/sound-architecture/layered-dependencies/layer-check/PLAN.md — the format in use.

## How does the pre-commit hook hand the check the plans a commit adds?

git diff --cached --name-only --diff-filter=A lists the files the index adds. Rename detection decides whether a moved plan counts as added, and its default follows the diff.renames setting, so the hook passes -M to make a move a rename whatever the configuration. A plan moved to a new directory then keeps passing, as a plan already in the tree should. The pathspec ':(glob)plans/**/PLAN.md' limits the list to plans under plans/ by their exact name.

What is committed is the index, not the working tree, and a plan can have unstaged edits. git checkout-index --prefix=<dir>/ writes the staged version of each named path under a temporary directory, keeping the paths, so the check runs with ROOT set to that directory and reports each plan by its path in the repository. The existing format pass treats staged and unstaged versions apart for the same reason.

The check runs only when the commit adds a plan, so cmake is needed only then. The pre-push hook already calls cmake by that name. When a plan is added and no cmake is on the PATH, the hook refuses the commit and says why, since letting it through would let a plan without the section land unnoticed.

Rejected: reading the plans from the working tree — a section added but not staged would pass a commit whose plan lacks it. Having the script call git itself — the planted cases would each need a repository, where now they need only files.

Sources: git help diff — --diff-filter, -M; git help checkout-index — --prefix exports index entries under a directory; .githooks/pre-commit and .githooks/pre-push — the hooks' existing shape.

## Can ctest exercise the hook itself?

Not simply on both builds. A ctest case would build a scratch repository and commit in it with core.hooksPath set to the source tree's .githooks. On Windows, git runs the hook under Git for Windows's bash, which rewrites path-like arguments passed to a native cmake.exe, and the temporary directory the hook makes is such a path. The script carries the rule and is tested on both builds by planted cases; the hook's plumbing is a few lines, checked by hand when it is written, and a ctest of it is a backlog item.

Sources: https://www.msys2.org/docs/filesystem-paths/ — automatic conversion of path-like arguments to native programs.
