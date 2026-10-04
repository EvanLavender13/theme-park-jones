---
name: researching
description: Use whenever the user asks to research something, or whenever a planning skill needs to research prior art, patterns, or implementation approaches. Runs the searches, synthesizes findings, and writes them to a RESEARCH.md sibling at the relevant plans node so the details outlast the session. Keeps current-state knowledge, not a log. Use this even for a quick look; findings that aren't written down are lost.
---

# Researching

## Purpose

Research a question and keep what matters. Run the searches, synthesize the findings, and write them to `RESEARCH.md` at the node the research serves. That way the details survive when the session ends.

The brief at that node keeps a one-line conclusion in its "Research notes" section. `RESEARCH.md` holds the details behind it.

## Hard gate

Synthesize; don't paste. Write findings in your own words. Don't copy source text into `RESEARCH.md`. Quote only where the exact wording matters, and keep any quote under fifteen words.

`RESEARCH.md` is current-state knowledge, not a log. Organize it by question. Use no dates. When new findings overturn old ones, replace the old text in place. Git holds the history.

Research informs; it doesn't decide. Record what was found and what it suggests. The user or the calling planning skill makes the decision.

## Checklist

Create a task for each item. Complete them in order.

1. State the question: Write one sentence on what is being decided or learned. If the request is broad, narrow it with the user before searching.
2. Identify the node: This is the plans node the research serves: a slice, capability, milestone, or feature directory. Its slug must already be decided so the file path exists. If no node exists, see "Research with no node" below.
3. Search: Scale the searches to the question. Start broad, then narrow. Follow the search tools' own guidance on building queries and judging sources.
4. Synthesize: Put the findings that matter into your own words. Capture what was found, what it means, and what it settles.
5. Capture the rejected options: For each approach you considered and didn't take, record the option and the reason. This is the most valuable part, because it stops the question from being argued again later.
6. Write RESEARCH.md: Put it at the node, next to the brief. Use the format below. If the file exists, update the relevant question in place instead of appending a new copy.
7. Refresh the brief: Update the one-line "Research notes" entry in the node's brief to the current conclusion.
8. Report: Tell the user the question, the conclusion, and where the details are written.

## RESEARCH.md format

```markdown
# Research: <node name>

## <Question>

Synthesized findings in prose. What was found, what it means, what it settles.

Rejected: <option> — <reason>. <option> — <reason>.

Sources: <url> — <what it contributed>; <url> — <what it contributed>.
```

Use one `##` section per question. Add a section when you research a new question at this node. When a question comes up again, update its existing section in place.

## Research with no node

Sometimes the research serves no existing plans node, such as an open question with no slice or capability yet. In that case, ask the user where it should attach. The options are:

- the nearest existing capability
- a node to be created next
- holding it in the session until a node exists

Don't invent a node, and don't write `RESEARCH.md` outside the plans tree.

## Process notes

When narrowing, ask one question per message. Settle the question with the user before spending searches on a broad request.

Cite enough to find the source again. A source is a URL plus a one-line note on what it contributed. Don't store full pages or long excerpts.

The details go here; the conclusions go in the brief. Someone reading the brief gets the answer. Someone who needs the reasoning opens `RESEARCH.md`.

## What this skill does not do

- Decide. It records findings; the user or the calling skill decides.
- Paste source text. It synthesizes.
- Keep a dated log. It keeps current-state knowledge and replaces outdated text in place.
- Write outside the plans tree.
