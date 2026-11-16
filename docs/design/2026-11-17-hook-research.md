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

## Direction A chosen; the hook, researched further (2026-11-17, later)

The user chose A, with their own rules: a score race; a killer takes half the victim's Applause; a
killed guest becomes a ghost where they fell and comes back by reaching a certain room or using a
certain item (so an accidental death does not end the evening); at dawn the most Applause wins.

**More references**

- *Spartacus: A Game of Blood and Treachery* (board game): gladiators who win in the arena gain
  Favor and their house gains Influence, the score. The host of the games chooses who fights and
  whether the loser lives: the crowd's attention is a resource to fight over.
- *Darwin Project* and *Upsilon Circuit*: battle royales framed as game shows where the audience
  drops items and helps decide who lives. In *The Hunger Games*, entertaining the audience earns
  sponsor gifts.
- *Fortnite* / *Apex Legends* respawns: an eliminated player drops a card; carrying it to a van
  brings them back, and using the van sets off an alarm others hear. The revive spot is a place on
  the map worth fighting over.
- *Splatoon* Turf War: the score is not kills but ground covered; a novice still scores. The losing
  side gets a "Danger" warning; comebacks are easy.
- *Incan Gold*: push your luck. You bank treasure only by leaving; those caught by a hazard lose
  what they carried. The game pulls you to stay and to leave at once.
- Catch-up design: a "headwind" (slowing the leader) is more elegant than a hand-out to the loser;
  League of Legends' designers say comebacks must be earned; Power Grid makes the leader act last.
  Several strong routes to victory also curb a runaway leader.

**Proposed hooks for A** (each is a lever; the first four are the core)

1. **The programme (audience requests).** Each round (or two) the house announces one to three
   acts the audience wants: "a death in the Orchestra pit", "someone walks out of the Wig room
   alive", "a broken truce", "the leader brought down". The first guest to deliver earns a burst of
   Applause. This is the game "pointing out how to earn": it pulls guests to the same rooms, so they
   collide. Our nine quests become standing acts; events and rooms supply the rest.
