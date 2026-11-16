# The Last Curtain: what makes a stronger hook — research notes

2026-11-17. Asked by the user (item 10 of their 2026-11-17 list): "survive until the end or kill all
other guests" is not a strong enough hook; players have little reason to explore or level up, as
items and levels give almost no edge against other guests. Research party games and games with AI
players (such as Mario Party) for their fun, hook and level design. This note is input for the
decision; nothing here is decided. The user chooses the direction.

## What the current game measures (from the soak, `docs/testing/s38`)

- The only goal is to outlive everyone. Survival and winning are the same number, so the safest play
  is often to do nothing risky.
- Kind guests (who fight least) win 2-3% of games; dirty ones about 28%. Aggression pays, exploring
  does not show up in the result.
- Quests give a power and a stat, but a power is one more way to hurt others; it does not change *how*
  you can win.
- Rooms, items and levels feed one fight at a time; nothing accumulates into a visible lead.

## What the reference games do

**Mario Party.** Winning is counted in Stars, a currency separate from survival; coins buy Stars and
break ties. Coins come from many places (spaces, minigames, events), and Stars move between players
(Boo steals them). Bonus Stars at the end reward things done along the way (most minigames won, most
event spaces), so a player who played differently can still win. Luck is deliberate: hidden blocks,
Bowser spaces, pity items for those behind keep the underdog hopeful and the leader insecure. The
"last five turns" event shows the standings and gives the last-place player a roulette that changes
the rules for the endgame. Reviewers criticised Super Mario Party for making Stars too cheap: when the
score is easy, the board stops mattering.

**Pummel Party** (Mario Party with weapons, bots built in). Keys buy chalices (the score). Damage makes
a player drop keys others can pick up, and a death drops many: hurting someone is a way to score,
not only to remove them. Items on the board sabotage others (punch, shoot, steal keys).

**Armello** (digital board game, AI rivals, day and night). Several ways to win: kill the dying king,
bring four spirit stones, out-corrupt him, or hold the most prestige when he dies. Prestige comes from
quests and from beating other players. Reviews: the prestige route is the easiest against the AI, and
AI rivals sometimes throw themselves at the king and hand others the win; players wanted faster AI
turns and adjustable AI aggression.

**Slay the Spire.** Its designer calls risk against reward the core of the design, repeated at every
scale: the map offers safer or richer paths; elites hurt now but give a relic. The map turns random
encounters into chosen sequences of risks.

**Extraction games** (Escape from Tarkov, ARC Raiders, Dark and Darker). You go in, loot, and must get
out alive with it; dying loses what you carried. The loop is greed against survival: you hear
something move and decide whether staying for more loot will get you killed.

**Lethal Company.** A rising quota forces risk: being behind pushes the crew to riskier moons; safety
says stay together, the systems say split up.

**Dead by Daylight.** The hunted have a job, not just survival: repair five generators to power the
exit gates. The hunter must patrol those objectives, so the chase happens around goals.

**Battle royales.** Everyone starts with nothing; loot is power; the shrinking zone forces fights.

**Into the Breach.** Every enemy action is shown before the player moves. Losses feel earned; the
enemies feel like actors with intentions.

**Dicey Dungeons.** Pure dice frustrate; each class gets a way to shape its luck (the Warrior rerolls
up to three times a turn). Rerolls are the simplest such tool.

**Kingmaking.** In elimination games with three or more players, a player who cannot win can decide
who does, and players dislike it. Known remedies: hidden scores, less targeting of one player, and
player elimination. Our ghosts are players who cannot win but can still kill: a kingmaking risk.

## Principles for The Last Curtain

1. **A score separate from survival.** Something to collect that exploring, quests, fights and
   bravery all add to, so "outlive everyone" is not the only lever (Mario Party, Pummel Party,
   Armello prestige).
2. **Killing should pay, not only remove** (Pummel Party key drops), so fights are a choice with a
   prize, and a strong guest is also a rich target.
3. **Risk against reward on the map** (Slay the Spire, extraction games): richer rooms are more
   dangerous; carrying your prize makes you a target.
