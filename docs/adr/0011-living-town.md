# ADR 0011: Living town (S21-S25) — a simulated town with a built-in model

- Status: **accepted; planned** (user, 2026-11-10). Nothing implemented yet.
- Context: the user asked what one thing would give an Umbra game a look and feel other engines
  don't reach easily, reviewed the options and the market (below) and chose an **open-ended
  storytelling simulation RPG**. Decisions are the user's; the design that follows them is Claude's
  proposal and can change phase by phase.

## The user's decisions (2026-11-10)

| # | Decision |
|---|---|
| 1 | Goal: **ship and sell a game** made with Umbra. |
| 2 | An authored **main plot that always wins in the end**, inside a **simulated world that is the core gameplay and the hook**. |
| 3 | A **built-in AI model runs in the shipped game** and makes the world proliferate. It is **required**, not optional. The game stays fully offline: the model ships with the game. |
| 4 | Every playthrough must produce **different outcomes**. The **developer manages the outcomes** so they fit the main story (otherwise it is a normal story with few surprises). |
| 5 | Scale: a solo developer. **One small town with about seven characters** is enough. |
| 6 | It is done **after S9**. |
| 7 | The theme is open: it need **not be fantasy**; it can be a modern, everyday-life RPG. |

These supersede two earlier rules (CONTEXT C07/C08 reading, CLAUDE.md product constraints):
"quest/dialogue generation is an authoring feature, not a mandatory runtime LLM" and "always-on
runtime conversational LLMs" as out of scope. For a game that uses the living-town plugin, a
bundled local model **is** part of the game. Still true: no cloud, no account, no download after
installation, and the authoritative simulation runs without any model (tests use a stand-in).

## Why this direction (research summary, 2026-11-10)

- The genre sells for very small teams and keeps selling for years: Kenshi (one developer for most
  of its life) passed 3 million copies in May 2026; Dwarf Fortress (two people) passed 1 million
  on Steam; Bannerlord passed 5 million; Wildermyth (six people) mixes handcrafted and procedural
  story layers.
