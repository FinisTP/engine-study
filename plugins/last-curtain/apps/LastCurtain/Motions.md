# Sprite motions

Plan D (`docs/design/2026-11-17-performances.md`, "Art and motion"): guests and specters are flat
pictures, and every action is a short motion of the picture, not an animation clip. The motions are
data, `assets/motions.json`; `tools/art/preview_motions.py` renders them all to a video to judge them.

## A motion

A motion has a length in seconds, may loop, and has **keys** at times `t`. A key sets any of:

| Field | Meaning |
|---|---|
| `x`, `y` | Offset in sprite heights; `+x` toward where the sprite faces, `+y` up |
| `sx`, `sy` | Scale about the feet |
| `rot` | Lean in degrees; `+` leans forward |
| `skew` | Shear; `+` moves the top forward |
| `alpha` | Opacity |
| `tint` | `[r, g, b, amount]`: the colour mixed in |
| `shake` | Sideways jitter, in sprite heights |
| `dissolve` | 0 to 1: how much is burned away |

A field keeps its last value until a later key changes it, easing between the two keys with
smoothstep: `a = u * u * (3 - 2u)`, where `u` runs 0 to 1 between the keys.

`faces` switches the picture at times (`[t, "angry"]`): calm, angry, neardead, happy, scheming,
afraid or talk. `flip` turns the sprite around first. `trail` draws three faded copies just behind it.

## The transform

A picture pixel `(u, v)` (picture `w` x `h`, `v` down) reaches the screen in five steps:

1. Feet-local, with the feet at the origin and `y` up: `p = (u - w/2, h - v)`.
2. Scale: `p = (sx * p.x, sy * p.y)`.
3. Skew: `p.x += skew * p.y`.
4. Lean by angle `r = rot` in radians: `p = (p.x cos r + p.y sin r, -p.x sin r + p.y cos r)`.
   A forward lean moves the top toward the facing side.
5. Screen, with facing `f` = +1 (right) or -1 (left) and sprite height `S` in pixels:
   `X = feetX + f * (p.x + x * S) + shake * S * sin(70 t)`, `Y = floorY - (p.y + y * S)`.

Rendering the inverse of this map fills the sprite's screen area, so nothing tears. In the engine
the same matrix places a quad; the tint, alpha and dissolve go to its shader.

## Which moment plays which motion

| Game moment | Guest | Other |
|---|---|---|
| Waiting | idle | |
| Moving (Move) | walk | |
| Takes an item (PickUp) | pickup | |
| Leaves an item (DropItem) | drop | |
| Uses an item; shows a prop in the duel | use | |
| An event's roll is coming (Check) | brace | |
| The roll passes / fails | pass / fail | |
| Asks, answers, chats (Chat, OpenChat, Answer) | talk | the other: talk |
| A request granted / refused (Persuade) | nod / refuse | |
| A bot asks someone to hunt | scheme | |
| A blow (Attack, Strike; Strike in the duel) | attack | the target: hit, 0.42 s later |
| Guarding (Guard; Hold in the duel) | guard | |
| A trap or a bad event (Effect below 0) | hit | |
| A blow that misses | dodge | |
| Leaves a fight; runs from the killer (Leave) | flee | |
| A rare prop's power (Power) | power | |
| A level gained (LevelUp) | levelup | |
| The killer is near | fear | |
| HP runs out on the board (Down) | down | |
| Killed (Died) | die, then ghost | |
| Walking as a ghost | ghost | |
| Back to life at the Dressing room (Woke) | revive | |
| A duel ended standing; the winner at dawn | bow | |
| HP runs out in the duel | booed | |
| The specter waits | | specter: haunt |
| The specter shows a heavy / light blow / taunt is next | | specter: loom (snarl) / twitch (head cracks aside) / taunt (grin) |
| The specter strikes | the guest: hit, guard or dodge | specter: lunge |
| The specter is struck | | specter: hit |
| The specter speaks | | specter: talk |
| The specter is beaten (the finale) | bow | specter: vanish |

The specter (`art/sprites/specter`) has the same seven face names, read for its mask: calm is the still
mask, angry a snarl, happy a stretched grin, scheming a crooked tilt, afraid a scream, neardead its
head sunk into its shoulders, talk its mouth open. On the stage it is drawn about 1.4 times a guest's
height, so it towers over them; its haunt leans toward its prey and its loom rears up taller.

## The shader

`plugins/last-curtain/assets/shaders/sprite.surface.hlsl` draws a sprite's quad. A draw item has only a
colour and an emissive to give it, so `Table::Sprite` packs them:

- colour rgb = the tint colour, colour a = the opacity;
- emissive = `floor(dissolve * 99) + amount * 0.99`: the shader reads `burn = floor(e) / 99` and
  `amount = frac(e) / 0.99`.

The shader cuts out pixels below half alpha, drops a fixed scatter of pixels for opacity below 1 (no
sorting needed), burns away pixels whose noise is below `burn` with an ember edge, mixes the picture
toward the tint, and lights it as a card facing the camera.

A sprite facing left uses a mirrored quad (u = 1 - u). Mirroring the matrix instead would reverse the
winding, and the culled back face would vanish; `SpriteModel` mirrors the motion on both sides
instead, so its determinant stays positive.

With no tint of its own a sprite takes 18% of its guest's colour, so the guests can be told apart
while they all share the template picture.