4. **Several ways to win**, so a kind guest has a route (Armello).
5. **Hope for whoever is behind** (Mario Party's pity items and endgame roulette), within limits.
6. **Show the bots' intentions** (Into the Breach), so the player can plan around them; it also makes
   persuasion legible.
7. **Give the dice a handle** (Dicey Dungeons): rerolls as a resource (the user's item 5).
8. **Keep ghosts from kingmaking**: give them their own goal rather than "kill anyone".

## Three directions to choose from

**A. Applause (a Mario Party-style score).** Guests collect Applause: first into a room, events passed,
quests, monsters slain, rounds survived. A death drops half of it where you fell. At dawn of the last
night the survivors leave; the most Applause among them wins, with "standing ovation" bonuses revealed
at the end (most rooms opened, most chats won...). Strength: every action scores; easy to balance by
numbers. Risk: it is a different game from "last one standing", the user's original battle royale.

**B. The Stage Door (a Dead by Daylight / extraction goal).** The house has one way out, the Stage Door,
shut by three locks. Keys to the locks lie in the most dangerous rooms, come from quests, or are held
by monsters. Whoever carries three keys to the door escapes and wins at once; a carried key drops on
death, so a key-holder becomes everyone's target. The killer patrols near the keys. Last one alive
still wins too. Strength: keeps the battle royale and the Opera House fantasy, gives exploring, levels
(to win key fights) and persuasion ("give me your key") a clear purpose; ends nights decisively.
Risk: the door race needs balancing against the killer.

**C. Takings (an extraction greed loop).** Rooms hold valuables; you score only what you carry out at
dawn through an exit. Dying loses what you carried; others can take it. Several nights add up; the
richest survivor at the end wins. Strength: pure push-your-luck tension. Risk: least like the current
rules; needs the most new content.

**My recommendation: B**, with three of the principles that suit any direction: rerolls as a
resource (7), the bots' next intent shown (6), and ghosts with their own goal (8). B gives each
existing system a job (quests can hand out keys, items and levels win key fights, chats can bargain
for keys) instead of adding a second game on top.

## Sources

- Mario Party series, Super Mario Wiki: https://www.mariowiki.com/Mario_Party_(series)
- Last Five Turns Event, Super Mario Wiki: https://mariowiki.com/Last_5_Turns
- 5 Lessons from Mario Party for Board Game Developers: https://brandonthegamedev.com/5-lessons-from-mario-party-for-board-game-developers/
- Super Mario Party review, USgamer: https://www.usgamer.net/articles/super-mario-party-review
- Pummel Party review, Gideon's Gaming: https://gideonsgaming.com/pummel-party-review-my-kind-of-party
- Pummel Party items guide: https://gameplay.tips/guides/9298-pummel-party.html
- Armello review, GameSpot: https://www.gamespot.com/reviews/armello-review-nintendo-switch-update/1900-6416262/
- Armello review, Gamingtrend: https://gamingtrend.com/reviews/armello-review/
- Slay the Spire design tensions: https://saschb2b.github.io/game-mechanics/games/slay-the-spire/design-tensions
- What is an extraction shooter, Corsair: https://www.corsair.com/ca/en/explorer/glossary/what-is-an-extraction-shooter/
- Best extraction shooters 2026, GameGeeker: https://gamegeeker.com/editors-picks/best-extraction-shooter-games-2026?market=US
- Lethal Company, PC Gamer: https://pcgamer.com/your-new-co-op-survival-horror-obsession-is-about-being-underpaid-and-overworked-space-scavengers
- Lethal Company guide: https://earlyguides.pages.dev/lethal-company-guide
- Dead by Daylight, Wikipedia: https://wikipedia.com/wiki/Dead_by_Daylight
- What are battle royale games, Yudiz: https://www.yudiz.com/insights/what-are-battle-royale-games/
- Into the Breach: https://games.gg/into-the-breach/
- Dicey Dungeons, Game Developer: https://gamedeveloper.com/business/road-to-the-igf-cavanagh-houston-dobbe-s-i-dicey-dungeons-i-
- Dicey Dungeons review, Gamingtrend: https://gamingtrend.com/feature/reviews/of-dice-and-decks-dicey-dungeons-review
- Kingmaker scenario, Wikipedia: https://en.wikipedia.org/wiki/Kingmaker_scenario
- Is kingmaking cursed?, Skeleton Code Machine: https://www.skeletoncodemachine.com/p/is-kingmaking-cursed
