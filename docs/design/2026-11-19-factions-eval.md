# The Last Curtain: faction matches — evaluation

2026-11-19. An evaluation of `2026-11-19-factions.md`, using the game-trend evaluation method (core
loop, live comparables, gates, a 7-criteria score) adapted to a PC single-player game with no store
or ad plan, as for the performances (`2026-11-17-performances-eval.md`). No creator profile file
exists; the profile is taken from the project: a solo developer, a working C++ engine, a soak tool,
2D sprites made with ComfyUI, no art or 3D budget.

## Verdict: PIVOT in scope, not in concept

The concept is sound and has a real gap (no offline, single-player turn-based MOBA). But as written
it is the largest design this project has had: about twelve systems at once. Build a **3v3 core
slice first** (below) and prove the loop with the soak before adding the rest.

## Core loop

Each turn, for each of your three characters: move toward a monster, a carrier or the stage → act
(battle, board ability, Perform) → end turn. Monsters give essence; essence carried to the stage and
performed becomes score; killing a carrier takes its essence. Most score after 30 turns wins. You
fall behind by being killed while carrying, losing your guardian, or never reaching the stage.

## Scorecard

Monetization left out (no store plan). Six criteria out of 30; the method's GO line (24/35) is about
21/30.

| Criterion | Score | Why |
|---|---|---|
| Simplicity | 2 | Factions, speed order with skips, move-then-act, two ability kinds, five jobs with level choices, 3-round battles, a mirrored lane generator, guardians, a moving stage, monster behaviours, item classes, events, and a faction AI with roles. |
| Content cost | 3 | The board is generated and checked by mirror, and all rules are data. But jobs, battle abilities and board abilities are hand-designed, and each needs balancing against the rest. |
| Shareability | 3 | A carrier shot down a step from the stage, and the essence changing hands, make good clips. Single-player limits spread. |
| Competition gap | 4 | The turn-based MOBAs found are online PvP, unreleased or gone (see Comparables). Guards of Atlantis II shows the appetite on the tabletop. No offline single-player one was found. |
| Timing | 3 | MOBAs are mature, not rising. Pokémon Unite closes on 23 March 2027, which leaves its players looking, but this game is not built for that window. |
| Creator fit | 4 | The engine, board, battles, items, events, saves, bots, soak and sprites carry over. The weak spot is the faction AI: the whole game is only as good as the bot side. |
| **Total** | **19/30** | No score of 1. Gates pass: no third-party IP (Unite is a reference, nothing is taken from it), no backend, no multiplayer, 2D art already made. |

**Distance to a playable 3v3 slice:** about 15-20 working sessions (generator 3-4, factions and
order 2, move/undo/end turn 2, stage and Perform 2, battle abilities and board abilities 3-4,
guardians and respawn 1, monster behaviours 2, faction AI 3-4, soak 1-2).
**Reusable parts:** `Steps` and the board view, the three-round battle, items and events (to
reshape), `nightbots` and the soak, saves, sprites and motions, the art pipeline.

**Scored again for the 3v3 slice** (Simplicity 3, Content cost 4): **21/30**, at the GO line.

## Comparables

| Title | Who, when | State | Model | Complaints or limits found |
|---|---|---|---|---|
| Pokémon Unite | The Pokémon Company / Tencent, 2021 | 1.5-2.5M daily players on Google Play (estimate); declining; closing 23 March 2027 | Free to play | Player mood listed as frustrated; monetization criticised by reviewers |
| Guards of Atlantis II | Wolff Designa, 2023 (tabletop) | BGG 8.6 at launch; record crowdfunding | Boxed game | Best at 4-10 players; about 2 hours a game; zero luck |
| GANK | Dicephaly Studios (2 people) | Coming soon on Steam, free demo | Premium | You control all 5 heroes, turn-based, with bots and a campaign; the developers say the many skills can overwhelm a first-timer |
| Curse of the Dragonbeast | — | Coming soon on Steam | — | Turn-based MOBA with roguelike parts; 20+ professions, 130+ items |
| Ashcrown | — | No longer on the Steam store | Cross-platform PvP | Turn-based champion PvP; gone from Steam |

GANK is the closest: it already has "you control the whole team, turn-based". The twist here is not
that; it is **carried essence and the stage**, the horror opera, and playing offline against bots.