- It fails in known ways: the simulation is invisible to the player (the "Tale-Spin effect"),
  autonomy produces nonsense (Oblivion's Radiant AI), procedural stories repeat, and the hits took
  about a decade.
- It is **not** something other engines cannot do (RimWorld runs on Unity; Kenshi 2 is being made
  in Unreal). Umbra's edge is narrower and real: rules that already run headless, one validated
  intent path for every actor, durable ids, and fast seeded tests.
- Legal caution: Warner Bros' "Nemesis" patent (US, until 2036-08-11) is reported to cover
  procedurally generated NPCs in a hierarchy that changes with the player's actions. This plan has
  no enemy promotion ladder; get advice before adding one. (Claude is not a lawyer.)

## Design

### 1. The simulation decides, the model proposes and words

```text
world tick (deterministic) --> legal intents and facts --> model (worker thread)
        ^                                                    | proposal: one intent / one event /
        |                                                    v           one spoken line
   state changes  <-- validator (the same one players and AI use) <--+
```

- Residents are full RPG characters (the player's stats, skills, inventory rules). They have
  needs, a schedule, relationships and a memory of events. All of it is ordinary game state.
- The model never writes state. It **chooses** among the legal intents the simulation offers a
  resident ("why would Tobin do this now?"), **proposes** events of kinds the developer authored,
  and **words** lines, rumours and chronicle entries from a short brief of facts.
- Every proposal goes through the validator. An invalid or late proposal is dropped and the
  conventional choice is used for that one decision (the existing rule: a timeout triggers
  fallback; it is logged separately). This is failure handling, not an optional mode.
- What the model produced is **stored** (chronicle, save). Loading a save never asks the model to
  repeat itself, so saves stay exact even though model output differs between machines.

### 2. Different outcomes, managed by the developer

- A playthrough has a world seed. Surprise comes from the seed, the player's actions and the
  model's proposals.
- The developer authors an **outcome plan** (a data file): story beats with their conditions and
  time windows, which outcomes are allowed, required or forbidden per beat, which residents may
  die or leave and when, and a pacing budget (how much trouble at once). The **storyteller** reads
  it and steers: it filters proposals, raises or lowers pressure, and forces a beat when its
  window closes. So the main plot always arrives, by a different road.
- A headless **soak test** fast-forwards many seeds and reports the spread of outcomes per beat,
  broken states (a required resident dead, a beat unreachable) and repetition. This is the
  developer's instrument for "managing outcomes", and the main thing a general engine does not
  give for free.

### 3. Seen by the player

A chronicle screen, rumours between residents, changed greetings and prices, and warnings before
consequences ("three days until..."). If the player cannot see it, it does not count.

### 4. Theme-neutral

Nothing in the plugin says "fantasy": needs, event kinds, resident roles and the outcome plan are
data. Fights are optional (combat plugins are already optional per project). A modern
everyday-life town (jobs, money, friendships, a secret) uses the same systems as a village
threatened by bandits. The reference project keeps its fantasy content until the user decides
otherwise; S25 may be a new project.

### 5. The model

- Candidate: llama.cpp (C++) inside a new plugin, running a small open-weights model on a worker
  thread, CPU first. Example sizes checked 2026-11-10: Qwen3 0.6B at 4-bit about 0.4-0.5 GB,
  1.7B about 1.1 GB, Apache-2.0. The choice is made in S23 by measuring on the user's PC: speed,
  memory beside the renderer, how often proposals are valid, how lines read.
- Small models are weak at long consistency and structured output: briefs are short, answers are
  schema-forced, and the simulation holds all memory.
- **Platforms:** a living-town game is **PC only for now**. A model of this size does not fit the
  browser build and is heavy on phones; web and Android exports stay available for projects
  without the plugin. Revisit when a game needs it.
- New dependency and the model's license go through the usual review; the model file and its
  notice are checked by `UmbraTool package-check`.

## Phases (after S9)

| Phase | What | Example |
|---|---|---|
| **S21 World tick & residents** | A deterministic world clock (seeded, fixed step, integer math in the coarse tick so PC builds agree); residents as RPG characters with needs, schedules, relationships and growth; residents act when the player is elsewhere; world state in saves; headless fast-forward | Seven residents live a week with nobody watching: the same seed gives the same week |
| **S22 Memory, chronicle & rumours** | An event log; each resident remembers what they saw or heard; rumours travel; chronicle screen; plain template wording (used by tests and as the per-line fallback) | The player skips a favour; two days later another resident mentions it |
| **S23 Built-in model** | The model plugin (llama.cpp, worker thread, bounded queue, deadline); briefs and schema-forced answers; proposals through the validator; stored output; a scripted stand-in model for tests; the model pack in packaging | Residents speak in their own voice about real events; a resident picks an unexpected but legal action |
| **S24 Storyteller & outcome plan** | The outcome plan file and its validation; the storyteller (filter, pressure, forced beats); warnings; editor view of beats and outcomes; the soak test and its report | One story beat reached three different ways across seeds; a forbidden outcome never happens in 1,000 seeds |
| **S25 The town** | The example game slice: one town, seven residents, a short main plot with managed variation, played start to finish | Two playthroughs that differ and both end the main plot |

**Order:** S8 (done), S9, **S21-S25**, then S11, S19, S20, S12-S17. **S10 (RPG systems) and S18
(party) are re-scoped**: what the town needs from them (money and trade between residents,
dialogue choices with consequences, a resident accompanying the player) is built inside S21-S24;
the rest waits until a game needs it.

## Consequences

- The game gains a required run-time dependency (the model) and a larger download.
- "Verified" for these phases means: deterministic simulation tests, stand-in model tests, the soak
  report, and the user's play test with the real model on Windows. A cloud run cannot judge how the
  model's lines read.
- Risk: a world that is alive but dull. Each phase is judged on whether a player notices and
  cares, not on the number of agents.

## Open questions (decide when the phase starts)

1. S23: which model and size; whether one fine-tuned model is worth it later.
2. S25: the theme and whether it is a new project or the reference project.
3. Languages the model must speak (the game's text is English today).
4. Content limits for model-written lines (a filter before a line reaches the screen).