2. **Killing takes half** (the user's rule), and **the spotlight**: the leader is lit for everyone
   (seen even in the dark) and carries a bounty. A headwind on the leader, earned by the hunters.
3. **Applause is also currency** (Mario Party coins, Hunger Games sponsors): spend it on a reroll
   (the user's item 5), a gift from the house (an item), or a faster revive. Every spend lowers the
   score: a real choice between winning a fight now and winning the night.
4. **The encore at dawn** (Mario Party bonus stars): three of six ovations, revealed only at the end
   (most rooms opened, most chats won, the boldest roll passed, most kills, fewest deaths...), each
   worth a lump of Applause. Gives a kind or careful guest a route to win.
5. **The revive room** (the user's rule, done Fortnite-style): ghosts revive in the Dressing room
   (or with a rare item); the revive is heard across the house, so the room becomes a place to
   ambush and to defend.
6. **The audience gets bored**: a guest who earns nothing for a few turns starts to lose a little.
   Stops hiding in a corner.
7. **(Later) Bank by bowing** (Incan Gold): Applause earned stays "on you" until you take a bow on
   the stage; a killer takes half of what you carry. Strong tension, more to explain: a candidate
   for after the first playtest.

## One-rule hooks (2026-11-17, later still)

The user: the hooks above add too many concepts, and a room-based audience request is unfair (a bot
may already stand there). So: hooks that are one rule each, found in small, very successful games.

1. **The hot seat (King of Tokyo).** One central spot: whoever holds it scores every turn and hits
   everyone outside, but everyone outside hits only them, and they cannot heal there. You leave only
   when struck (the striker may take your place). Reviewers single out this one idea as what makes the
   game. For us: **the Stage**. Whoever stands on it earns Applause each round; everyone may strike
   them; the house's spotlight is on them. It is a fixed, known place, so it is fair (spawns can be
   equally far from it), and it gives every guest a standing reason to move and fight.
2. **Steal on knockout (Tetris 99).** A knockout takes all of the victim's badges, which make your
   attacks stronger; players hunt the badge leaders. The user's "killing takes half" is this rule.
   Watch for the snowball: Tetris 99 needs its survival phase to keep it in check.
3. **Simple rules, a competent opponent (Buckshot Roulette).** Shoot yourself or the dealer; a few
   items shift the odds. Reviewers praise how competent the AI dealer feels, never using items just
   because it has them; the developer credits simple rules that anyone can follow and that are fun
   to watch. For us: fewer concepts, and bots whose choices look smart.
4. **One deadly penalty, plain bluffing (Liar's Bar).** Viral because it is a simple idea and fun to
   watch; critics found it thin after a while. Simplicity spreads; depth keeps players.
5. **Hot potato.** A curse passed around until it goes off on whoever holds it. For us: a cursed prop
   (a mask) that doubles your Applause but marks you for the killer; pass it by touching a guest.
6. **Role reversal (Pac-Man's power pellet).** For a few moments the hunted hunts the hunters: fear,
   power, fear. For us: a rare item that makes the killer flee you for two rounds.
7. **Loot-and-escape with fragile loot (R.E.P.O.).** Valuables lose value when knocked about; the
   haul is shown on the cart. One idea that makes every carry tense.

**Recommendation:** Applause + killing takes half + **the Stage** (1). One place, one rule, no new
screens: it answers the balance worry (a known spot, not a lucky one) and points every guest at the
same room. 5 and 6 are good second steps if playtests show a quiet middle of the night.

## The user's seven concepts, weighed (2026-11-17, 03:00)

The user offered seven concepts (mixes allowed). Each is weighed against its closest existing games.

| # | Concept | Closest games and what happened | Fit with what exists | Verdict |
|---|---|---|---|---|
| 1 | Trapped performer; 3-5 performances a night; face a specter each time; the script's props earn extra applause | Monster Hunter: preparation and execution are two linked phases, the prep a calm contrast to the fights; Balatro shows the boss in advance, so it plays as a puzzle, not a surprise | Rooms, items, events and fights become the prep; the specter reuses the killer; needs the fight view (item 3), already wanted | **Strongest core**: it tells the player what to explore for and why |
| 2 | A survival show: one guest eliminated each hour unless they meet a hard request; race for one item | Squid Game: Unleashed: fast and fun but critics found little long-term depth (Metacritic 51); Fall Guys fell to a fraction of its peak within months | Elimination fights the user's revive rule; a single scarce item is good tension | Use the **scarce-prop race** as a layer, not hourly cuts |
| 3 | Always hunted; no playing safe | Dead by Daylight: the hunted have objectives, the hunter patrols them | The killer exists | Keep as the **pressure between performances** |
| 4 | You are the traitor; kill without being caught | AI Werewolf games exist (one on Steam with frontier models, Wolfcha); research finds LLM agents weak at deduction against rule-based players | A different genre (hidden roles, accusations); a 4B local model is a risky judge | **Shelve**; a possible later mode |
| 5 | Gather resources; after midnight all bots hunt you | 7 Days to Die's blood moon: a deadline that turns aimless looting into urgent preparation; prep, defence, recover | The midnight killer exists | Same rhythm as 1; **merge into 1** (each performance is the deadline) |
| 6 | Applause for exploring and hunting | (the earlier direction A) | Exists in part | Fine as the score, but gives no direction alone |
| 7 | The traitor-reveal board game formula | Its reviewers name the randomness that makes some scenarios unwinnable; we also avoid its text and art | We left it already | **Drop** |

**Recommended mix: 1 + 5 + a light 2 + 3, scored by 6.**

- *The programme.* A night has 3-5 performances (for example 21:00, 23:00, 01:00, 03:00, and a finale at
  dawn). The next act's script is shown one act ahead (Balatro): which specter, and the props it calls
  for ("Act II, the Duel: a blade and a mask").
- *Between acts* (5, 3): explore for the props; props are scarce (one or two of each), so rivals race
  and fight for them (2); the killer and monsters keep anyone from playing safe.
- *At curtain* (1): each guest on stage faces the specter in a turn-based duel (the fight view, item
  3): Applause for surviving and winning, more for each script prop used.
- *Score* (6): Applause; the most at dawn wins. Killing a rival still takes half; death is a ghost
  walk to the Dressing room (the user's rules).
- *The model* (our built-in AI): the specter's taunts and a critic's review after each act (flavour);
  later, act scripts proposed from authored pieces and checked by the rules; asking a rival for a prop
  in plain words.

Three concepts for the player: Applause, the script (what to bring), the performance (the duel).

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
- Spartacus: A Game of Blood and Treachery, Dice Tower: https://www.dicetower.com/board-game/128671
- Spartacus review, TechRaptor: https://techraptor.net/content/spartacus-board-game-review
- Darwin Project, Kotaku: https://kotaku.com/darwin-project-is-like-the-hunger-games-for-streaming-1823753340
- Upsilon Circuit: https://ambigamingcorner.com/2016/09/28/upsilon-circuit
- Hunger Games sponsorship points: https://survival-docs.pages.dev/eng/sponsorship-points
- Fortnite rebooting, Fortnite Wiki: https://fortnite.fandom.com/wiki/Rebooting
- Fortnite reboot vans, Game Informer: https://www.gameinformer.com/2019/04/05/fortnite-adding-respawn-vans-in-next-update
- Splatoon Turf War: https://splatoonwiki.org/wiki/Turf_War
- Splatoon design, CheatCC: https://www.cheatcc.com/?p=211551
- Incan Gold review, Critical Hits: https://critical-hits.com//2008/05/03/incan-gold-a-review
- Should games have a catch-up mechanic?, League of Gamemakers: https://leagueofgamemakers.com/ask-the-league-should-games-have-a-catch-up-mechanic
- Comeback mechanics, League of Legends: https://www.leagueoflegends.com/en-gb/news/dev/quick-gameplay-thoughts-2-25-comeback-mechanics/
- Positional balance, Games Precipice: https://www.gamesprecipice.com/positional-balance
- King of Tokyo review, Meeple Like Us: https://www.meeplelikeus.co.uk/king-of-tokyo-2011/
- King of Tokyo review, GMS Magazine: https://www.gmsmagazine.com/hail-to-the-king-baby-a-review-of-king-of-tokyo
- King of Tokyo, Dread Central: https://www.dreadcentral.com/news/52273/tabletop-terrors-slap-around-your-enemies-and-become-king-of-tokyo/
- Tetris 99 tips (badges), GamesRadar: https://www.gamesradar.com/tetris-99-tips
- Buckshot Roulette review, GamesHedge: https://www.gameshedge.com/?p=87567
- Buckshot Roulette developer interview, 80.lv: https://origin.80.lv/articles/buckshot-roulette-developer-on-making-the-game-solo-feedback-success
- Liar's Bar, PC Gamer: https://www.pcgamer.com/games/card-games/the-zootopia-russian-roulette-game-is-kind-of-a-bummer-to-play-but-its-fun-to-watch
- Liar's Bar explained: https://www.edinburghnews.scotsman.com/news/people/liars-bar-video-game-explained-4842922
- Hex Hex XL (hot potato): https://therewillbe.games/articles-boardgame-reviews/2160-spooky-trash-o-ween-game-reviews-hex-hex-xl
- Pac-Man's power pellet: https://dinogame.gg/blog/how-pacman-changed-arcade-design/
- R.E.P.O. guide: https://bloxodes.com/wiki/repo
- Why people love Monster Hunter, PC Gamer: https://www.pcgamer.com/why-people-love-monster-hunter-so-damn-much
- Balatro boss blinds: https://saschb2b.github.io/game-mechanics/games/balatro/blinds-and-bosses
- Balatro critical play: https://mechanicsofmagic.com/2025/05/23/critical-play-addiction-by-design
- Fall Guys decline, eXputer: https://exputer.com/?p=61152
- Squid Game: Unleashed, Metacritic: https://legacy.prod.metacritic.com/game/ios/squid-game-unleashed/critic-reviews
- 7 Days to Die blood moon: https://xgamingserver.com/blog/?p=20035
- Werewolf with AI models, Steam: https://store.steampowered.com/app/4586780
- LLM agents in social deduction: https://aitopics.org/tag/evil player
- Suck Up!: https://community.openai.com/t/vampire-game-where-you-convince-llm-to-let-you-in/604295
- Traitor board game randomness: https://2d4chan.org/wiki/Betrayal