## Gaps to exploit

1. **Online PvP is the norm**, and its weak point is player count (Ashcrown is gone; GoA II needs a
   full table). A complete offline game against bots has no such problem.
2. **Too many skills overwhelm newcomers** (GANK's own warning). The forecast and five Unite roles
   with few abilities each answer that directly.
3. **Long matches** (GoA II about two hours). A 3v3 match should fit in about 30-45 minutes of the
   player's time; the soak can measure it.

## What is missing

1. **Who picks the characters.** Nothing says how each side's three are chosen (a draft, a fixed
   pick, random). The mirror makes the board fair, but not the line-ups or the turn order: a faster
   team moves first every round. A mirrored pick (each side gets the same jobs) or a draft is needed.
2. **Two against one.** Battles are one against one. Ganging up (two on one, a support behind a
   fighter) is the heart of a MOBA team fight. As written, numbers only help one battle after
   another. Decide whether an ally next to a battle can help (a support ability, a joined blow), or
   say that it cannot on purpose.
3. **What a battle costs in time.** A three-round battle with a pick each round, on top of 3 × 30
   board turns, is a lot of input. The bot side's battles should resolve quickly on screen.
4. **Bot difficulty.** One bot faction is the only opponent; at least two levels are needed so a
   new player can win and a practised one is still tested.
5. **The first match.** The three things to learn are clear, but nothing introduces them; a short
   first match (1v1, fewer turns) would.

## What is irrelevant for now

- **1v1 to 5v5**: build 3v3 only; the other sizes come free later if the data allows them.
- **Many item classes and the turn items** (skip, push last): powerful, but not needed to prove the
  loop. Add them after the slice.
- **Lanes "good for ambushes"**: with the whole board visible (no fog of war), there are no ambushes,
  only choke points. Keep lanes for distance and routes, and drop the ambush promise or add fog later.

## What to focus on

1. **The carry-and-perform moment.** Reaching the stage with a pile of essence, a full round exposed
   before Perform, an enemy closing in: this is the game. Every other system should feed it.
2. **The faction AI.** It needs carriers that go home when they carry too much, defenders that stand
   between enemy carriers and the stage, and a guard when its hub is threatened.
3. **The forecast.** Every action, in battles too, shows its result or its dice range first.

## What to verify (the soak and one playtest)

- Each side wins 45-55% of mirrored matches; turn order (the faster team) moves win rate by no
  more than 5 points.
- No single score source brings more than 60% of the winners' score; matches are not decided by
  the guardian alone.
- The side behind after two-thirds of the turns still wins at least a quarter of the time.
- How many turns a match needs to be decided (to set the 30).
- The player's time per match, measured in a playtest: aim for under 45 minutes for 3v3.

## Kill switch

Stop and rethink the battle or the turn structure if, after tuning, any holds:
- a mirrored soak cannot get a side's win rate inside 40-60%;
- the bots never Perform under pressure (the loop collapses to a brawl);
- a 3v3 playtest match takes over an hour, or the user finds the waiting dull in two playtests.

## Assumptions and unknowns

- Player counts for Unite are third-party estimates; reviews of the turn-based MOBAs could not be
  read (none released with reviews).
- No creator profile file was found; the profile above comes from the repository.
- Timing barely matters for a game that is not trend-led.

## Sources

- Curse of the Dragonbeast: https://store.steampowered.com/app/4104270
- Ashcrown: https://store.steampowered.com/app/2080720
- GANK: https://store.steampowered.com/app/3579860 and the demo https://store.steampowered.com/app/4689220
- Guards of Atlantis II: https://frontlinegaming.org/2025/01/08/board-game-review-guards-of-atlantis-ii/,
  https://meeplesherald.com/crowdfunding/guards-of-atlantis-ii-tabletop-moba-gets-funded-in-10-minutes/,
  https://www.analog.games/gameItem/show/419cd83f96531da3019653eb2f770542/
- Pokémon Unite: https://activeplayer.io/?p=6463, https://marlvel.ai/api/llm/apps/games/pokemon-unite,
  https://frvr.com/blog/news/pokemon-unite-shut-down-date-nintendo-switch-mobile/,
  https://goombastomp.com/pokemon-unite-review/
