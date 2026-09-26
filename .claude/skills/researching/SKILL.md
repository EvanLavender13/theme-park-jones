---
name: researching
description: Use whenever the user asks to research something, or whenever a planning skill needs to research prior art, patterns, or implementation approaches. Runs the searches, synthesizes findings, and writes them to a RESEARCH.md sibling at the relevant plans node so the depth survives the session. Retains current-state knowledge, not a log. Use this even for a quick look; an unrecorded finding is a lost finding.
---

# Researching

## Purpose

Research a question and retain what matters. Run the searches, synthesize the findings, and write them to `RESEARCH.md` at the node the research serves, so the depth is retained instead of lost when the session ends.

The brief at that node keeps a one-line conclusion in its "Research notes" section. `RESEARCH.md` holds the depth behind it.

## Hard gate

Synthesize; never paste. Write findings in your own words. Do not copy source text into `RESEARCH.md`. Quote only where exact wording is load-bearing, and keep any quote under fifteen words.

`RESEARCH.md` is current-state knowledge, not a log. Organize by question. Use no dates. When new findings overturn old ones, replace the old text in place. Git holds the history.

Research informs; it does not decide. Record what was found and what it suggests. The user or the calling planning skill makes the decision.

## Checklist

Create a task for each item. Complete in order.

1. State the question: One sentence on what is being decided or learned. If the request is broad, narrow it with the user before searching.
2. Identify the node: The plans node this research serves: a capability, milestone, or feature directory. Its slug must be decided so the file path exists. If no node exists, see "Research with no node" below.
3. Search: Scale searches to the question. Start broad, then narrow. Follow the search tools' own guidance on query construction and source quality.
4. Synthesize: Pull the findings that matter into your own words. Capture what was found, what it means, and what it settles.
5. Capture the rejected options: For each approach considered and not taken, record the option and the reason. This is the highest-value part; it stops the question being re-litigated later.
6. Write RESEARCH.md: At the node, as a sibling to the brief. Use the format below. If the file exists, update the relevant question in place rather than appending a new copy.
7. Refresh the brief: Update the one-line "Research notes" entry in the node's brief to the current distilled conclusion.
8. Report: Tell the user the question, the conclusion, and where the depth is written.

## RESEARCH.md format

```markdown
# Research: <node name>

## <Question>

Synthesized findings in prose. What was found, what it means, what it settles.

Rejected: <option> — <reason>. <option> — <reason>.

Sources: <url> — <what it contributed>; <url> — <what it contributed>.
```

One `##` section per question. Add a section when a new question is researched at this node. Update an existing section in place when its question is revisited.

## Research with no node

When the research serves no existing plans node — an open question with no capability yet — ask the user where it should attach. Options: the nearest existing capability, a node to be created next, or held in the session until a node exists. Do not invent a node or write `RESEARCH.md` outside the plans tree.

## Process notes

One question per message when narrowing. Settle the question with the user before spending searches on a broad request.

Cite enough to re-find. A source is a URL plus a one-line note on what it contributed. Do not store full pages or long excerpts.

Depth lives here; conclusions live in the brief. Someone reading the brief gets the answer. Someone who needs the reasoning opens `RESEARCH.md`.

## What this skill does not do

- Decide. It records findings; the user or the calling skill decides.
- Paste source text. It synthesizes.
- Keep a dated log. It keeps current-state knowledge, superseded in place.
- Write outside the plans tree.