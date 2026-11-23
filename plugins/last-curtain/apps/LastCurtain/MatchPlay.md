# MatchPlay: the faction match on screen

`MatchPlay` is the match as the person plays it, without a window, so it is tested. The rules
(`modules/match`) decide everything at once, the monsters' phase included; `MatchPlay` tells it one
beat at a time and holds the bots while it does. It never decides a rule: every question goes to
`Check`, `Apply`, `Preview` or `ChooseAction`.

## Beats

The rules write a log (`MatchState::log`). `MatchPlay` keeps two marks in it: what is queued (the
rules have written it) and what is shown. Each `Update` past the wait reveals the next queued entry
as a caption; the wait is `plainBeatSeconds` after a step (`Moved`) and `beatSeconds` after anything
else (`assets/options.csv`). One unit's consecutive steps share one line. When nothing is queued and
the acting character is a bot's, the bot takes one action (`ChooseAction`) and its first line shows.

The player's side is faction 0. `PlayerTurn` is true when everything is told, the round's banner has
played, and `Current` is one of the player's characters. `Do` refuses anything else. What the player
does shows at once while the turn stays theirs; once it passes (End turn, a battle that ends inside
the monsters' phase), it plays out in beats like a bot's.

`Skip` (the `skip` action on the main screen) tells everything at once and runs the bots to the
player's next turn.

## Pausing for a scene (campaign plan 4)

`Pause(true)` stops `Update`: no beat, no bot, no banner. `Quiet()` is true between told beats: every
log line shown, no battle on the table, no banner. `Told()` is how many log lines were shown; the
campaign's `StageMoments` reads only those, so a scene never runs ahead of what the person saw.

## The player's turn (the prototype's flow)

1. **Move.** `reach` lists every square the acting character can walk to (breadth first, `Check`
   asked for each step as if it stood there); `path` is the walk to the hovered square. A click on a
   lit square walks there one legal step at a time; a click on the character stays.
2. **Act.** `actions`: Undo move (Back before moving, which shows the range again), Attack, each
   board ability (its HP cost, its target symbol), each item, Pick up, Look into the room, Perform
   (on the stage only), End turn. Each is `{id, label, note, enabled, why}`: enabled only when
   `Check` says Ok for some target; `why` is the reason in words (`match-words.json` `why`;
   `NoTarget` when nothing valid is in reach).
3. **Target.** Attack or an ability arms it: `targets` lists the squares of every valid target,
   `reach` the ability's range (one step for Attack), and the panel adds the character's Attack and
   Defence. A click on a target acts; anywhere else disarms.

`forecast` follows the hovered square: an armed target (its numbers and the first blow, or the
ability's `Preview`), the stage, an announced spawn (what, its level, its round), any unit's numbers,
else the square's kind (`tiles`). The attack's figure is the display's own estimate of the first
blow with the free strike; the battle itself is picked blow by blow.

## The banner

When everything is told and the round has changed since the last banner, the banner shows "Turn N"
and "M turns left" (`match-words.json` `banner`) for `turnBannerSeconds`; nothing moves meanwhile.

## The screen's data (`UiData`)

- `score`: both sides.
- `order`: the strip. From the acting character to the end of the round (`current` on the first),
  then the marker (`next turn`), then those who have acted this round (`acted`); `down` ones are
  dimmed. During the monsters' phase every character has acted.
- `unit`: the panel: the acting character (or the next one in order): name, sprite, side, role,
  level, HP, max HP, essence.
- `log`: every caption, newest last; `logOpen`.
- `banner`: its title and line.

## Words

`assets/match-words.json`: `log` has a line per `LogKind` (`{who}`, `{whom}`, `{n}`, `{monster}`,
`{item}`, `{result}`, `{side}`), `why` a reason per `Why` (the tooltips), `banner`, `roles`, `misc`.

## The other screens

`Screen()` gives, in this order: `end` (the match is over and told), `banner`, `event` (the player
looked into a room: its card is open), `level` (the acting character has a level choice), `battle`
(a battle is on and told), else `""`.

- `battle`: both fighters (numbers, level, the side's shield, sprite), `Round r of n`, the last
  blow's line, and on the player's pick each battle ability with its `Preview` (damage, healing,
  cost) or why not (`pick` with the slot). A bot's or monster's pick is one beat.
- `level`: the job row's two abilities (`pick` 0 or 1).
- `event`: the card's name, text and two choices with their chance (`choose` 0 or 1), then the
  result line; `close` returns to the board.
- `end`: the winner, the scores, kills per side (the units' counts), essence performed and monsters
  downed per side (from the log).

## Motions and sounds

Each told line brings its motions (`Moves`: a step walks, a blow is an attack and a hit, a fall holds
`down` until `revive` at the hub, a level `levelup`, a spawn `loom`, an event pass or fail, a pick-up)
and its sound (`Sound`: step, hit, death, pass, door, fail, item, bell; `turn` when a banner opens).
The window takes them once (`TakeMoves`, `TakeSounds`): the table plays the motion on that unit's
sprite (`motions.json`, the night's), `Sounds` plays the cue (`sounds.csv`).

## Spoken lines

A blow, a fall, a performance or a level puts a short line (`match-words.json` `lines`, by log
kind) in a bubble on its speaker's card, characters only, one bubble per speaker, for two beats. The
line is picked from the round, the speaker and the moment, so a replay says the same. The built-in
model can write them later. The same character on both sides is told apart: theirs is
"Name (theirs)" (`misc.theirs`); the order strip shows short names in the side's colour.

## Treasure

While the player's character holds dealt cards, `treasure` is the screen: the face-down cards
(`pick` by index), then the card turned (`result`, its log line), then `close`.

## The event's dice

The rules roll an event's percent chance. The screen shows two dice against the 2d6 total whose odds
(the ways of 36 to reach it or more) are nearest that chance (`DiceTarget`; ties go to the lower
total), tumbling for `diceSeconds` and landing on a total that agrees with what the rules decided:
the middle of the totals that pass, or that fail. Display only; nothing reads them.

## The battle view and focus

`UiData()["fight"]` is the battle on the table: attacker and defender, from the moment its `Battle`
line is told until its last blow or fall has been told and a beat (`beatSeconds`) has passed, so a
bot's battle that the rules finish at once is still seen. The battle screen stays up as long, with
picks only while the rules' battle runs. `UiData()["focus"]` is whom the last told line is about
(striker and struck, healer and healed, the one who levels, performs, takes or appears) for
`beatSeconds`; the table frames them. The player's own action that has someone to look at keeps
its beat before the bots go on.

## Told, not ahead (2026-11-20 evening)

A blow, a heal, an attack and a board ability are told beat by beat, the player's own too
(`blowSeconds` a blow): the cards' HP follow the told lines (`toldHp_`, `UiData()["hp"]`), and a
fighter the rules have downed stands (`standing`) until its fall is told. No round banner and no
bot's action while a finished fight is still on its stage, and the player cannot act then. Group
battles: `fight` is the four places, `picking` whose pick it is; the battle screen has `mine` and
`theirs`, and a strike is offered at each foe standing ("slot:target"). The hint line always says
what to do next; the order strip runs `orderLength` (12) portraits, each with a tip of its numbers
and a click to look at it; level cards say what an ability does (`AbilityText`).
