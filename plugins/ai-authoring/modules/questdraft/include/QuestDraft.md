# Quest drafts (`QuestDraft.hpp`)

How an AI-written quest becomes content (roadmap P8B.2–P8B.3). This part has no network code:
the model is reached through `Umbra::LocalAi` (`engine/localai/include/LocalAi.md`), and drafts
can also come from a file (`UmbraTool draft --from`).

## The draft

The model fills `DraftSchema()`. It is deliberately simpler than the content format, so a small
model gets it right and can't reach outside its scope:

```json
{ "quest":  { "name", "description", "objectives": [ {"kind": "defeat|obtain|talk", "target", "count", "text"} ],
              "reward": { "xp", "skillPoints", "items": [ {"item", "count"} ] } },
  "offer":  "the player's line that opens the quest",
  "start":  "first node id",
  "nodes":  [ { "id", "speaker", "text",
                "choices": [ { "text", "next",
                               "requires": [ {"kind": "questNotStarted|questActive|questCompleted|hasItem", "item", "count"} ],
                               "actions":  [ {"kind": "startQuest|talk|give|take", "item", "count"} ] } ] } ] }
```

`startQuest`, `talk` and the quest conditions always mean *this* quest and *this* NPC, so a draft
cannot start or test other quests.

## PlanDraft, step by step

1. **Read the draft.** Every field is checked for type and size (`draft.field`, `draft.limit`,
   `draft.kind`, `draft.next`, `draft.node.duplicate`). Nothing is cut silently; text is only
   ever stored as text (roadmap T42). Limits: 5 objectives, 20 nodes, 4 choices per node,
   300 characters per line, 1000 XP, 3 skill points, 3 reward items.
2. **Make ids.** The quest id comes from its name (`"Wolves at the Well"` → `wolves_at_the_well`,
   `_2` if taken). Node ids get the quest id as prefix, so they never clash with existing nodes.
3. **Build additive changes.** One `Insert` of the quest; then the nodes go into the NPC's
   conversation (the one a scene uses, if there are several) plus one new choice, the offer, in
   its start node, before a final "Goodbye". If the NPC has no conversation, a new one is made
   (with a warning: attach it to the NPC in a scene).
4. **Scope check** (`CheckDraftScope`, roadmap T21): only `Insert` (or `Set` of something that
   doesn't exist yet) at `/quests/-`, `/conversations/-`, `/conversations/N/nodes/-` or a
   choice position. Changing or removing anything is refused.
5. **Same validation as a manual edit**: the game's own loader (`RpgValidator`) on the result.
6. **Play-through** (roadmap T20): a valid file can still be an unfinishable quest. The real
   `game::GameSession` plays it from a new game:
   - Search the NPC's conversation (breadth-first over the player's choices, up to 8 deep) for a
     path that starts the quest.
   - For each objective in order: `defeat` → win that many fights (with loot, exactly like
     `GameSession::CheckFightEnd`); `obtain` → already held, or loot from an enemy that carries
     it, or a conversation path that gives it; `talk` → a path in that NPC's conversation that
     counts as reporting back.
   - The quest must end **completed** with its reward paid.

   Every attempt replays the steps found so far from a fresh game, so no state is shared. Fights
   are assumed won: difficulty is not this check's job. Enemies or conversations that no scene
   uses yet are warnings (the creator may add them), not errors.

Errors block approval; warnings don't. The preview (`DraftPlan::preview`) and play-through lines
(`DraftPlan::play`) are what the creator reads before approving.

## Approval and undo

Applying is an ordinary `Document::Apply(changes, label, …, expectedRevision)`: all or nothing,
one undo step named `AI draft: <name>`. `expectedRevision` is the revision the plan was checked
against, so a manual edit made while reading the preview turns the approval into a conflict
(`edit.conflict`, roadmap T48) instead of overwriting it. The editor then offers "Check again".
