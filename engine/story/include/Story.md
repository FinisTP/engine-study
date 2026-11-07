# Story: conversations and quests (`Umbra::Story`)

Definitions live in the same `umbra.rpg` content file as skills and items (sections
`conversations` and `quests`, parsed by `Umbra::Rpg`). This module holds the changing state
(`QuestLog`) and runs a conversation (`Conversation`). No world, physics or renderer.

## Conversation format

```json
{ "id": "elder_mara", "npc": "elder", "start": "hello", "nodes": [
  { "id": "hello", "speaker": "Elder Mara", "text": "Traveler...", "choices": [
    { "text": "I'll get it back.", "next": "accepted",
      "if": [ { "quest": "sun_relic", "state": "notStarted" } ],
      "do": [ { "startQuest": "sun_relic" } ] },
    { "text": "Goodbye.", "next": "" } ] },
  { "id": "accepted", "speaker": "Elder Mara", "text": "Come back alive." } ] }
```

| Part | Meaning |
|---|---|
| `if` | All must hold for the choice to be shown: `{"quest": id, "state": notStarted/active/completed/failed}`, `{"hasItem": id, "count": n}` |
| `do` | Done in order when picked: `{"startQuest": id}`, `{"talk": npc}` (counts for Talk objectives), `{"give": item, "count": n}`, `{"take": item, "count": n}` |
| `next` | The node to show next; empty or missing ends the conversation. A node with no choices ends after its line |

Loading rejects a missing start node, a `next` that names no node, unknown quests/items and a
conversation without `npc` (codes `conversation.start`, `conversation.next`, `ref.quest`, `ref.item`,
`conversation.npc`).

## Running a conversation

- `VisibleChoices` lists the current node's choices whose conditions hold.
- `Choose` applies **all or none** of the choice's actions: they run on copies of the quest log and the
  hero, and only a fully successful choice replaces the originals. An npc giving an item that doesn't fit
  (`NoRoom`, matching the inventory decision) or taking one the player lacks (`MissingItem`) changes nothing
  and stays on the same node.
- `Cancel` (the player walks away, a fight starts, the area changes) ends it with no effect.
- Talking again starts from the start node; conditions on quest state pick the right lines, so a
  quest can't be started twice.

## Quests

```json
{ "id": "sun_relic", "name": "The Stolen Relic",
  "objectives": [ { "defeat": "bandit_leader" }, { "obtain": "sun_relic" }, { "talk": "elder" } ],
  "failOn": [ "death" ], "reward": { "xp": 120, "skillPoints": 1, "items": [ { "item": "iron_sword" } ] } }
```

Objectives are done in order; progress rules, failure and rewards are described in the P6.2 section below.
