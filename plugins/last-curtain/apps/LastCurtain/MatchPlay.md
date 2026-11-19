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
